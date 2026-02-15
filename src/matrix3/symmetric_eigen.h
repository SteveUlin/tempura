#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

#include "matrix3/matrix.h"

namespace tempura::matrix3 {

// Symmetric Eigendecomposition
// ----------------------------
// Decomposes a symmetric matrix A into:
//   A = V·D·Vᵀ
// where:
//   - V is orthogonal (columns are eigenvectors)
//   - D is diagonal (entries are eigenvalues, sorted ascending)
//
// Algorithm:
//   1. Householder tridiagonal reduction: A → QTQᵀ
//   2. Implicit QL iteration with Wilkinson shift on T
//   3. Eigenvector accumulation: V = Q · (product of Givens rotations)
//
// QL (vs QR) sweeps from bottom-right, giving better convergence
// for the smallest eigenvalues. Wilkinson shift ensures cubic convergence.

template <typename MatrixType>
class SymmetricEigen {
 public:
  using ValueType =
      std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit SymmetricEigen(MatrixType matrix)
      : eigenvectors_{std::move(matrix)},
        eigenvalues_(eigenvectors_.rows()),
        off_diag_(eigenvectors_.rows()),
        tau_(eigenvectors_.rows()) {
    init();
  }

  // Eigenvalues sorted ascending
  constexpr auto eigenvalues() const -> const std::vector<ValueType>& {
    return eigenvalues_;
  }

  // Orthogonal matrix whose columns are eigenvectors (column i ↔ eigenvalue i)
  constexpr auto eigenvectors() const -> const MatrixType& {
    return eigenvectors_;
  }

 private:
  constexpr void init() {
    if (std::is_constant_evaluated()) {
      assert(eigenvectors_.rows() == eigenvectors_.cols() &&
             "SymmetricEigen requires square matrix");
    }
    tridiagonalize();
    formQ();
    qlIteration();
    sort();
  }

  // Householder reduction to tridiagonal form.
  // On exit: eigenvalues_ holds diagonal, off_diag_ holds subdiagonal,
  // Householder vectors stored in lower triangle of eigenvectors_.
  constexpr void tridiagonalize() {
    using std::sqrt;
    auto n = static_cast<int64_t>(eigenvectors_.rows());

    auto p = std::vector<ValueType>(n);

    for (int64_t j = 0; j < n - 2; ++j) {
      // Householder reflection from subcolumn A[j+1:n, j]
      auto norm_sq = ValueType{0};
      for (int64_t i = j + 1; i < n; ++i) {
        norm_sq += eigenvectors_[i, j] * eigenvectors_[i, j];
      }
      auto norm = sqrt(norm_sq);

      if (norm == ValueType{0}) {
        tau_[j] = ValueType{0};
        off_diag_[j] = ValueType{0};
        continue;
      }

      // α chosen so x₀ - α has large magnitude (avoids cancellation)
      auto alpha =
          (eigenvectors_[j + 1, j] >= ValueType{0}) ? -norm : norm;
      tau_[j] = (alpha - eigenvectors_[j + 1, j]) / alpha;
      auto denom = eigenvectors_[j + 1, j] - alpha;

      // Normalize Householder vector: v₀ = 1, v[i] = x[i]/(x₀ - α)
      for (int64_t i = j + 2; i < n; ++i) {
        eigenvectors_[i, j] /= denom;
      }
      eigenvectors_[j + 1, j] = ValueType{1};
      off_diag_[j] = alpha;

      // Similarity transform: A ← HAH where H = I - τvvᵀ
      //   p = τ · A_sub · v
      //   K = (τ/2) · vᵀ · p
      //   q = p - K · v
      //   A_sub -= v·qᵀ + q·vᵀ
      auto m = n - j - 1;

      for (int64_t i = 0; i < m; ++i) {
        ValueType sum{0};
        for (int64_t k = 0; k < m; ++k) {
          sum += eigenvectors_[j + 1 + i, j + 1 + k] *
                 eigenvectors_[j + 1 + k, j];
        }
        p[i] = tau_[j] * sum;
      }

      ValueType K{0};
      for (int64_t i = 0; i < m; ++i) {
        K += eigenvectors_[j + 1 + i, j] * p[i];
      }
      K *= tau_[j] / ValueType{2};

      // q = p - K·v (reuse p for q)
      for (int64_t i = 0; i < m; ++i) {
        p[i] -= K * eigenvectors_[j + 1 + i, j];
      }

      // Symmetric rank-2 update on trailing submatrix
      for (int64_t i = 0; i < m; ++i) {
        for (int64_t k = 0; k <= i; ++k) {
          auto update = eigenvectors_[j + 1 + i, j] * p[k] +
                        p[i] * eigenvectors_[j + 1 + k, j];
          eigenvectors_[j + 1 + i, j + 1 + k] -= update;
          if (i != k) {
            eigenvectors_[j + 1 + k, j + 1 + i] -= update;
          }
        }
      }
    }

    // Extract diagonal and last off-diagonal
    for (int64_t i = 0; i < n; ++i) {
      eigenvalues_[i] = eigenvectors_[i, i];
    }
    if (n > 1) {
      off_diag_[n - 2] = eigenvectors_[n - 1, n - 2];
    }
    if (n > 0) {
      off_diag_[n - 1] = ValueType{0};
    }
  }

  // Build orthogonal Q from stored Householder vectors
  constexpr void formQ() {
    auto n = eigenvectors_.rows();
    MatrixType Q{};

    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        Q[i, j] = (i == j) ? ValueType{1} : ValueType{0};
      }
    }

    // Q = H₀ H₁ ... H_{n-3}, built by applying from the right: I·H_{n-3}·...·H₀
    // Equivalent to left-multiplying in reverse order.
    for (int64_t j = static_cast<int64_t>(n) - 3; j >= 0; --j) {
      if (tau_[j] == ValueType{0}) continue;

      for (std::size_t k = static_cast<std::size_t>(j + 1); k < n; ++k) {
        auto dot = Q[j + 1, k];
        for (int64_t i = j + 2; i < static_cast<int64_t>(n); ++i) {
          dot += eigenvectors_[i, j] * Q[i, k];
        }
        Q[j + 1, k] -= tau_[j] * dot;
        for (int64_t i = j + 2; i < static_cast<int64_t>(n); ++i) {
          Q[i, k] -= tau_[j] * dot * eigenvectors_[i, j];
        }
      }
    }

    eigenvectors_ = std::move(Q);
  }

  // Implicit QL iteration with Wilkinson shift on tridiagonal (eigenvalues_, off_diag_).
  // Accumulates Givens rotations into eigenvectors_.
  constexpr void qlIteration() {
    using std::abs;
    using std::hypot;

    auto n = static_cast<int64_t>(eigenvalues_.size());
    if (n <= 1) return;

    constexpr int kMaxIter = 30;

    for (int64_t l = 0; l < n; ++l) {
      int iter = 0;

      for (;;) {
        // Find smallest m ≥ l where off_diag_[m] is negligible
        int64_t m = l;
        for (; m < n - 1; ++m) {
          auto dd = abs(eigenvalues_[m]) + abs(eigenvalues_[m + 1]);
          if (dd + abs(off_diag_[m]) == dd) break;
        }

        if (m == l) break;

        (void)kMaxIter;
        assert(iter++ < kMaxIter && "QL iteration failed to converge");

        // Wilkinson shift from 2×2 block at (l, l+1)
        auto g = (eigenvalues_[l + 1] - eigenvalues_[l]) /
                 (ValueType{2} * off_diag_[l]);
        auto r = hypot(g, ValueType{1});
        // sign(g)·r avoids cancellation in denominator
        g = eigenvalues_[m] - eigenvalues_[l] +
            off_diag_[l] / (g + (g >= ValueType{0} ? r : -r));

        auto s = ValueType{1};
        auto c = ValueType{1};
        auto p = ValueType{0};

        bool underflow = false;
        for (int64_t i = m - 1; i >= l; --i) {
          auto f = s * off_diag_[i];
          auto b = c * off_diag_[i];
          r = hypot(f, g);
          off_diag_[i + 1] = r;

          if (r == ValueType{0}) {
            // Underflow — undo partial update and retry
            eigenvalues_[i + 1] -= p;
            off_diag_[m] = ValueType{0};
            underflow = true;
            break;
          }

          s = f / r;
          c = g / r;
          g = eigenvalues_[i + 1] - p;
          r = (eigenvalues_[i] - g) * s + ValueType{2} * c * b;
          p = s * r;
          eigenvalues_[i + 1] = g + p;
          g = c * r - b;

          // Accumulate Givens rotation into eigenvectors
          for (int64_t k = 0; k < n; ++k) {
            f = eigenvectors_[k, i + 1];
            eigenvectors_[k, i + 1] = s * eigenvectors_[k, i] + c * f;
            eigenvectors_[k, i] = c * eigenvectors_[k, i] - s * f;
          }
        }

        if (underflow) continue;

        eigenvalues_[l] -= p;
        off_diag_[l] = g;
        off_diag_[m] = ValueType{0};
      }
    }
  }

  // Sort eigenvalues ascending, reorder eigenvector columns to match
  constexpr void sort() {
    auto n = eigenvalues_.size();
    if (n <= 1) return;

    for (std::size_t i = 0; i < n - 1; ++i) {
      auto min_idx = i;
      for (std::size_t j = i + 1; j < n; ++j) {
        if (eigenvalues_[j] < eigenvalues_[min_idx]) {
          min_idx = j;
        }
      }
      if (min_idx != i) {
        std::swap(eigenvalues_[i], eigenvalues_[min_idx]);
        for (std::size_t k = 0; k < n; ++k) {
          std::swap(eigenvectors_[k, i], eigenvectors_[k, min_idx]);
        }
      }
    }
  }

  MatrixType eigenvectors_;
  std::vector<ValueType> eigenvalues_;
  std::vector<ValueType> off_diag_;
  std::vector<ValueType> tau_;
};

// Deduction guide
template <typename MatrixType>
SymmetricEigen(MatrixType) -> SymmetricEigen<std::remove_cvref_t<MatrixType>>;

}  // namespace tempura::matrix3
