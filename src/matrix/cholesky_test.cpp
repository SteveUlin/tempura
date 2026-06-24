#include "cholesky.h"

#include <cmath>
#include <cstddef>

#include "matrix.h"
#include "mdarray.h"
#include "transpose.h"
#include "unit.h"
#include "vec.h"

using namespace tempura;

// constexpr oracle: the classic SPD example A = [[4,12,-16],[12,37,-43],[-16,-43,98]]
// factors to L = [[2,0,0],[6,1,0],[-8,5,3]] (Wikipedia), and L·Lᵀ reconstructs A.
constexpr auto reconstructsKnown() -> bool {
  InlineDense<double, 3, 3> a{{4, 12, -16}, {12, 37, -43}, {-16, -43, 98}};
  auto ch = cholesky(a);
  if (!ch.positive_definite) return false;
  const double expectL[3][3] = {{2, 0, 0}, {6, 1, 0}, {-8, 5, 3}};
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      if (!isClose(ch.factor[i, j], expectL[i][j], {.rtol = 1e-12, .atol = 1e-12})) return false;
  auto recon = multiply(ch.factor, transposed(ch.factor));  // L·Lᵀ = A
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      if (!isClose(recon[i, j], a[i, j], {.rtol = 1e-12, .atol = 1e-12})) return false;
  return true;
}
static_assert(reconstructsKnown());

auto main() -> int {
  "L·Lᵀ = A; solve residual ≈ 0; log|A| and det match"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(3, 3));
    auto va = a.toMdspan();
    double vals[3][3] = {{4, 12, -16}, {12, 37, -43}, {-16, -43, 98}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];

    auto ch = cholesky(a);
    expectTrue(ch.positive_definite);

    auto recon = multiply(ch.factor, transposed(ch.factor));
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((recon[i, j]), vals[i][j], {.rtol = 1e-10, .atol = 1e-10});

    // det = (2·1·3)² = 36; log|A| = log(36)
    expectClose(determinant(ch), 36.0, {.rtol = 1e-10, .atol = 1e-10});
    expectClose(logDeterminant(ch), std::log(36.0), {.rtol = 1e-10, .atol = 1e-10});

    Vec<double, Dyn> b(dims(3));
    b[0] = 1; b[1] = 2; b[2] = 3;
    auto x = solve(ch, b);
    for (std::size_t i = 0; i < 3; ++i) {  // residual A·x − b ≈ 0
      double row = 0;
      for (std::size_t j = 0; j < 3; ++j) row += vals[i][j] * x[j];
      expectClose(row, (b[i]), {.rtol = 1e-9, .atol = 1e-9});
    }
  };

  "inverse: A·A⁻¹ = I"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(2, 2));
    auto va = a.toMdspan();
    va[0, 0] = 2; va[0, 1] = 1; va[1, 0] = 1; va[1, 1] = 2;  // SPD
    auto ch = cholesky(a);
    auto inv = inverse(ch);
    auto prod = multiply(a, inv);
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        expectClose((prod[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-10, .atol = 1e-10});
  };

  "non-positive-definite input is flagged, not masked"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(2, 2));
    auto va = a.toMdspan();
    va[0, 0] = 1; va[0, 1] = 2; va[1, 0] = 2; va[1, 1] = 1;  // symmetric, eigenvalues 3,−1
    auto ch = cholesky(a);
    expectFalse(ch.positive_definite);  // solve()/det would assert on this
  };

  return TestRegistry::result();
}
