#include "db/common/metrics.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <format>

namespace db {

size_t Histogram::BucketFor(uint64_t nanos) {
  // bit_width(n) is the index of the highest set bit plus one, so values in
  // [2^(i-1), 2^i) land in bucket i. Values >= 2^63 share the last bucket.
  return std::min<size_t>(std::bit_width(nanos), kNumBuckets - 1);
}

uint64_t Histogram::BucketUpperBound(size_t bucket) {
  if (bucket == 0) return 0;
  if (bucket >= kNumBuckets - 1) return UINT64_MAX;
  return (uint64_t{1} << bucket) - 1;
}

void Histogram::Record(uint64_t nanos) {
  buckets_[BucketFor(nanos)].fetch_add(1, std::memory_order_relaxed);
  count_.fetch_add(1, std::memory_order_relaxed);
  sum_.fetch_add(nanos, std::memory_order_relaxed);
  uint64_t prev = max_.load(std::memory_order_relaxed);
  // Retry until we install `nanos` or observe a value that is already larger.
  while (prev < nanos && !max_.compare_exchange_weak(prev, nanos, std::memory_order_relaxed)) {
  }
}

uint64_t Histogram::Percentile(double p) const {
  uint64_t total = Count();
  if (total == 0) return 0;
  p = std::clamp(p, 0.0, 100.0);
  // Rank of the sample we want (1-based); at least the first sample.
  auto rank = std::max<uint64_t>(1, static_cast<uint64_t>(std::ceil(p / 100.0 * total)));
  uint64_t seen = 0;
  for (size_t i = 0; i < kNumBuckets; ++i) {
    seen += buckets_[i].load(std::memory_order_relaxed);
    if (seen >= rank) {
      // A bucket's upper bound can exceed every recorded sample; the observed
      // max is a tighter bound for the top buckets.
      return std::min(BucketUpperBound(i), Max());
    }
  }
  // Only reachable if buckets were read while Record() was mid-update.
  return Max();
}

void Histogram::Reset() {
  for (auto& b : buckets_) b.store(0, std::memory_order_relaxed);
  count_.store(0, std::memory_order_relaxed);
  sum_.store(0, std::memory_order_relaxed);
  max_.store(0, std::memory_order_relaxed);
}

MetricsRegistry& MetricsRegistry::Instance() {
  static MetricsRegistry registry;
  return registry;
}

Counter& MetricsRegistry::GetCounter(std::string_view name) {
  std::lock_guard lock(mu_);
  auto it = counters_.find(name);
  if (it == counters_.end()) {
    it = counters_.emplace(std::string(name), std::make_unique<Counter>()).first;
  }
  return *it->second;
}

Histogram& MetricsRegistry::GetHistogram(std::string_view name) {
  std::lock_guard lock(mu_);
  auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    it = histograms_.emplace(std::string(name), std::make_unique<Histogram>()).first;
  }
  return *it->second;
}

std::string MetricsRegistry::Dump() const {
  std::lock_guard lock(mu_);
  std::string out;
  for (const auto& [name, c] : counters_) {
    out += std::format("{} = {}\n", name, c->Value());
  }
  for (const auto& [name, h] : histograms_) {
    out += std::format("{}: count={} p50={}ns p95={}ns p99={}ns max={}ns\n", name, h->Count(),
                       h->Percentile(50), h->Percentile(95), h->Percentile(99), h->Max());
  }
  return out;
}

void MetricsRegistry::ResetAll() {
  std::lock_guard lock(mu_);
  for (auto& [_, c] : counters_) c->Reset();
  for (auto& [_, h] : histograms_) h->Reset();
}

}  // namespace db
