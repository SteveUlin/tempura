#include "gauss_jordan.h"

#include <cstddef>

#include "lu_decomposition.h"
#include "matrix.h"
#include "mdarray.h"
#include "unit.h"

using namespace tempura;

// constexpr oracle: invert in place, then A·A⁻¹ must be the identity.
constexpr auto invertsToIdentity() -> bool {
  InlineDense<double, 3, 3> a{{2, 1, 1}, {1, 3, 2}, {1, 0, 0}};
  InlineDense<double, 3, 3> inv{{2, 1, 1}, {1, 3, 2}, {1, 0, 0}};  // copy
  if (!gaussJordan(inv)) return false;
  auto prod = multiply(a, inv);
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      if (!isClose(prod[i, j], i == j ? 1.0 : 0.0, {.rtol = 1e-12, .atol = 1e-12}))
        return false;
  return true;
}
static_assert(invertsToIdentity());

auto main() -> int {
  "gaussJordan inverts: A·A⁻¹ = I, and matches LU inverse"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(3, 3));
    auto va = a.toMdspan();
    double vals[3][3] = {{2, 1, 1}, {1, 3, 2}, {1, 0, 0}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];

    Dense<double, Dyn, Dyn> inv(dims(3, 3));
    auto vi = inv.toMdspan();
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) vi[i, j] = vals[i][j];
    expectTrue(gaussJordan(inv));

    auto prod = multiply(a, inv);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((prod[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-10, .atol = 1e-10});

    // cross-check against the independent LU inverse
    auto lu_inv = inverse(a);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((inv[i, j]), (lu_inv[i, j]), {.rtol = 1e-10, .atol = 1e-10});
  };

  "gaussJordan solves A·X = B (ride-along RHS)"_test = [] {
    // A·x = b with A = [[2,1],[1,3]], b = [3,5] → x = [0.8, 1.4]
    Dense<double, Dyn, Dyn> a(dims(2, 2));
    auto va = a.toMdspan();
    va[0, 0] = 2; va[0, 1] = 1; va[1, 0] = 1; va[1, 1] = 3;
    Dense<double, Dyn, Dyn> b(dims(2, 1));
    auto vb = b.toMdspan();
    vb[0, 0] = 3; vb[1, 0] = 5;
    expectTrue(gaussJordan(a, b));            // a → A⁻¹, b → A⁻¹·b = x
    expectClose((b[0, 0]), 0.8, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((b[1, 0]), 1.4, {.rtol = 1e-12, .atol = 1e-12});
  };

  "gaussJordan reports a singular matrix"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(2, 2));
    auto va = a.toMdspan();
    va[0, 0] = 1; va[0, 1] = 2; va[1, 0] = 2; va[1, 1] = 4;  // row 2 = 2·row 1
    expectFalse(gaussJordan(a));
  };

  "gaussJordan needs pivoting: zero leading pivot"_test = [] {
    // [[0,1],[1,0]] inverse is itself; without pivoting the 0 pivot would fail
    Dense<double, Dyn, Dyn> a(dims(2, 2));
    auto va = a.toMdspan();
    va[0, 0] = 0; va[0, 1] = 1; va[1, 0] = 1; va[1, 1] = 0;
    expectTrue(gaussJordan(a));
    expectClose((a[0, 0]), 0.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((a[0, 1]), 1.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((a[1, 0]), 1.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
