// Micro-benchmarks for primitives that sit on hot paths. Run from a Release
// build; sanitizer builds distort timings by an order of magnitude.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>

#include "db/common/failpoint.h"
#include "db/common/metrics.h"

namespace {

// Keep the compiler from optimizing away a loop whose result is unused.
template <typename T>
void DoNotOptimize(const T& value) {
  asm volatile("" : : "r,m"(value) : "memory");
}

void Run(const char* name, uint64_t iters, const std::function<void(uint64_t)>& body) {
  auto start = std::chrono::steady_clock::now();
  body(iters);
  auto elapsed = std::chrono::steady_clock::now() - start;
  double ns = std::chrono::duration<double, std::nano>(elapsed).count();
  std::printf("%-32s %12llu iters %8.2f ns/op\n", name, static_cast<unsigned long long>(iters),
              ns / static_cast<double>(iters));
}

}  // namespace

int main() {
  constexpr uint64_t kIters = 50'000'000;
  auto& metrics = db::MetricsRegistry::Instance();

  Run("counter.increment", kIters, [&](uint64_t n) {
    db::Counter& c = metrics.GetCounter("bench.counter");
    for (uint64_t i = 0; i < n; ++i) c.Increment();
    DoNotOptimize(c.Value());
  });

  Run("histogram.record", kIters, [&](uint64_t n) {
    db::Histogram& h = metrics.GetHistogram("bench.histogram");
    for (uint64_t i = 0; i < n; ++i) h.Record(i & 0xFFFF);
    DoNotOptimize(h.Count());
  });

  Run("failpoint.unarmed", kIters, [](uint64_t n) {
    uint64_t fired = 0;
    for (uint64_t i = 0; i < n; ++i) fired += FAILPOINT("bench.unarmed") ? 1 : 0;
    DoNotOptimize(fired);
  });

  // Arming an unrelated point disables the fast path: every site now takes
  // the registry mutex. This bounds the cost during crash tests.
  db::FailpointRegistry::Instance().Arm("bench.other", db::FailpointAction::kReturnError);
  Run("failpoint.other_armed", kIters / 10, [](uint64_t n) {
    uint64_t fired = 0;
    for (uint64_t i = 0; i < n; ++i) fired += FAILPOINT("bench.unarmed") ? 1 : 0;
    DoNotOptimize(fired);
  });
  db::FailpointRegistry::Instance().DisarmAll();
  return 0;
}
