#include "symmetric_eigen.h"

#include <cmath>
#include <cstddef>

#include "matrix.h"
#include "mdarray.h"
#include "transpose.h"
#include "unit.h"

using namespace tempura;

// constexpr oracle: V·D·Vᵀ reconstructs A for [[2,1],[1,2]] (eigenvalues 1, 3).
constexpr auto reconstructs2x2() -> bool {
  InlineDense<double, 2, 2> a{{2, 1}, {1, 2}};
  auto eig = symmetricEigen(a);
  if (!isClose(eig.values[0], 1.0, {.rtol = 1e-10, .atol = 1e-10})) return false;
  if (!isClose(eig.values[1], 3.0, {.rtol = 1e-10, .atol = 1e-10})) return false;
  // A·v = λ·v for each column
  for (std::size_t c = 0; c < 2; ++c)
    for (std::size_t i = 0; i < 2; ++i) {
      double av = 0;
      for (std::size_t k = 0; k < 2; ++k) av += a[i, k] * eig.vectors[k, c];
      if (!isClose(av, eig.values[c] * eig.vectors[i, c], {.rtol = 1e-10, .atol = 1e-10}))
        return false;
    }
  return true;
}
static_assert(reconstructs2x2());

auto main() -> int {
  // 1-D Laplacian: eigenvalues 2 − √2, 2, 2 + √2 (well separated, non-degenerate).
  "eigenvalues match the 1-D Laplacian spectrum; A·V = V·D; VᵀV = I"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(3, 3));
    auto va = a.toMdspan();
    double vals[3][3] = {{2, -1, 0}, {-1, 2, -1}, {0, -1, 2}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];

    auto eig = symmetricEigen(a);
    const double s = std::sqrt(2.0);
    expectClose((eig.values[0]), 2.0 - s, {.rtol = 1e-9, .atol = 1e-9});
    expectClose((eig.values[1]), 2.0, {.rtol = 1e-9, .atol = 1e-9});
    expectClose((eig.values[2]), 2.0 + s, {.rtol = 1e-9, .atol = 1e-9});

    // A·vᵢ = λᵢ·vᵢ
    for (std::size_t c = 0; c < 3; ++c)
      for (std::size_t i = 0; i < 3; ++i) {
        double av = 0;
        for (std::size_t k = 0; k < 3; ++k) av += vals[i][k] * eig.vectors[k, c];
        expectClose(av, (eig.values[c] * eig.vectors[i, c]), {.rtol = 1e-9, .atol = 1e-9});
      }

    auto gram = multiply(transposed(eig.vectors), eig.vectors);  // VᵀV = I
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((gram[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-9, .atol = 1e-9});
  };

  "degenerate spectrum: [[4,1,1],[1,4,1],[1,1,4]] has eigenvalues 3, 3, 6"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(3, 3));
    auto va = a.toMdspan();
    double vals[3][3] = {{4, 1, 1}, {1, 4, 1}, {1, 1, 4}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];

    auto eig = symmetricEigen(a);
    expectClose((eig.values[0]), 3.0, {.rtol = 1e-9, .atol = 1e-9});
    expectClose((eig.values[1]), 3.0, {.rtol = 1e-9, .atol = 1e-9});
    expectClose((eig.values[2]), 6.0, {.rtol = 1e-9, .atol = 1e-9});
    // even with a repeated eigenvalue the basis must stay orthonormal
    auto gram = multiply(transposed(eig.vectors), eig.vectors);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((gram[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-9, .atol = 1e-9});
  };

  return TestRegistry::result();
}
