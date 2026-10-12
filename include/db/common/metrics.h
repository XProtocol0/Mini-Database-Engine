#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace db {

// A monotonically increasing counter, safe to bump from any thread.
class Counter {
 public:
  // Relaxed ordering: a counter is a statistic, not a synchronization point,
  // so it never needs to order other memory operations.
  void Increment(uint64_t delta = 1) { value_.fetch_add(delta, std::memory_order_relaxed); }
  uint64_t Value() const { return value_.load(std::memory_order_relaxed); }
  void Reset() { value_.store(0, std::memory_order_relaxed); }

 private:
  std::atomic<uint64_t> value_{0};
};

// A lock-free latency histogram with power-of-two buckets.
//
// Bucket i holds samples in [2^(i-1), 2^i) nanoseconds (bucket 0 holds 0).
// Recording is one relaxed atomic add, so it is cheap enough for hot paths.
// The price is precision: a percentile is reported as the upper bound of the
// bucket it falls in, i.e. it can be off by up to 2x. That is enough to tell
// a 10us lock wait from a 10ms one, which is what these metrics are for.
class Histogram {
 public:
  static constexpr size_t kNumBuckets = 64;

  void Record(uint64_t nanos);
  void Record(std::chrono::nanoseconds d) { Record(static_cast<uint64_t>(d.count())); }

  uint64_t Count() const { return count_.load(std::memory_order_relaxed); }
  uint64_t Sum() const { return sum_.load(std::memory_order_relaxed); }
  uint64_t Max() const { return max_.load(std::memory_order_relaxed); }

  // Approximate p-th percentile (p in [0, 100]) in nanoseconds. Returns 0 when
  // empty. Concurrent Record() calls may make the snapshot slightly stale.
  uint64_t Percentile(double p) const;

  void Reset();

  static size_t BucketFor(uint64_t nanos);
  static uint64_t BucketUpperBound(size_t bucket);

 private:
  std::array<std::atomic<uint64_t>, kNumBuckets> buckets_{};
  std::atomic<uint64_t> count_{0};
  std::atomic<uint64_t> sum_{0};
  std::atomic<uint64_t> max_{0};
};

// Process-wide registry of named metrics.
//
// Subsystems look a metric up once (typically into a member reference) and
// then update it lock-free; only the lookup takes the registry mutex. Metrics
// are never removed, so returned references stay valid for the process
// lifetime. Names are dotted paths such as "buffer.hits".
class MetricsRegistry {
 public:
  static MetricsRegistry& Instance();

  Counter& GetCounter(std::string_view name);
  Histogram& GetHistogram(std::string_view name);

  // Human-readable dump of every metric, sorted by name (for \metrics).
  std::string Dump() const;

  // Zero every metric. Intended for tests and benchmarks.
  void ResetAll();

 private:
  mutable std::mutex mu_;
  // std::less<> enables lookup by string_view without building a std::string.
  std::map<std::string, std::unique_ptr<Counter>, std::less<>> counters_;
  std::map<std::string, std::unique_ptr<Histogram>, std::less<>> histograms_;
};

// RAII timer: records the elapsed time into a histogram on destruction.
class ScopedTimer {
 public:
  explicit ScopedTimer(Histogram& h) : hist_(h), start_(std::chrono::steady_clock::now()) {}
  ~ScopedTimer() { hist_.Record(std::chrono::steady_clock::now() - start_); }
  ScopedTimer(const ScopedTimer&) = delete;
  ScopedTimer& operator=(const ScopedTimer&) = delete;

 private:
  Histogram& hist_;
  std::chrono::steady_clock::time_point start_;
};

}  // namespace db
