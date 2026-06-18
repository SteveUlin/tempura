#include "banded.h"

#include <cstddef>

#include "matrix.h"
#include "mdarray.h"
#include "unit.h"
#include "vec.h"

using namespace tempura;

// constexpr oracle: a banded matvec equals the equivalent dense matvec.
constexpr auto bandedMatvecMatchesDense() -> bool {
  Banded<double, 4, 3> a{};  // tridiagonal
  for (std::size_t i = 0; i < 4; ++i) {
    a.set(i, i, 2.0);
    if (i > 0) a.set(i, i - 1, -1.0);
    if (i + 1 < 4) a.set(i, i + 1, -1.0);
  }
  InlineVec<double, 4> x{1, 2, 3, 4};
  auto yb = multiply(a, x);
  auto yd = multiply(toDense(a), x);  // dense matvec over the materialized form
  for (std::size_t i = 0; i < 4; ++i)
    if (!isClose(yb[i], (yd[i]), {.rtol = 1e-12, .atol = 1e-12})) return false;
  return true;
}
static_assert(bandedMatvecMatchesDense());

auto main() -> int {
  "in-band reads return stored values; out-of-band reads return 0"_test = [] {
    Banded<double, 4, 3> a{};
    a.set(1, 1, 5.0);
    a.set(1, 0, 3.0);  // subdiagonal
    a.set(1, 2, 7.0);  // superdiagonal
    expectEq((a[1, 1]), 5.0);
    expectEq((a[1, 0]), 3.0);
    expectEq((a[1, 2]), 7.0);
    expectEq((a[1, 3]), 0.0);  // out of band → structural zero
    expectEq((a[3, 0]), 0.0);
    expectEq((a[0, 3]), 0.0);
  };

  "banded matvec equals dense matvec, and skips structural zeros"_test = [] {
    Banded<double, 5, 3> a{};
    for (std::size_t i = 0; i < 5; ++i) {
      a.set(i, i, 4.0);
      if (i > 0) a.set(i, i - 1, 1.0);
      if (i + 1 < 5) a.set(i, i + 1, 2.0);
    }
    InlineVec<double, 5> x{1, 2, 3, 4, 5};
    auto yb = multiply(a, x);
    auto yd = multiply(toDense(a), x);
    for (std::size_t i = 0; i < 5; ++i)
      expectClose((yb[i]), (yd[i]), {.rtol = 1e-12, .atol = 1e-12});
  };

  "solveTridiagonal: residual A·x − b ≈ 0"_test = [] {
    // diagonally dominant tridiagonal (Thomas needs no pivoting here)
    Banded<double, 5, 3> a{};
    for (std::size_t i = 0; i < 5; ++i) {
      a.set(i, i, 4.0);
      if (i > 0) a.set(i, i - 1, -1.0);
      if (i + 1 < 5) a.set(i, i + 1, -1.0);
    }
    InlineVec<double, 5> b{1, 2, 3, 4, 5};
    auto x = solveTridiagonal(a, b);
    auto residual = multiply(a, x);  // should reproduce b
    for (std::size_t i = 0; i < 5; ++i)
      expectClose((residual[i]), (b[i]), {.rtol = 1e-10, .atol = 1e-10});
  };

  "pentadiagonal (5 bands) reads and matvec"_test = [] {
    Banded<double, 5, 5> a{};  // center band = 2
    for (std::size_t i = 0; i < 5; ++i) a.set(i, i, 10.0);
    a.set(0, 2, 1.0);  // two above the diagonal
    a.set(2, 0, 2.0);  // two below
    expectEq((a[0, 2]), 1.0);
    expectEq((a[2, 0]), 2.0);
    expectEq((a[0, 3]), 0.0);  // beyond the band
    InlineVec<double, 5> x{1, 1, 1, 1, 1};
    auto yb = multiply(a, x);
    auto yd = multiply(toDense(a), x);
    for (std::size_t i = 0; i < 5; ++i)
      expectClose((yb[i]), (yd[i]), {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
