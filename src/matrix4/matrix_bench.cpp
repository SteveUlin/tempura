#include "benchmark.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "matrix.h"
#include "mdarray.h"
#include "transpose.h"
#include "vec.h"

// Kernel sweeps that quantify the design's efficiency claims: matmul throughput, the
// per-pass tax of a strided transpose view vs materializing it, the cost of norm2's
// overflow-safe scaling, and of Accumulator's float→double widening.

using namespace tempura;
using namespace std::chrono_literals;

namespace {

template <typename T>
auto randomMat(std::size_t n, std::uint64_t seed) -> Dense<T, dyn, dyn> {
  std::mt19937_64 gen{seed};
  std::uniform_real_distribution<double> dist{-1.0, 1.0};
  Dense<T, dyn, dyn> m(n, n);
  T* p = m.containerData();
  for (std::size_t i = 0; i < n * n; ++i) p[i] = static_cast<T>(dist(gen));
  return m;
}

template <typename T>
auto randomVec(std::size_t n, std::uint64_t seed) -> Vec<T, dyn> {
  std::mt19937_64 gen{seed};
  std::uniform_real_distribution<double> dist{-1.0, 1.0};
  Vec<T, dyn> v(n);
  T* p = v.containerData();
  for (std::size_t i = 0; i < n; ++i) p[i] = static_cast<T>(dist(gen));
  return v;
}

}  // namespace

auto main() -> int {
  std::println("matmul: multiply(A,B,dst) GFLOP/s (work = 2n³)");
  std::println("  {:>6}  {:>10}", "n", "GFLOP/s");
  for (std::size_t n : {16u, 32u, 64u, 128u, 256u, 512u}) {
    auto a = randomMat<double>(n, 1);
    auto b = randomMat<double>(n, 2);
    Dense<double, dyn, dyn> dst(n, n);
    const double work = 2.0 * static_cast<double>(n) * n * n;
    auto r = "matmul"_bench.warmup(2).minEpoch(2ms).maxRuntime(250ms).minSamples(3)
                 .flops(work).quiet() = [&] {
      multiply(a, b, dst);
      doNotOptimize(*dst.containerData());
    };
    std::println("  {:>6}  {:>10.2f}", n, r.gflops);
  }
  std::println("");

  // Transpose: a strided view costs on every pass; materializing pays one copy to
  // restore contiguous (cache-friendly) passes. Three arms expose the trade — the
  // view wins for a single use, materialize wins once reuse amortizes the copy.
  std::println("transpose reduction: GB/s (read n² doubles)");
  std::println("  {:>6}  {:>12}  {:>12}  {:>16}", "n", "contiguous", "strided view",
               "materialize+reduce");
  for (std::size_t n : {64u, 128u, 256u, 512u, 1024u}) {
    auto m = randomMat<double>(n, 3);
    const double bytes = static_cast<double>(n) * n * sizeof(double);
    auto reduce = [n](auto v) {
      double s = 0.0;
      for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) s += v[i, j];
      doNotOptimize(s);
    };
    auto contiguous = "t contiguous"_bench.warmup(3).minEpoch(2ms).maxRuntime(150ms)
                          .bytes(bytes).quiet() = [&] { reduce(m.toMdspan()); };
    auto strided = "t strided"_bench.warmup(3).minEpoch(2ms).maxRuntime(150ms)
                       .bytes(bytes).quiet() = [&] { reduce(transposed(m)); };
    auto materialized = "t materialize"_bench.warmup(3).minEpoch(2ms).maxRuntime(150ms)
                            .bytes(bytes).quiet() = [&] {
      auto r = materialize(transposed(m));
      reduce(r.toMdspan());
    };
    std::println("  {:>6}  {:>10.2f}  {:>10.2f}  {:>14.2f}", n, contiguous.gbytes,
                 strided.gbytes, materialized.gbytes);
  }
  std::println("");

  // norm2: the overflow-safe scaled form (a divide per element) vs a naive Σxᵢ². The
  // tax buys correctness on extreme inputs; this says how much it costs on normal ones.
  std::println("norm2: scaled (overflow-safe) vs naive √Σxᵢ²  [ns/element]");
  std::println("  {:>8}  {:>12}  {:>12}", "n", "scaled", "naive");
  for (std::size_t n : {1024u, 16384u, 262144u, 1048576u}) {
    auto x = randomVec<double>(n, 4);
    const double per = static_cast<double>(n);
    auto scaled = "norm2 scaled"_bench.warmup(5).minEpoch(2ms).maxRuntime(150ms).quiet() =
        [&] {
          double r = norm2(x);
          doNotOptimize(r);
        };
    auto naive = "norm2 naive"_bench.warmup(5).minEpoch(2ms).maxRuntime(150ms).quiet() = [&] {
      double s = 0.0;
      const double* p = x.containerData();
      for (std::size_t i = 0; i < n; ++i) s += p[i] * p[i];
      double r = std::sqrt(s);
      doNotOptimize(r);
    };
    std::println("  {:>8}  {:>10.3f}  {:>10.3f}", n,
                 static_cast<double>(scaled.median_wall.count()) / per,
                 static_cast<double>(naive.median_wall.count()) / per);
  }
  std::println("");

  // Accumulator: dot over float vectors widening to double (the library default —
  // halves SIMD lane count) vs accumulating in float. Quantifies the accuracy tax.
  std::println("dot(float): double accumulation (safe) vs float (fast)  [GFLOP/s, work=2n]");
  std::println("  {:>8}  {:>12}  {:>12}", "n", "double acc", "float acc");
  for (std::size_t n : {1024u, 16384u, 262144u, 1048576u}) {
    auto x = randomVec<float>(n, 5);
    auto y = randomVec<float>(n, 6);
    const double work = 2.0 * static_cast<double>(n);
    auto wide = "dot double"_bench.warmup(5).minEpoch(2ms).maxRuntime(150ms).flops(work)
                    .quiet() = [&] {
      auto r = dot(x, y);  // Accumulator widens float → double internally
      doNotOptimize(r);
    };
    auto narrow = "dot float"_bench.warmup(5).minEpoch(2ms).maxRuntime(150ms).flops(work)
                      .quiet() = [&] {
      float s = 0.0F;
      const float* px = x.containerData();
      const float* py = y.containerData();
      for (std::size_t i = 0; i < n; ++i) s += px[i] * py[i];
      doNotOptimize(s);
    };
    std::println("  {:>8}  {:>10.2f}  {:>10.2f}", n, wide.gflops, narrow.gflops);
  }
  std::println("");

  return 0;
}
