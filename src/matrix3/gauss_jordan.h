#pragma once

#include <algorithm>
#include <cstdint>
#include <cstddef>

#include "matrix3/permutation.h"

namespace tempura::matrix3 {

enum class Pivot : uint8_t {
  kNone,
  kRow,
};

// Single pivot reduction: scale pivot to 1, zero all other column elements
template <auto eps, typename MatrixT, typename... MatrixTs>
constexpr auto gaussJordanReduceImpl_(int64_t i, int64_t j, MatrixT& A,
                                      MatrixTs&... B) -> bool {
  using std::abs;
  if (abs(A[i, j]) < eps) [[unlikely]] {
    return false;
  }

  auto inv = 1.0 / A[i, j];
  A[i, j] = 1.0;

  // Scale pivot row
  for (int64_t k = 0; k < static_cast<int64_t>(A.extent().extent(1)); ++k) {
    A[i, k] *= inv;
  }
  (
      [&] {
        for (int64_t k = 0; k < static_cast<int64_t>(B.extent().extent(1)); ++k) {
          B[i, k] *= inv;
        }
      }(),
      ...);

  // Reduce all other rows
  for (int64_t k = 0; k < static_cast<int64_t>(A.extent().extent(0)); ++k) {
    if (k == i) {
      continue;
    }
    auto factor = A[k, j];
    A[k, j] = 0.0;
    for (int64_t l = 0; l < static_cast<int64_t>(A.extent().extent(1)); ++l) {
      A[k, l] -= factor * A[i, l];
    }
    (
        [&] {
          for (int64_t l = 0; l < static_cast<int64_t>(B.extent().extent(1)); ++l) {
            B[k, l] -= factor * B[i, l];
          }
        }(),
        ...);
  }

  return true;
}

// Gauss-Jordan Elimination
// ------------------------
// Numerical Recipes 3rd Edition, Section 2.1
//
// Invert a matrix using "Gauss-Jordan Elimination": all elements in the
// column of the pivot are zeroed and the pivot is scaled to one.
//
// ⎡ . X . . ⎤
// ⎢ . 1 . . ⎥
// ⎢ . X . . ⎥
// ⎣ . X . . ⎦
//
// Modifies input matrix A in place. Additional matrices B can be provided
// to transform along with A, effectively solving the system Ax = B.
//
// Returns false if matrix is singular (attempts to divide by value < eps).

template <Pivot pivot, auto eps = 1e-10, typename MatrixT, typename... MatrixTs>
  requires(pivot == Pivot::kNone)
constexpr auto gaussJordan(MatrixT& A, MatrixTs&... B) -> bool {
  auto rows = static_cast<int64_t>(A.extent().extent(0));
  auto cols = static_cast<int64_t>(A.extent().extent(1));

  using std::min;
  for (int64_t i = 0; i < min(rows, cols); ++i) {
    if (!gaussJordanReduceImpl_<eps>(i, i, A, B...)) [[unlikely]] {
      return false;
    }
  }
  return true;
}

template <Pivot pivot, auto eps = 1e-10, typename MatrixT, typename... MatrixTs>
  requires(pivot == Pivot::kRow)
constexpr auto gaussJordan(MatrixT& A, MatrixTs&... B) -> bool {
  auto rows = static_cast<int64_t>(A.extent().extent(0));
  auto cols = static_cast<int64_t>(A.extent().extent(1));

  // Row pivoting requires square matrix
  if (rows != cols) {
    return false;
  }

  auto perm = [&] {
    if constexpr (A.extent().staticExtent(0) == kDynamic) {
      return Permutation<kDynamic>{static_cast<std::size_t>(rows)};
    } else {
      return Permutation<A.extent().staticExtent(0)>{};
    }
  }();

  for (int64_t i = 0; i < rows; ++i) {
    // Find largest pivot in column i
    int64_t biggest = i;
    for (int64_t j = i; j < cols; ++j) {
      using std::abs;
      if (abs(A[perm.data()[j], i]) > abs(A[perm.data()[biggest], i])) {
        biggest = j;
      }
    }

    // Swap rows physically (not just in permutation)
    perm.swap(i, biggest);
    for (int64_t j = 0; j < cols; ++j) {
      std::swap(A[i, j], A[biggest, j]);
    }
    (
        [&] {
          for (int64_t j = 0; j < static_cast<int64_t>(B.extent().extent(1)); ++j) {
            std::swap(B[i, j], B[biggest, j]);
          }
        }(),
        ...);

    if (!gaussJordanReduceImpl_<eps>(i, i, A, B...)) [[unlikely]] {
      return false;
    }
  }

  // Unscramble columns: apply inverse permutation to columns of A
  // After row operations, we have x = A' P B where P is permutation matrix
  // To get the inverse, apply permutation in reverse order to columns
  for (int64_t j = cols - 1; j >= 0; --j) {
    if (perm.data()[j] == j) {
      continue;  // Skip if already in correct position
    }
    for (int64_t i = 0; i < rows; ++i) {
      std::swap(A[i, j], A[i, perm.data()[j]]);
    }
    perm.swap(j, perm.data()[j]);
  }

  return true;
}

}  // namespace tempura::matrix3
