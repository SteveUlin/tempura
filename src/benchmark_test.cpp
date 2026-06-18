#include "benchmark.h"

#include <chrono>
#include <cstddef>

#include "unit.h"

using namespace tempura;
using namespace std::chrono_literals;

// Measurement-correctness oracle: these assert PROPERTIES the harness must satisfy
// for its numbers to mean anything, not absolute timings. If any fails, every
// GFLOP/s figure downstream (notably the LU eager-vs-lazy verdict) is fiction.
// Kept fast (short epoch + runtime, quiet) so it runs under ctest, not as a sweep.

// A workload whose cost is provably linear in k and cannot be elided: each add is
// observed, so the optimizer can neither vectorize it away nor hoist it.
auto linearWork(std::size_t k) {
  return [k] {
    double acc = 0.0;
    for (std::size_t i = 0; i < k; ++i) {
      acc += static_cast<double>(i);
      doNotOptimize(acc);
    }
    doNotOptimize(acc);
  };
}

auto timeOf(std::string_view, std::size_t k) -> double {
  auto r = "linear"_bench.warmup(50).minEpoch(200us).maxRuntime(80ms).quiet() = linearWork(k);
  return static_cast<double>(r.median_wall.count());
}

auto main() -> int {
  // 1. Linear cost scales linearly. Doubling the work must ~double the time; a
  //    linear workload that doesn't track means batching/timing is broken.
  "linear workload scales linearly"_test = [] {
    const double t1 = timeOf("k", 4096);
    const double t2 = timeOf("2k", 8192);
    const double t4 = timeOf("4k", 16384);
    expectGT(t2 / t1, 1.6);
    expectLT(t2 / t1, 2.4);
    expectGT(t4 / t2, 1.6);
    expectLT(t4 / t2, 2.4);
  };

  // 2. doNotOptimize actually works — THE critical test. A dropped result is
  //    eliminated (≈ loop overhead); the same result, observed, survives and costs
  //    real time. If the "kept" arm reads ~0, the optimizer is deleting work and
  //    every throughput number is invented.
  "doNotOptimize prevents dead-code elimination"_test = [] {
    constexpr std::size_t kWork = 512;
    auto dropped = "dropped"_bench.warmup(50).minEpoch(200us).maxRuntime(80ms).quiet() = [] {
      double a = 0.0;
      for (std::size_t i = 0; i < kWork; ++i) a += static_cast<double>(i) * 0.5;
      // a is never observed → the whole loop is dead code, removed at -O2
    };
    auto kept = "kept"_bench.warmup(50).minEpoch(200us).maxRuntime(80ms).quiet() = [] {
      double a = 0.0;
      for (std::size_t i = 0; i < kWork; ++i) a += static_cast<double>(i) * 0.5;
      doNotOptimize(a);  // observed → the loop must execute
    };
    expectGT(kept.median_wall.count(), 0L);
    expectGT(kept.median_wall.count(), dropped.median_wall.count() * 2);
  };

  // 3. Timer-resolution sanity: for a sub-µs op the harness must batch (never time
  //    one call against a coarse clock), and the calibrated epoch must dwarf clock
  //    granularity so timer overhead is <1% of the signal.
  "auto-batching keeps the epoch far above clock granularity"_test = [] {
    auto r = "tiny"_bench.minEpoch(1ms).maxRuntime(80ms).quiet() = [] {
      double a = 1.0;
      doNotOptimize(a);
      a = a * 1.5 + 0.5;
      doNotOptimize(a);
    };
    expectGT(r.batch, std::size_t{1});  // a sub-µs op forced real batching
    const double epoch_ns = r.median_wall.count() * static_cast<double>(r.batch);
    expectGT(epoch_ns, 500'000.0);  // batch epoch ≥ 0.5 ms ≫ any clock tick
  };

  // 4. A/B null: two identical workloads must measure ~equal. The comparison path
  //    the LU sweep relies on must not manufacture a spurious difference.
  "identical workloads measure within noise"_test = [] {
    const double a = timeOf("a", 8192);
    const double b = timeOf("b", 8192);
    const double rel = (a > b ? a - b : b - a) / (a < b ? a : b);
    expectLT(rel, 0.25);
  };

  return TestRegistry::result();
}
