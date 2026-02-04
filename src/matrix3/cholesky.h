#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>

#include "matrix3/matrix.h"

namespace tempura::matrix3 {

// Cholesky Decomposition
// ----------------------
// Decomposes a symmetric positive-definite matrix A into:
//   A = L·Lᵀ
// where L is lower triangular with positive diagonal.
//
// Critical for Bayesian inference: MVN log-probability, covariance
// parameterization, and Riemannian HMC all depend on this.
//
// Uses Cholesky–Banachiewicz algorithm (row-by-row variant).
// Storage: L is stored in the lower triangle of matrix_ (upper triangle unused).
//
// No pivoting needed — SPD matrices are inherently numerically stable.
//
// Solving Ax = b becomes:
//   1. Forward substitution:  Ly = b
//   2. Backward substitution: Lᵀx = y

// Minimum value to clamp diagonal to when matrix is not positive definite.
// Allows graceful degradation while marking the decomposition as failed.
constexpr double kMinDiagonal = 1e-30;

template <typename MatrixType>
class Cholesky {
 public:
  using ValueType =
      std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit Cholesky(MatrixType matrix)
      : matrix_{std::move(matrix)}, positive_definite_{true} {
    init();
  }

  // Solve Ax = b in-place via L(Lᵀx) = b
  template <typename RhsMatrixType>
  constexpr void solve(RhsMatrixType& b) const {
    if (std::is_constant_evaluated()) {
      assert(matrix_.rows() == b.extent().extent(0) &&
             "matrix and RHS must have matching rows");
    }

    auto n = static_cast<int64_t>(matrix_.rows());
    auto num_cols = static_cast<int64_t>(b.extent().extent(1));

    // Forward substitution: Ly = b
    for (int64_t i = 0; i < n; ++i) {
      for (int64_t k = 0; k < num_cols; ++k) {
        for (int64_t j = 0; j < i; ++j) {
          b[i, k] -= matrix_[i, j] * b[j, k];
        }
        b[i, k] /= matrix_[i, i];
      }
    }

    // Backward substitution: Lᵀx = y
    for (int64_t i = n - 1; i >= 0; --i) {
      for (int64_t k = 0; k < num_cols; ++k) {
        for (int64_t j = i + 1; j < n; ++j) {
          b[i, k] -= matrix_[j, i] * b[j, k];  // Lᵀ[i,j] = L[j,i]
        }
        b[i, k] /= matrix_[i, i];
      }
    }
  }

  // L factor (lower triangular, stored in lower triangle of matrix_)
  constexpr auto factor() const -> const MatrixType& { return matrix_; }

  // log|A| = 2·Σ log(L_ii)
  // Numerically stable — avoids overflow from large determinants.
  constexpr auto logDeterminant() const -> ValueType {
    using std::log;
    ValueType sum{};
    for (std::size_t i = 0; i < matrix_.rows(); ++i) {
      sum += log(matrix_[i, i]);
    }
    return ValueType{2} * sum;
  }

  // det(A) = (Π L_ii)²
  constexpr auto determinant() const -> ValueType {
    ValueType prod{1};
    for (std::size_t i = 0; i < matrix_.rows(); ++i) {
      prod *= matrix_[i, i];
    }
    return prod * prod;
  }

  // A⁻¹ via solving A * X = I column by column
  constexpr auto inverse() const -> MatrixType {
    auto n = matrix_.rows();
    MatrixType result{};

    // Initialize result as identity
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        result[i, j] = (i == j) ? ValueType{1} : ValueType{0};
      }
    }

    solve(result);
    return result;
  }

  // Whether the input was positive definite
  constexpr auto isPositiveDefinite() const -> bool { return positive_definite_; }

 private:
  constexpr void init() {
    if (std::is_constant_evaluated()) {
      assert(matrix_.rows() == matrix_.cols() &&
             "Cholesky requires square matrix");
    }

    using std::sqrt;
    auto n = matrix_.rows();

    // Cholesky–Banachiewicz: row-by-row
    for (std::size_t i = 0; i < n; ++i) {
      // Off-diagonal elements: L[i,j] for j < i
      for (std::size_t j = 0; j < i; ++j) {
        ValueType sum = matrix_[i, j];
        for (std::size_t k = 0; k < j; ++k) {
          sum -= matrix_[i, k] * matrix_[j, k];
        }
        matrix_[i, j] = sum / matrix_[j, j];
      }

      // Diagonal element: L[i,i] = sqrt(A[i,i] - Σ L[i,k]²)
      ValueType diag = matrix_[i, i];
      for (std::size_t k = 0; k < i; ++k) {
        diag -= matrix_[i, k] * matrix_[i, k];
      }

      if (diag <= ValueType{0}) {
        // Matrix is not positive definite
        positive_definite_ = false;
        // Clamp to small positive value to avoid NaN and allow solve to proceed
        // (results will be garbage, but user should check isPositiveDefinite())
        matrix_[i, i] = static_cast<ValueType>(kMinDiagonal);
      } else {
        matrix_[i, i] = sqrt(diag);
      }

      // Zero out upper triangle (optional, for cleanliness)
      for (std::size_t j = i + 1; j < n; ++j) {
        matrix_[i, j] = ValueType{0};
      }
    }
  }

  MatrixType matrix_;  // stores L in lower triangle
  bool positive_definite_;
};

// Deduction guide
template <typename MatrixType>
Cholesky(MatrixType) -> Cholesky<std::remove_cvref_t<MatrixType>>;

}  // namespace tempura::matrix3
