#pragma once

#include "matrix/matrix.h"

// LU decomposition
//
// LU decomposition splits a square matrix into two triangular matrices, M = LU.
//   - L is a lower triangular matrix with 1s on the diagonal
//   - U is an upper triangular matrix
//
// We can relatively easily solve a Triangular matrix equation, Tx = b, with the
// help of a few for loops. By splitting M into L and U, we can solve the system
// of equations LUx = b by solving:
//   - Ly = b (the forward substitution)
//   - Ux = y (the backward substitution)
//
// We calculate L and U with a form of Gaussian Elimination. Use elementary
// row operations to zero out the columns under each diagonal element. This
// forms the matrix U. By keeping track of these elementary row operations, we
// create the matrix L.
//
// As an optimization, LU<> stores L and U in the same matrix.

namespace tempura::matrix {

template <MatrixT M, Pivoting pivot = Pivoting::Partial>
  requires(M::kExtent.row == kDynamic || M::kExtent.col == kDynamic ||
           M::kExtent.row == M::kExtent.col)
class LU;

template <MatrixT M>
  requires(M::kExtent.row == kDynamic || M::kExtent.col == kDynamic ||
           M::kExtent.row == M::kExtent.col)
class LU<M, Pivoting::Partial> final {
  constexpr static RowCol kExtent =
      (M::kExtent.row != kDynamic) ? RowCol{M::kExtent.row, M::kExtent.row}
                                   : RowCol{M::kExtent.col, M::kExtent.col};

  explicit LU(M matrix) : matrix_(std::move(matrix)) {
    CHECK(matrix_.shape().row == matrix_.shape().col);

    const size_t n = size();
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = i + 1; j < n; ++j) {
        matrix_[j, i] /= matrix_[i, i];
        for (size_t k = i + 1; k < n; ++k) {
          matrix_[j, k] -= matrix_[j, i] * matrix_[i, k];
        }
      }
    }
  }

  auto size() const -> size_t { return matrix_.shape().row; }

  auto matrix() const -> const M& { return matrix; }

  auto determinant() const {
    CHECK(size() == matrix_.shape().col);
    auto det = matrix_[0, 0];
    for (size_t i = 1; i < size(); ++i) {
      det *= matrix_[i, i];
    }
    return det;
  }

  template <MatrixT B>
    requires(matchingExtent({kExtent.row, 1}, B::kExtent))
  auto solve(B b) const -> B {
    return solveLU(matrix_, b);
  }

 private:
  M matrix_;
};

}  // namespace tempura::matrix
