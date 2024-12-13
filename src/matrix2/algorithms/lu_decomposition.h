#include "matrix2/matrix.h"
#include "matrix2/storage/permutation.h"
#include "matrix2/to_string.h"
#include "matrix2/views/permuted.h"

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
// forms the matrix U. By keeping track of these elementary row operations, we
// create the matrix L.
//
// As an optimization, LU<> stores L and U in the same matrix.
//
// Numerical Recipes 3rd Edition, Section 2.3

template <MatrixT M>
class LU {
 public:
  using Child = M;

  // If possible, LU will take ownership of the argument.
  template <typename T>
    requires(MatrixT<std::remove_reference_t<T>>)
  explicit LU(T&& matrix);

 private:
  RowPermuted<M> matrix_;
};

// LU decomposition with implicit pivoting
template <MatrixT M>
template <typename T>
  requires(MatrixT<std::remove_reference_t<T>>)
LU<M>::LU(T&& matrix) : matrix_(std::forward<T>(matrix)) {
  const int64_t n = matrix_.shape().row;

  // To be scale invariant when pivoting, we divide each row by its largest
  // value. We don't actually divide the matrix, but keep track of the scaling
  // terms.
  auto scale = [&] {
    if constexpr (Child::kExtent == kDynamic) {
      return std::vector<typename M::ValueType>(matrix_.shape().row);
    } else {
      return std::array<typename M::ValueType, M::kExtent.row>{};
    }
  }();


  CHECK(matrix_.shape().row == matrix_.shape().col);

  for (int64_t i = 0; i < n; ++i) {
    for (int64_t j = i + 1; j < n; ++j) {
      matrix_[j, i] /= matrix_[i, i];
      for (int64_t k = i + 1; k < n; ++k) {
        matrix_[j, k] -= matrix_[j, i] * matrix_[i, k];
      }
    }
  }
}

}  // namespace tempura::matrix
