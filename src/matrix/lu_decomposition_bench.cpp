#include "benchmark.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mdspan>
#include <random>
#include <span>
#include <utility>
#include <vector>

#include "dyn.h"
#include "matrix.h"
#include "mdarray.h"
#include "unit.h"

// The point of the exercise: lu_decomposition.h claims eager physical row swaps beat
// "lazy permuted-view indirection — that is cache-hostile." This A/B measures it.
//
// Both arms run BIT-IDENTICAL arithmetic on identical data (same scale-invariant pivot
// metric ⇒ same pivot sequence), so any time gap is purely physical-swap vs
// gather-on-access. The lazy arm is a measurement FOIL — never shipped.

using namespace tempura;
using namespace std::chrono_literals;

namespace {

using Mat = Dense<double, Dyn, Dyn>;
using MS = std::mdspan<double, std::dextents<std::size_t, 2>>;

// Fixed-seed random n×n, entries in [-1,1]. No diagonal bump: we WANT partial
// pivoting to fire heavily so the lazy arm's row[] scrambles and its gathers hit
// scattered physical rows — that is the cache-hostility under test. Almost surely
// nonsingular; growth stays finite (we only time the arithmetic, not its accuracy).
auto randomMatrix(std::size_t n, std::uint64_t seed) -> std::vector<double> {
  std::mt19937_64 gen{seed};
  std::uniform_real_distribution<double> dist{-1.0, 1.0};
  std::vector<double> a(n * n);
  for (auto& x : a) x = dist(gen);
  return a;
}

// EAGER arm: the production strategy. On a pivot, physically swap the two rows (n
// contiguous doubles) so every later access is a direct, sequential, vectorizable read.
void factorEager(MS f, std::span<double> scale, std::span<std::size_t> perm) {
  const std::size_t n = f.extent(0);
  for (std::size_t i = 0; i < n; ++i) {
    double s = 0.0;
    for (std::size_t j = 0; j < n; ++j) s = std::max(s, std::abs(f[i, j]));
    scale[i] = s;
    perm[i] = i;
  }
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t pivot = i;
    double best = 0.0;
    for (std::size_t ii = i; ii < n; ++ii) {
      const double m = scale[ii] == 0.0 ? 0.0 : std::abs(f[ii, i]) / scale[ii];
      if (m > best) { best = m; pivot = ii; }
    }
    if (pivot != i) {
      for (std::size_t k = 0; k < n; ++k) { using std::swap; swap(f[i, k], f[pivot, k]); }
      std::swap(scale[i], scale[pivot]);
      std::swap(perm[i], perm[pivot]);
    }
    if (f[i, i] != 0.0)
      for (std::size_t j = i + 1; j < n; ++j) {
        f[j, i] /= f[i, i];
        for (std::size_t k = i + 1; k < n; ++k) f[j, k] -= f[j, i] * f[i, k];
      }
  }
}

// LAZY arm: defer the swap. A pivot swaps two INDICES (O(1)); every data access is
// gathered through row[]. scale is intrinsic to a physical row, so it is indexed by
// physical id and never swapped. Identical arithmetic to factorEager.
void factorLazy(MS f, std::span<double> scale_phys, std::span<std::size_t> row) {
  const std::size_t n = f.extent(0);
  for (std::size_t p = 0; p < n; ++p) {
    double s = 0.0;
    for (std::size_t j = 0; j < n; ++j) s = std::max(s, std::abs(f[p, j]));
    scale_phys[p] = s;
    row[p] = p;
  }
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t pivot = i;
    double best = 0.0;
    for (std::size_t ii = i; ii < n; ++ii) {
      const std::size_t rii = row[ii];
      const double m = scale_phys[rii] == 0.0 ? 0.0 : std::abs(f[rii, i]) / scale_phys[rii];
      if (m > best) { best = m; pivot = ii; }
    }
    std::swap(row[i], row[pivot]);  // index swap only — the deferred row movement
    const std::size_t pi = row[i];
    if (f[pi, i] != 0.0)
      for (std::size_t j = i + 1; j < n; ++j) {
        const std::size_t pj = row[j];
        f[pj, i] /= f[pi, i];
        for (std::size_t k = i + 1; k < n; ++k) f[pj, k] -= f[pj, i] * f[pi, k];
      }
  }
}

// Refill the workspace from the pristine input (layout_right, contiguous). Charged to
// both arms equally — an O(n²) common offset, dominated by the O(n³) factorization.
void refill(Mat& ws, const std::vector<double>& pristine) {
  double* dst = ws.containerData();
  for (std::size_t i = 0; i < pristine.size(); ++i) dst[i] = pristine[i];
}

}  // namespace

auto main() -> int {
  // Faithfulness oracle: the foil must compute the SAME factorization as the eager
  // path, else the A/B compares two different algorithms. Identical op order ⇒ values
  // match to the bit; a loose tolerance still catches a structural mistake.
  "lazy foil reproduces the eager factorization"_test = [] {
    constexpr std::size_t n = 6;
    auto pristine = randomMatrix(n, 1);
    Mat e(dims(n, n));
    Mat l(dims(n, n));
    refill(e, pristine);
    refill(l, pristine);
    std::vector<double> se(n);
    std::vector<std::size_t> pe(n);
    std::vector<double> sl(n);
    std::vector<std::size_t> rl(n);
    factorEager(e.toMdspan(), se, pe);
    factorLazy(l.toMdspan(), sl, rl);
    auto fe = e.toMdspan();
    auto fl = l.toMdspan();
    for (std::size_t i = 0; i < n; ++i)
      for (std::size_t j = 0; j < n; ++j)
        expectClose((fe[i, j]), (fl[rl[i], j]), {.rtol = 1e-12, .atol = 1e-12});
  };

  std::println("");
  std::println("LU factorization: eager physical-swap vs lazy gather-on-access");
  std::println("  GFLOP/s (work = ⅔n³), fixed-seed random matrix, median of samples");
  std::println("  {:>6}  {:>12}  {:>12}  {:>8}", "n", "eager", "lazy", "eager/lazy");

  bool crossed = false;
  for (std::size_t n : {4u, 8u, 16u, 32u, 64u, 128u, 256u, 512u}) {
    const auto pristine = randomMatrix(n, 0xC0FFEE);
    const double work = (2.0 / 3.0) * static_cast<double>(n) * n * n;

    Mat ws(dims(n, n));
    std::vector<double> scale(n);
    std::vector<std::size_t> perm(n);

    auto eager = "lu eager"_bench.warmup(2).minEpoch(2ms).maxRuntime(250ms).minSamples(3)
                     .flops(work).quiet() = [&] {
      refill(ws, pristine);
      factorEager(ws.toMdspan(), scale, perm);
      doNotOptimize(*ws.containerData());
    };
    auto lazy = "lu lazy"_bench.warmup(2).minEpoch(2ms).maxRuntime(250ms).minSamples(3)
                    .flops(work).quiet() = [&] {
      refill(ws, pristine);
      factorLazy(ws.toMdspan(), scale, perm);
      doNotOptimize(*ws.containerData());
    };

    const double speedup = lazy.gflops > 0.0 ? eager.gflops / lazy.gflops : 0.0;
    const bool eager_wins = eager.gflops > lazy.gflops;
    std::println("  {:>6}  {:>10.2f}  {:>10.2f}  {:>8.2f}x  {}", n, eager.gflops,
                 lazy.gflops, speedup, eager_wins ? "← eager" : "← lazy");
    if (eager_wins && !crossed) {
      std::println("  ── crossover: eager overtakes lazy at n = {} ──", n);
      crossed = true;
    }
  }
  std::println("");

  return TestRegistry::result();
}
