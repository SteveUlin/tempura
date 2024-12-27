#include "matrix2/matrix.h"
#include "matrix2/storage/inline_dense.h"
#include "matrix2/storage/permutation.h"
#include "matrix2/storage/permuted.h"
#include "matrix2/to_string.h"

namespace tempura::matrix {

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
// forms the matrix U. W  create the matrix L by keeping track of these
// elementary row operations.
//
// As an optimization, LU<> stores L and U in the same matrix.
//
// Numerical Recipes 3rd Edition, Section 2.3

// Perturbation to avoid division by zero
constexpr auto safeDivide(auto a, auto b) {
  if (b == 0) {
    return a / 1e-30;
  }
  return a / b;
}

template <MatrixT M>
  requires(M::kRow == M::kCol or M::kRow == kDynamic or M::kCol == kDynamic)
class LU {
 public:
  using ChildT = M;

  constexpr explicit LU(const M& matrix) : matrix_{matrix} { init(); }

  constexpr explicit LU(M&& matrix) : matrix_{std::move(matrix)} { init(); }

  template <MatrixT B>
  constexpr void solve(B& b) const;

  constexpr auto determinant() const -> typename M::ValueType {
    auto det = matrix_[0, 0];
    for (int64_t i = 1; i < matrix_.shape().row; ++i) {
      det *= matrix_[i, i];
    }
    return det;
  }

  constexpr auto data() const -> const RowPermuted<M>& { return matrix_; }

 private:
  constexpr void init();

  RowPermuted<M> matrix_;
};

template <MatrixT M>
LU(const M&) -> LU<M>;

template <MatrixT M>
LU(M&&) -> LU<M>;

// LU decomposition with implicit pivoting
template <MatrixT M>
  requires(M::kRow == M::kCol or M::kRow == kDynamic or M::kCol == kDynamic)
constexpr void LU<M>::init() {
  CHECK(matrix_.shape().row == matrix_.shape().col);

  // To be scale invariant when pivoting, we divide each row by its largest
  // value. (We don't actually divide the matrix, but keep track of the scaling
  // terms.)
  auto scale = RowPermuted{InlineDense<typename M::ValueType, M::kRow, 1>{},
                           MatRef{matrix_.permutation()}};
  // Init the scale vector
  for (int64_t i = 0; i < matrix_.shape().row; ++i) {
    using std::abs;
    using std::max;
    scale[i] = abs(matrix_[i, 0]);
    for (int j = 1; j < matrix_.shape().col; ++j) {
      scale[i] = max(scale[i], abs(matrix_[i, j]));
    }
  }

  for (int64_t i = 0; i < matrix_.shape().row; ++i) {
    // Find the pivot row
    auto pivot_row = i;
    auto pivot_score = safeDivide(matrix_[i, i], scale[i]);
    for (int64_t ii = i + 1; ii < matrix_.shape().row; ++ii) {
      auto score = safeDivide(matrix_[ii, i], scale[ii]);
      if (score > pivot_score) {
        pivot_row = ii;
        pivot_score = score;
      }
    }
    matrix_.swap(i, pivot_row);

    for (int64_t j = i + 1; j < matrix_.shape().row; ++j) {
      matrix_[j, i] = safeDivide(matrix_[j, i], matrix_[i, i]);
      for (int64_t k = i + 1; k < matrix_.shape().col; ++k) {
        matrix_[j, k] -= matrix_[j, i] * matrix_[i, k];
      }
    }
  }
}

template <MatrixT M>
  requires(M::kRow == M::kCol or M::kRow == kDynamic or M::kCol == kDynamic)
template <MatrixT B>
constexpr void LU<M>::solve(B& b) const {
  CHECK(matrix_.shape().row == b.shape().row);
  matrix_.permutation().permuteRows(b);
  for (int64_t i = 1; i < matrix_.shape().row; ++i) {
    for (int64_t j = 0; j < i; ++j) {
      for (int64_t k = 0; k < b.shape().col; ++k) {
        b[i, k] -= matrix_[i, j] * b[j, k];
      }
    }
  }
  for (int64_t i = matrix_.shape().row - 1; i >= 0; --i) {
    for (int64_t j = i + 1; j < matrix_.shape().row; ++j) {
      for (int64_t k = 0; k < b.shape().col; ++k) {
        b[i, k] -= matrix_[i, j] * b[j, k];
      }
    }
    for (int64_t k = 0; k < b.shape().col; ++k) {
      b[i, k] = safeDivide(b[i, k], matrix_[i, i]);
    }
  }
}

}  // namespace tempura::matrix
