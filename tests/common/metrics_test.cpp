#include "db/common/metrics.h"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

namespace db {
namespace {

TEST(CounterTest, ConcurrentIncrementsAreNotLost) {
  Counter c;
  constexpr int kThreads = 8;
  constexpr int kPerThread = 100'000;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < kPerThread; ++i) c.Increment();
    });
  }
  for (auto& t : threads) t.join();
  EXPECT_EQ(c.Value(), uint64_t{kThreads} * kPerThread);
}

TEST(HistogramTest, BucketBoundaries) {
  EXPECT_EQ(Histogram::BucketFor(0), 0u);
  EXPECT_EQ(Histogram::BucketFor(1), 1u);
  EXPECT_EQ(Histogram::BucketFor(2), 2u);
  EXPECT_EQ(Histogram::BucketFor(3), 2u);
  EXPECT_EQ(Histogram::BucketFor(4), 3u);
  EXPECT_EQ(Histogram::BucketFor(UINT64_MAX), Histogram::kNumBuckets - 1);
  // Every value must be <= the upper bound of its own bucket.
  for (uint64_t v : {0ull, 1ull, 7ull, 8ull, 1000ull, 1ull << 40}) {
    EXPECT_LE(v, Histogram::BucketUpperBound(Histogram::BucketFor(v))) << v;
  }
}

TEST(HistogramTest, EmptyReportsZero) {
  Histogram h;
  EXPECT_EQ(h.Count(), 0u);
  EXPECT_EQ(h.Percentile(50), 0u);
}

TEST(HistogramTest, PercentilesAreWithinOneBucket) {
  Histogram h;
  for (uint64_t v = 1; v <= 1000; ++v) h.Record(v);
  EXPECT_EQ(h.Count(), 1000u);
  EXPECT_EQ(h.Sum(), 500'500u);
  EXPECT_EQ(h.Max(), 1000u);
  // True p50 is 500; a power-of-two bucket can overstate it by up to 2x.
  uint64_t p50 = h.Percentile(50);
  EXPECT_GE(p50, 500u);
  EXPECT_LT(p50, 1000u);
  // The top percentile is clamped to the observed max.
  EXPECT_EQ(h.Percentile(100), 1000u);
}

TEST(HistogramTest, ConcurrentRecords) {
  Histogram h;
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&, t] {
      for (int i = 0; i < 10'000; ++i) h.Record(static_cast<uint64_t>(t * 10'000 + i));
    });
  }
  for (auto& t : threads) t.join();
  EXPECT_EQ(h.Count(), 40'000u);
  EXPECT_EQ(h.Max(), 39'999u);
}

TEST(MetricsRegistryTest, SameNameReturnsSameMetric) {
  auto& reg = MetricsRegistry::Instance();
  Counter& a = reg.GetCounter("test.registry.counter");
  Counter& b = reg.GetCounter("test.registry.counter");
  EXPECT_EQ(&a, &b);
  EXPECT_NE(&reg.GetHistogram("test.registry.h1"), &reg.GetHistogram("test.registry.h2"));
}

TEST(MetricsRegistryTest, DumpListsMetrics) {
  auto& reg = MetricsRegistry::Instance();
  reg.GetCounter("test.dump.hits").Increment(3);
  reg.GetHistogram("test.dump.latency").Record(100);
  std::string dump = reg.Dump();
  EXPECT_NE(dump.find("test.dump.hits = 3"), std::string::npos) << dump;
  EXPECT_NE(dump.find("test.dump.latency: count=1"), std::string::npos) << dump;
}

TEST(MetricsRegistryTest, ResetAllZeroes) {
  auto& reg = MetricsRegistry::Instance();
  Counter& c = reg.GetCounter("test.reset.c");
  c.Increment(5);
  reg.ResetAll();
  EXPECT_EQ(c.Value(), 0u);
}

TEST(ScopedTimerTest, RecordsOneSample) {
  Histogram h;
  {
    ScopedTimer t(h);
  }
  EXPECT_EQ(h.Count(), 1u);
}

}  // namespace
}  // namespace db
