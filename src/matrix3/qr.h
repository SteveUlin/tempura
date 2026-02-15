#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>
#include <type_traits>
#include <vector>

#include "matrix3/matrix.h"

namespace tempura::matrix3 {

// QR Decomposition via Householder Reflections
// ---------------------------------------------
// Decomposes square matrix A into:
//   A = Q·R
// where:
//   - Q is orthogonal (QᵀQ = I)
//   - R is upper triangular
//
// Foundation for eigendecomposition and least-squares problems.
//
// Storage: R occupies the upper triangle of matrix_. Householder vectors
// occupy the strict lower triangle (v₀ = 1 is implicit, not stored).
// τ coefficients stored separately in tau_.
//
// Solving Ax = b becomes:
//   1. Apply Qᵀ to b (via stored Householder reflections)
//   2. Back-substitution: Rx = Qᵀb

template <typename MatrixType>
class QR {
 public:
  using ValueType =
      std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit QR(MatrixType matrix)
      : matrix_{std::move(matrix)}, tau_(matrix_.rows()) {
    init();
  }

  // Solve Ax = b in-place (modifies b)
  template <typename RhsMatrixType>
  constexpr void solve(RhsMatrixType& b) const {
    if (std::is_constant_evaluated()) {
      assert(matrix_.rows() == b.extent().extent(0) &&
             "matrix and RHS must have matching rows");
    }

    auto n = static_cast<int64_t>(matrix_.rows());
    auto num_cols = static_cast<int64_t>(b.extent().extent(1));

    // Phase 1: Apply Qᵀ to b (forward order: H₀, H₁, ..., H_{n-1})
    for (int64_t j = 0; j < n; ++j) {
      if (tau_[j] == ValueType{0}) continue;

      for (int64_t k = 0; k < num_cols; ++k) {
        // dot = v·b where v₀ = 1 (implicit)
        auto dot = b[j, k];
        for (int64_t i = j + 1; i < n; ++i) {
          dot += matrix_[i, j] * b[i, k];
        }
        // Apply reflection: b -= τ·dot·v
        b[j, k] -= tau_[j] * dot;
        for (int64_t i = j + 1; i < n; ++i) {
          b[i, k] -= tau_[j] * dot * matrix_[i, j];
        }
      }
    }

    // Phase 2: Back-substitution Rx = Qᵀb
    for (int64_t i = n - 1; i >= 0; --i) {
      for (int64_t k = 0; k < num_cols; ++k) {
        for (int64_t j = i + 1; j < n; ++j) {
          b[i, k] -= matrix_[i, j] * b[j, k];
        }
        b[i, k] /= matrix_[i, i];
      }
    }
  }

  // Materialize Q by applying reflections in reverse to identity
  constexpr auto Q() const -> MatrixType {
    auto n = matrix_.rows();
    MatrixType result{};

    // Initialize as identity
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        result[i, j] = (i == j) ? ValueType{1} : ValueType{0};
      }
    }

    // Q = H₀ H₁ ... H_{n-1}, built by applying H_{n-1}, ..., H₀ to I
    for (int64_t j = static_cast<int64_t>(n) - 1; j >= 0; --j) {
      if (tau_[j] == ValueType{0}) continue;

      for (std::size_t k = static_cast<std::size_t>(j); k < n; ++k) {
        // dot = vᵀ · Q[:,k] where v₀ = 1
        auto dot = result[j, k];
        for (int64_t i = j + 1; i < static_cast<int64_t>(n); ++i) {
          dot += matrix_[i, j] * result[i, k];
        }
        // Q[:,k] -= τ·dot·v
        result[j, k] -= tau_[j] * dot;
        for (int64_t i = j + 1; i < static_cast<int64_t>(n); ++i) {
          result[i, k] -= tau_[j] * dot * matrix_[i, j];
        }
      }
    }

    return result;
  }

  // Materialize R (upper triangle from packed storage)
  constexpr auto R() const -> MatrixType {
    auto n = matrix_.rows();
    MatrixType result{};

    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        result[i, j] = (j >= i) ? matrix_[i, j] : ValueType{0};
      }
    }

    return result;
  }

  // Access packed storage
  constexpr auto data() const -> const MatrixType& { return matrix_; }

 private:
  constexpr void init() {
    if (std::is_constant_evaluated()) {
      assert(matrix_.rows() == matrix_.cols() && "QR requires square matrix");
    }

    using std::sqrt;
    auto n = static_cast<int64_t>(matrix_.rows());

    for (int64_t j = 0; j < n; ++j) {
      // Compute ‖x‖² for subdiagonal vector x = A[j:n, j]
      auto norm_sq = ValueType{0};
      for (int64_t i = j; i < n; ++i) {
        norm_sq += matrix_[i, j] * matrix_[i, j];
      }
      auto norm = sqrt(norm_sq);

      if (norm == ValueType{0}) {
        tau_[j] = ValueType{0};
        continue;
      }

      // α chosen to avoid catastrophic cancellation in x₀ - α
      auto alpha = (matrix_[j, j] >= ValueType{0}) ? -norm : norm;
      tau_[j] = (alpha - matrix_[j, j]) / alpha;
      auto denom = matrix_[j, j] - alpha;

      // Store normalized Householder vector below diagonal
      for (int64_t i = j + 1; i < n; ++i) {
        matrix_[i, j] /= denom;
      }
      matrix_[j, j] = alpha;  // R diagonal element

      // Apply reflection to trailing columns: A -= τ·v·(vᵀ·A)
      for (int64_t k = j + 1; k < n; ++k) {
        auto dot = matrix_[j, k];  // v₀ = 1
        for (int64_t i = j + 1; i < n; ++i) {
          dot += matrix_[i, j] * matrix_[i, k];
        }
        matrix_[j, k] -= tau_[j] * dot;
        for (int64_t i = j + 1; i < n; ++i) {
          matrix_[i, k] -= tau_[j] * dot * matrix_[i, j];
        }
      }
    }
  }

  MatrixType matrix_;                // R in upper triangle, Householder vectors below
  std::vector<ValueType> tau_;       // Householder coefficients (length n)
};

// Deduction guide
template <typename MatrixType>
QR(MatrixType) -> QR<std::remove_cvref_t<MatrixType>>;

}  // namespace tempura::matrix3
