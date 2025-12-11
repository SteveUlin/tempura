#pragma once

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "matrix3/matrix.h"
#include "matrix3/permuted.h"

namespace tempura::matrix3 {

// LU Decomposition
// ----------------
// Numerical Recipes 3rd Edition, Section 2.3
//
// Decomposes square matrix A into lower and upper triangular matrices:
//   P·A = L·U
// where:
//   - L is lower triangular with 1s on diagonal
//   - U is upper triangular
//   - P is a permutation matrix (stored implicitly)
//
// Uses scale-invariant partial pivoting for numerical stability.
// Storage: L and U share the same matrix (L below diagonal, U on and above).
//
// Solving Ax = b becomes:
//   1. Forward substitution: Ly = Pb
//   2. Backward substitution: Ux = y

// Perturbation to avoid division by zero
constexpr auto safeDivide(auto a, auto b) {
  if (b == 0) {
    return a / 1e-30;
  }
  return a / b;
}

template <typename MatrixType>
class LU {
 public:
  using ValueType = std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit LU(MatrixType matrix)
      : matrix_{RowPermuted{std::move(matrix)}} {
    init();
  }

  // Solve Ax = b in-place (modifies b)
  template <typename RhsMatrixType>
  constexpr void solve(RhsMatrixType& b) const {
    if (std::is_constant_evaluated()) {
      assert(matrix_.rows() == b.extent().extent(0) &&
             "matrix and RHS must have matching rows");
    }

    // Apply permutation to b
    matrix_.permutation().permuteRows(b);

    // Forward substitution: Ly = Pb
    for (int64_t i = 1; i < static_cast<int64_t>(matrix_.rows()); ++i) {
      for (int64_t j = 0; j < i; ++j) {
        for (int64_t k = 0; k < static_cast<int64_t>(b.extent().extent(1)); ++k) {
          b[i, k] -= matrix_[i, j] * b[j, k];
        }
      }
    }

    // Backward substitution: Ux = y
    for (int64_t i = static_cast<int64_t>(matrix_.rows()) - 1; i >= 0; --i) {
      for (int64_t j = i + 1; j < static_cast<int64_t>(matrix_.rows()); ++j) {
        for (int64_t k = 0; k < static_cast<int64_t>(b.extent().extent(1)); ++k) {
          b[i, k] -= matrix_[i, j] * b[j, k];
        }
      }
      for (int64_t k = 0; k < static_cast<int64_t>(b.extent().extent(1)); ++k) {
        b[i, k] = safeDivide(b[i, k], matrix_[i, i]);
      }
    }
  }

  // Determinant from product of diagonal elements
  constexpr auto determinant() const -> ValueType {
    auto det = matrix_[0, 0];
    for (int64_t i = 1; i < static_cast<int64_t>(matrix_.rows()); ++i) {
      det *= matrix_[i, i];
    }
    return det;
  }

  // Access decomposed matrix (L below diagonal, U on/above diagonal)
  constexpr auto data() const -> const auto& { return matrix_; }

 private:
  constexpr void init() {
    if (std::is_constant_evaluated()) {
      assert(matrix_.rows() == matrix_.cols() && "LU requires square matrix");
    }

    // Scale-invariant pivoting: track largest element in each row
    auto scale_data = std::vector<ValueType>(matrix_.rows());
    for (std::size_t i = 0; i < matrix_.rows(); ++i) {
      using std::abs;
      using std::max;
      scale_data[i] = abs(matrix_[i, 0]);
      for (std::size_t j = 1; j < matrix_.cols(); ++j) {
        scale_data[i] = max(scale_data[i], abs(matrix_[i, j]));
      }
    }

    // Decomposition with scale-invariant partial pivoting
    for (std::size_t i = 0; i < matrix_.rows(); ++i) {
      // Find pivot row: largest scaled element in column i
      std::size_t pivot_row = i;
      using std::abs;
      auto pivot_score = safeDivide(abs(matrix_[i, i]), scale_data[i]);

      for (std::size_t ii = i + 1; ii < matrix_.rows(); ++ii) {
        auto score = safeDivide(abs(matrix_[ii, i]), scale_data[ii]);
        if (score > pivot_score) {
          pivot_row = ii;
          pivot_score = score;
        }
      }

      // Swap rows via permutation
      matrix_.swap(i, pivot_row);
      std::swap(scale_data[i], scale_data[pivot_row]);

      // Gaussian elimination: zero out column below pivot
      for (std::size_t j = i + 1; j < matrix_.rows(); ++j) {
        matrix_[j, i] = safeDivide(matrix_[j, i], matrix_[i, i]);
        for (std::size_t k = i + 1; k < matrix_.cols(); ++k) {
          matrix_[j, k] -= matrix_[j, i] * matrix_[i, k];
        }
      }
    }
  }

  RowPermuted<MatrixType> matrix_;
};

// Deduction guide
template <typename MatrixType>
LU(MatrixType) -> LU<std::remove_cvref_t<MatrixType>>;

}  // namespace tempura::matrix3
