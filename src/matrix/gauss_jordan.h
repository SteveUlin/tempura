#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "matrix.h"

namespace tempura {

// Gauss-Jordan elimination (Numerical Recipes §2.1): reduce A to the identity by
// scaling each pivot to 1 and zeroing its ENTIRE column (not just below — that is the
// difference from LU). A is overwritten by A⁻¹ in place; any extra right-hand-side
// matrices B… ride along the same row operations, so each becomes A⁻¹·B (solving
// A·X = B for all of them at once). Partial row pivoting for stability.
//
// Returns false on a singular A — singularity is a legitimate outcome of factoring, not
// a precondition violation, so it is reported, not asserted. Floating-point only
// (elimination needs field division). A's columns are unscrambled at the end (see below);
// the ride-along B… need none.

template <MatrixView Am, MatrixView... Bm>
  requires std::floating_point<typename ViewOf<Am>::value_type>
constexpr auto gaussJordan(Am& a, Bm&... b) -> bool {
  auto va = view(a);
  using T = typename decltype(va)::value_type;
  const std::size_t n = static_cast<std::size_t>(va.extent(0));
  assert(va.extent(0) == va.extent(1) && "Gauss-Jordan with pivoting needs a square matrix");

  // Singularity is scale-relative: a pivot is "zero" only against the matrix's own
  // magnitude, so a well-conditioned but large/small-scaled system is not misflagged.
  T scale{};
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t j = 0; j < n; ++j) scale = std::max(scale, std::abs(va[i, j]));
  const T tol = scale * std::numeric_limits<T>::epsilon() * static_cast<T>(n);

  auto swapRows = [](auto m, std::size_t r1, std::size_t r2) {
    for (std::size_t k = 0; k < static_cast<std::size_t>(m.extent(1)); ++k) {
      using std::swap;
      swap(m[r1, k], m[r2, k]);
    }
  };

  // Per-step pivot row, for the column unscramble below. The in-place inverse trick
  // builds A⁻¹ in A's own storage, but each physical row swap also permutes the
  // partially-built inverse columns — so A (not the ride-along B, which is a plain
  // augmented solve) needs its columns unscrambled at the end.
  std::vector<std::size_t> pivots(n);

  for (std::size_t i = 0; i < n; ++i) {
    // Partial pivot: largest-magnitude candidate in column i, rows i..n-1.
    std::size_t pivot = i;
    for (std::size_t r = i + 1; r < n; ++r)
      if (std::abs(va[r, i]) > std::abs(va[pivot, i])) pivot = r;
    pivots[i] = pivot;
    if (pivot != i) {
      swapRows(va, i, pivot);
      (swapRows(view(b), i, pivot), ...);
    }
    if (std::abs(va[i, i]) <= tol) return false;  // singular

    const T inv = T{1} / va[i, i];
    va[i, i] = T{1};
    for (std::size_t k = 0; k < n; ++k) va[i, k] *= inv;
    (([&] {
       auto vb = view(b);
       for (std::size_t k = 0; k < static_cast<std::size_t>(vb.extent(1)); ++k) vb[i, k] *= inv;
     }()),
     ...);

    for (std::size_t r = 0; r < n; ++r) {
      if (r == i) continue;
      const T factor = va[r, i];
      va[r, i] = T{};
      for (std::size_t k = 0; k < n; ++k) va[r, k] -= factor * va[i, k];
      (([&] {
         auto vb = view(b);
         for (std::size_t k = 0; k < static_cast<std::size_t>(vb.extent(1)); ++k)
           vb[r, k] -= factor * vb[i, k];
       }()),
       ...);
    }
  }

  // Unscramble A's columns in reverse pivot order to undo the row-swap permutation.
  for (std::size_t l = n; l-- > 0;) {
    if (pivots[l] == l) continue;
    for (std::size_t r = 0; r < n; ++r) {
      using std::swap;
      swap(va[r, l], va[r, pivots[l]]);
    }
  }
  return true;
}

}  // namespace tempura
