#pragma once

#include "matrix2/matrix.h"
#include "matrix2/storage/permutation.h"
#include "matrix2/to_string.h"

namespace tempura::matrix {

namespace internal {

template <auto eps>
constexpr auto gaussJordanReduce(int64_t i, int64_t j, MatrixT auto& A,
                                 MatrixT auto&... B) -> bool {
  using std::abs;
  if (abs(A[i, j]) < eps) [[unlikely]] {
    return false;
  }
  auto inv = 1.0 / A[i, j];
  A[i, j] = 1.0;
  for (int64_t k = 0; k < A.shape().col; ++k) {
    A[i, k] *= inv;
  }
  (
      [&] {
        for (int64_t k = 0; k < B.shape().col; ++k) {
          B[i, k] *= inv;
        }
      }(),
      ...);

  // Reduce all other rows
  for (int64_t k = 0; k < A.shape().row; ++k) {
    if (k == i) {
      continue;
    }
    auto factor = A[k, j];
    A[k, j] = 0.0;
    for (int64_t l = 0; l < A.shape().col; ++l) {
      A[k, l] -= factor * A[i, l];
    }
    (
        [&] {
          for (int64_t l = 0; l < B.shape().col; ++l) {
            B[k, l] -= factor * B[i, l];
          }
        }(),
        ...);
  }

  return true;
}

}  // namespace internal

// Gauss Jordan Elimination
// ------------------------
// Numerical Recipes 3rd Edition, Section 2.1
//
// Invert a provided matrix using "Gauss Jordan Elemination". That is, all
// elements in the column of the pivot are zeroed and the pivot is scaled to
// one.
//
// ⎡ . X . . ⎤
// ⎢ . 1 . . ⎥
// ⎢ . X . . ⎥
// ⎣ . X . . ⎦
//
// This implementation will modify the input matrix A in place, only allocating
// additional memory for pivoting. You may provide additional Matrices `B` to be
// transformed along with A, effectively solving the system Ax = B.
//
// This method will return false if it attempts to divide by a number within
// eps of zero. That is if:
// - The matrix is singular
// - The matrix is very ill-conditioned
// - You're not using pivoting and got unlucky

template <Pivot pivot, auto eps = 1e-10>
  requires(pivot == Pivot::kNone)
constexpr auto gaussJordan(MatrixT auto& A, MatrixT auto&... B) -> bool {
  (CHECK(A.shape().row == B.shape().row), ...);

  using std::min;
  for (int64_t i = 0; i < min(A.shape().row, A.shape().col); ++i) {
    if (!internal::gaussJordanReduce<eps>(i, i, A, B...)) [[unlikely]] {
      return false;
    }
  }
  return true;
}

template <Pivot pivot, auto eps = 1e-10>
  requires(pivot == Pivot::kRow)
constexpr auto gaussJordan(MatrixT auto& A, MatrixT auto&... B) -> bool {
  CHECK(A.shape().row == A.shape().col);
  (CHECK(A.shape().row == B.shape().row), ...);

  auto perm = [&] {
    if constexpr (std::remove_reference_t<decltype(A)>::kRow == kDynamic) {
      return Permutation<kDynamic>{A.shape().row};
    } else {
      return Permutation<std::remove_reference_t<decltype(A)>::kRow>{};
    }
  }();

  for (int64_t i = 0; i < A.shape().row; ++i) {
    int64_t biggest = i;
    for (int64_t j = i; j < A.shape().col; ++j) {
      using std::abs;
      if (abs(A[perm.data()[j], i]) > abs(A[perm.data()[biggest], i])) {
        biggest = j;
      }
    }

    perm.swap(i, biggest);
    for (int64_t j = 0; j < A.shape().col; ++j) {
      std::swap(A[i, j], A[biggest, j]);
    }
    (
        [&] {
          for (int64_t j = 0; j < B.shape().col; ++j) {
            std::swap(B[i, j], B[biggest, j]);
          }
        }(),
        ...);

    if (!internal::gaussJordanReduce<eps>(i, i, A, B...)) [[unlikely]] {
      return false;
    }
  }

  // Unscramble the columns of A inverse. After all of the operations above,
  // we have x = A' P B, where P is the permutation matrix. To get the inverse,
  // we need to apply the permutation to the columns of A'. We can do this by
  // applying the permutation in reverse order to the columns.
  for (int64_t j = A.shape().col - 1; j >= 0; --j) {
    // Avoid double swapping
    if (perm.data()[j] == j) {
      continue;
    }
    for (int64_t i = 0; i < A.shape().row; ++i) {
      std::swap(A[i, j], A[i, perm.data()[j]]);
    }
    perm.swap(j, perm.data()[j]);
  }

  return true;
}

}  // namespace tempura::matrix
