#include "svd.h"

#include <cstddef>

#include "matrix.h"
#include "mdarray.h"
#include "transpose.h"
#include "unit.h"

using namespace tempura;

// Reconstruct A[i,j] = Σ_k U[i,k]·σ_k·V[j,k].
template <typename Svd>
constexpr auto recon(const Svd& s, std::size_t i, std::size_t j, std::size_t n) -> double {
  double acc = 0;
  for (std::size_t k = 0; k < n; ++k) acc += s.u[i, k] * s.singular_values[k] * s.v[j, k];
  return acc;
}

// constexpr oracle: symmetric [[2,1],[1,2]] has singular values 3, 1; U·Σ·Vᵀ = A.
constexpr auto reconstructs2x2() -> bool {
  InlineDense<double, 2, 2> a{{2, 1}, {1, 2}};
  auto s = svd(a);
  if (!isClose(s.singular_values[0], 3.0, {.rtol = 1e-10, .atol = 1e-10})) return false;
  if (!isClose(s.singular_values[1], 1.0, {.rtol = 1e-10, .atol = 1e-10})) return false;
  for (std::size_t i = 0; i < 2; ++i)
    for (std::size_t j = 0; j < 2; ++j)
      if (!isClose(recon(s, i, j, 2), a[i, j], {.rtol = 1e-10, .atol = 1e-10})) return false;
  return true;
}
static_assert(reconstructs2x2());

auto main() -> int {
  "U·Σ·Vᵀ = A, UᵀU = VᵀV = I, σ descending and ≥ 0"_test = [] {
    Dense<double, dyn, dyn> a(3, 3);
    auto va = a.toMdspan();
    double vals[3][3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 10}};  // nonsingular general matrix
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];

    auto s = svd(a);

    for (std::size_t i = 0; i < 3; ++i)  // reconstruction
      for (std::size_t j = 0; j < 3; ++j)
        expectClose(recon(s, i, j, 3), vals[i][j], {.rtol = 1e-9, .atol = 1e-9});

    auto gu = multiply(transposed(s.u), s.u);  // UᵀU = I
    auto gv = multiply(transposed(s.v), s.v);  // VᵀV = I
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) {
        expectClose((gu[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-9, .atol = 1e-9});
        expectClose((gv[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-9, .atol = 1e-9});
      }

    expectGE((s.singular_values[0]), (s.singular_values[1]));  // descending
    expectGE((s.singular_values[1]), (s.singular_values[2]));
    for (std::size_t k = 0; k < 3; ++k) expectGE((s.singular_values[k]), 0.0);  // non-negative
  };

  "known singular values: diag(3,1,2) → 3, 2, 1"_test = [] {
    Dense<double, dyn, dyn> a(3, 3);
    auto va = a.toMdspan();
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = 0.0;
    va[0, 0] = 3; va[1, 1] = 1; va[2, 2] = 2;
    auto s = svd(a);
    expectClose((s.singular_values[0]), 3.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((s.singular_values[1]), 2.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((s.singular_values[2]), 1.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
