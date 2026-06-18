#include "lu_decomposition.h"

#include <cstddef>

#include "mdarray.h"
#include "matrix.h"
#include "unit.h"
#include "vec.h"

using namespace tempura;

// constexpr end to end: factor + solve a 2x2 at compile time (case 1, x == {1, 1}).
constexpr auto constexprSolve() -> bool {
  InlineDense<double, 2, 2> a{{2, 1}, {1, 2}};
  InlineVec<double, 2> b{3, 3};
  auto x = solve(luDecompose(a), b);
  auto near = [](double v, double t) { return v - t < 1e-12 && t - v < 1e-12; };
  return near(x[0], 1.0) && near(x[1], 1.0);
}
static_assert(constexprSolve());

auto main() -> int {
  // A·x ≈ b, computed by re-multiplying — the honest oracle (never trust a hardcoded x).
  auto solvesTo = [](const auto& a, const auto& x, const auto& b, double atol) {
    auto r = multiply(a, x);
    for (std::size_t i = 0; i < b.size(); ++i) expectClose((r[i]), (b[i]), {.atol = atol});
  };

  "2x2 solve"_test = [&] {
    Dense<double, 2, 2> a{{2, 1}, {1, 2}};
    Vec<double, 2> b{3, 3};
    auto x = solve(luDecompose(a), b);
    expectClose((x[0]), 1.0, {.atol = 1e-12});
    expectClose((x[1]), 1.0, {.atol = 1e-12});
  };

  "3x3 solve (residual) and determinant"_test = [&] {
    Dense<double, 3, 3> a{{1, 2, 3}, {2, 5, 2}, {6, -3, 1}};
    Vec<double, 3> b{1, 2, 3};
    auto lu = luDecompose(a);
    expectClose(determinant(lu), -77.0, {.rtol = 1e-12});
    solvesTo(a, solve(lu, b), b, 1e-12);
  };

  "identity system returns the RHS"_test = [&] {
    Dense<double, 3, 3> i3;
    identity(i3);
    Vec<double, 3> b{1, 2, 3};
    auto x = solve(luDecompose(i3), b);
    expectClose((x[0]), 1.0, {.atol = 1e-15});
    expectClose((x[1]), 2.0, {.atol = 1e-15});
    expectClose((x[2]), 3.0, {.atol = 1e-15});
  };

  "multiple right-hand sides"_test = [] {
    Dense<double, 2, 2> a{{2, 1}, {1, 2}};
    Dense<double, 2, 2> b{{5, 3}, {4, 3}};  // columns {5,4} and {3,3}
    auto x = solve(luDecompose(a), b);      // → columns {2,1} and {1,1}
    auto r = multiply(a, x);
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t k = 0; k < 2; ++k) expectClose((r[i, k]), (b[i, k]), {.atol = 1e-12});
  };

  "determinants"_test = [] {
    Dense<double, 2, 2> a2{{2, 1}, {1, 2}};
    expectClose(determinant(a2), 3.0, {.rtol = 1e-12});
    Dense<double, 3, 3> a3{{1, 2, 3}, {0, 1, 4}, {5, 6, 0}};
    expectClose(determinant(a3), 1.0, {.rtol = 1e-12});
  };

  "tiny pivot is fixed by pivoting"_test = [&] {
    Dense<double, 3, 3> a{{0.001, 2.0, 3.0}, {1.0, 1.0, 1.0}, {2.0, 1.0, 0.5}};
    Vec<double, 3> b{13.001, 6.0, 5.5};  // = A · {1, 2, 3}
    auto x = solve(luDecompose(a), b);
    expectClose((x[0]), 1.0, {.atol = 1e-9});
    expectClose((x[1]), 2.0, {.atol = 1e-9});
    expectClose((x[2]), 3.0, {.atol = 1e-9});
  };

  "reconstruction: P·A = L·U (pins pivot direction)"_test = [] {
    Dense<double, 3, 3> a{{2, 1, 1}, {4, -6, 0}, {-2, 7, 2}};
    auto lu = luDecompose(a);
    Dense<double, 3, 3> lmat;
    Dense<double, 3, 3> umat;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) {
        lmat[i, j] = i > j ? lu.factors[i, j] : (i == j ? 1.0 : 0.0);
        umat[i, j] = i <= j ? lu.factors[i, j] : 0.0;
      }
    auto lu_product = multiply(lmat, umat);
    Dense<double, 3, 3> pa = a;
    lu.perm.permuteRows(pa);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) expectClose((lu_product[i, j]), (pa[i, j]), {.atol = 1e-12});
  };

  "singular matrix is flagged; determinant ≈ 0"_test = [] {
    Dense<double, 3, 3> a{{1, 2, 3}, {2, 4, 6}, {1, 1, 1}};
    auto lu = luDecompose(a);
    expectTrue(lu.singular);
    expectClose(determinant(lu), 0.0, {.atol = 1e-10});
  };

  "determinant sign tracks pivot parity"_test = [] {
    Dense<double, 2, 2> swap2{{0, 1}, {1, 0}};
    expectClose(determinant(swap2), -1.0, {.rtol = 1e-12});
    Dense<double, 3, 3> rev{{0, 0, 1}, {0, 1, 0}, {1, 0, 0}};
    expectClose(determinant(rev), -1.0, {.rtol = 1e-12});
  };

  "scale-invariant pivoting on a badly-scaled system"_test = [] {
    Dense<double, 2, 2> a{{1e10, 1e10}, {1.0, 2.0}};
    Vec<double, 2> b{2e10, 3.0};  // = A · {1, 1}
    auto x = solve(luDecompose(a), b);
    auto r = multiply(a, x);
    expectClose((r[0]), 2e10, {.rtol = 1e-12});
    expectClose((r[1]), 3.0, {.atol = 1e-9});
  };

  "4x4 residual"_test = [&] {
    Dense<double, 4, 4> a{{4, 3, 2, 1}, {3, 4, 3, 2}, {2, 3, 4, 3}, {1, 2, 3, 4}};
    Vec<double, 4> b{1, 2, 3, 4};
    solvesTo(a, solve(luDecompose(a), b), b, 1e-12);
  };

  "well-conditioned but badly-scaled matrix is NOT flagged singular"_test = [&] {
    Dense<double, 2, 2> a{{1e16, 0.0}, {0.0, 1.0}};  // det = 1e16, perfectly invertible
    auto lu = luDecompose(a);
    expectFalse(lu.singular);  // absolute eps would have wrongly flagged the 1.0 pivot
    Vec<double, 2> b{1e16, 2.0};  // = A · {1, 2}
    auto x = solve(lu, b);
    expectClose((x[0]), 1.0, {.rtol = 1e-12});
    expectClose((x[1]), 2.0, {.atol = 1e-9});
  };

  "inverse: A · A⁻¹ = I"_test = [] {
    Dense<double, 3, 3> a{{1, 2, 3}, {2, 5, 2}, {6, -3, 1}};
    auto inv = inverse(a);
    auto prod = multiply(a, inv);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((prod[i, j]), i == j ? 1.0 : 0.0, {.atol = 1e-12});
  };

  return TestRegistry::result();
}
