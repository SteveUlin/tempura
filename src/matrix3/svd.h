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

// Singular Value Decomposition (Golub-Kahan)
// ------------------------------------------
// Decomposes a square matrix A into:
//   A = U·Σ·Vᵀ
// where:
//   - U is orthogonal (left singular vectors, columns)
//   - Σ is diagonal with non-negative entries (singular values, descending)
//   - V is orthogonal (right singular vectors, columns)
//
// Algorithm:
//   1. Golub-Kahan bidiagonalization: A = U₁·B·V₁ᵀ (Householder from both sides)
//   2. Implicit QR iteration on bidiagonal B with Wilkinson shift
//   3. Accumulate Givens rotations into U₁, V₁
//
// Unlike eigendecomposition of AᵀA, this avoids squaring the condition number.

template <typename MatrixType>
class SVD {
 public:
  using ValueType =
      std::remove_cvref_t<decltype(std::declval<MatrixType>()[0, 0])>;

  constexpr explicit SVD(MatrixType matrix)
      : u_{std::move(matrix)},
        v_{},
        diag_(u_.rows()),
        off_diag_(u_.rows()),
        tau_left_(u_.rows()),
        tau_right_(u_.rows()) {
    init();
  }

  constexpr auto U() const -> const MatrixType& { return u_; }
  constexpr auto V() const -> const MatrixType& { return v_; }
  constexpr auto singularValues() const -> const std::vector<ValueType>& {
    return diag_;
  }

  // Solve Ax = b in-place via x = V·Σ⁻¹·Uᵀ·b
  template <typename RhsMatrixType>
  constexpr void solve(RhsMatrixType& b) const {
    auto n = static_cast<int64_t>(u_.rows());
    auto num_cols = static_cast<int64_t>(b.extent().extent(1));
    auto temp = std::vector<ValueType>(n);

    for (int64_t col = 0; col < num_cols; ++col) {
      // temp = Σ⁻¹ · Uᵀ · b[:,col]
      for (int64_t i = 0; i < n; ++i) {
        ValueType sum{0};
        for (int64_t j = 0; j < n; ++j) {
          sum += u_[j, i] * b[j, col];
        }
        temp[i] = sum / diag_[i];
      }
      // b[:,col] = V · temp
      for (int64_t i = 0; i < n; ++i) {
        ValueType sum{0};
        for (int64_t j = 0; j < n; ++j) {
          sum += v_[i, j] * temp[j];
        }
        b[i, col] = sum;
      }
    }
  }

 private:
  constexpr void init() {
    if (std::is_constant_evaluated()) {
      assert(u_.rows() == u_.cols() && "SVD requires square matrix");
    }
    bidiagonalize();
    formV();
    formU();
    svdIteration();
    ensurePositive();
    sort();
  }

  // Golub-Kahan bidiagonalization: alternating left/right Householder reflections.
  // On exit: u_ holds packed Householder vectors (left below diagonal, right above
  // superdiagonal), diag_/off_diag_ hold the bidiagonal elements.
  constexpr void bidiagonalize() {
    using std::sqrt;
    auto n = static_cast<int64_t>(u_.rows());

    for (int64_t j = 0; j < n; ++j) {
      // Left Householder: zero u_[j+1:n, j]
      {
        auto norm_sq = ValueType{0};
        for (int64_t i = j; i < n; ++i) {
          norm_sq += u_[i, j] * u_[i, j];
        }
        auto norm = sqrt(norm_sq);

        if (norm != ValueType{0}) {
          auto alpha = (u_[j, j] >= ValueType{0}) ? -norm : norm;
          tau_left_[j] = (alpha - u_[j, j]) / alpha;
          auto denom = u_[j, j] - alpha;

          for (int64_t i = j + 1; i < n; ++i) {
            u_[i, j] /= denom;
          }
          u_[j, j] = alpha;

          // Apply H_L to trailing columns
          for (int64_t k = j + 1; k < n; ++k) {
            auto dot = u_[j, k];
            for (int64_t i = j + 1; i < n; ++i) {
              dot += u_[i, j] * u_[i, k];
            }
            u_[j, k] -= tau_left_[j] * dot;
            for (int64_t i = j + 1; i < n; ++i) {
              u_[i, k] -= tau_left_[j] * dot * u_[i, j];
            }
          }
        } else {
          tau_left_[j] = ValueType{0};
        }
      }

      // Right Householder: zero u_[j, j+2:n]
      if (j < n - 2) {
        auto norm_sq = ValueType{0};
        for (int64_t k = j + 1; k < n; ++k) {
          norm_sq += u_[j, k] * u_[j, k];
        }
        auto norm = sqrt(norm_sq);

        if (norm != ValueType{0}) {
          auto alpha = (u_[j, j + 1] >= ValueType{0}) ? -norm : norm;
          tau_right_[j] = (alpha - u_[j, j + 1]) / alpha;
          auto denom = u_[j, j + 1] - alpha;

          for (int64_t k = j + 2; k < n; ++k) {
            u_[j, k] /= denom;
          }
          u_[j, j + 1] = alpha;

          // Apply H_R to trailing rows (right multiplication)
          for (int64_t i = j + 1; i < n; ++i) {
            auto dot = u_[i, j + 1];
            for (int64_t k = j + 2; k < n; ++k) {
              dot += u_[i, k] * u_[j, k];
            }
            u_[i, j + 1] -= tau_right_[j] * dot;
            for (int64_t k = j + 2; k < n; ++k) {
              u_[i, k] -= tau_right_[j] * dot * u_[j, k];
            }
          }
        } else {
          tau_right_[j] = ValueType{0};
        }
      }
    }

    // Extract bidiagonal elements
    for (int64_t i = 0; i < n; ++i) {
      diag_[i] = u_[i, i];
    }
    for (int64_t i = 0; i < n - 1; ++i) {
      off_diag_[i] = u_[i, i + 1];
    }
    if (n > 0) off_diag_[n - 1] = ValueType{0};
  }

  // Build V from right Householder vectors (stored in rows of u_, above superdiagonal)
  constexpr void formV() {
    auto n = u_.rows();
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        v_[i, j] = (i == j) ? ValueType{1} : ValueType{0};
      }
    }

    for (int64_t j = static_cast<int64_t>(n) - 3; j >= 0; --j) {
      if (tau_right_[j] == ValueType{0}) continue;

      for (std::size_t k = static_cast<std::size_t>(j + 1); k < n; ++k) {
        auto dot = v_[j + 1, k];
        for (int64_t i = j + 2; i < static_cast<int64_t>(n); ++i) {
          dot += u_[j, i] * v_[i, k];
        }
        v_[j + 1, k] -= tau_right_[j] * dot;
        for (int64_t i = j + 2; i < static_cast<int64_t>(n); ++i) {
          v_[i, k] -= tau_right_[j] * dot * u_[j, i];
        }
      }
    }
  }

  // Build U from left Householder vectors (stored in columns of u_, below diagonal)
  constexpr void formU() {
    auto n = u_.rows();
    MatrixType U{};
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        U[i, j] = (i == j) ? ValueType{1} : ValueType{0};
      }
    }

    for (int64_t j = static_cast<int64_t>(n) - 1; j >= 0; --j) {
      if (tau_left_[j] == ValueType{0}) continue;

      for (std::size_t k = static_cast<std::size_t>(j); k < n; ++k) {
        auto dot = U[j, k];
        for (int64_t i = j + 1; i < static_cast<int64_t>(n); ++i) {
          dot += u_[i, j] * U[i, k];
        }
        U[j, k] -= tau_left_[j] * dot;
        for (int64_t i = j + 1; i < static_cast<int64_t>(n); ++i) {
          U[i, k] -= tau_left_[j] * dot * u_[i, j];
        }
      }
    }

    u_ = std::move(U);
  }

  // Implicit QR iteration on bidiagonal matrix, accumulating Givens into U and V.
  constexpr void svdIteration() {
    using std::abs;
    using std::hypot;

    auto n = static_cast<int64_t>(diag_.size());
    if (n <= 1) return;

    constexpr int kMaxTotalIter = 100;

    for (int total_iter = 0; total_iter < kMaxTotalIter * static_cast<int>(n);
         ++total_iter) {
      // Find largest unreduced block [p..q]: scan from bottom for non-negligible e
      int64_t q = n - 1;
      while (q > 0) {
        auto dd = abs(diag_[q - 1]) + abs(diag_[q]);
        if (dd + abs(off_diag_[q - 1]) != dd) break;
        off_diag_[q - 1] = ValueType{0};
        --q;
      }
      if (q == 0) break;  // Fully converged

      int64_t p = q - 1;
      while (p > 0) {
        auto dd = abs(diag_[p - 1]) + abs(diag_[p]);
        if (dd + abs(off_diag_[p - 1]) == dd) {
          off_diag_[p - 1] = ValueType{0};
          break;
        }
        --p;
      }

      svdStep(p, q, n);
    }
  }

  // One Golub-Kahan SVD step on unreduced bidiagonal block [p..q]
  constexpr void svdStep(int64_t p, int64_t q, int64_t n) {
    using std::abs;
    using std::hypot;

    // Wilkinson shift from trailing 2×2 of BᵀB
    auto d_qm1 = diag_[q - 1];
    auto e_qm1 = off_diag_[q - 1];
    auto d_q = diag_[q];

    auto t_nn = d_q * d_q + e_qm1 * e_qm1;
    auto t_nm = d_qm1 * e_qm1;
    auto t_mm = d_qm1 * d_qm1;
    if (q - 2 >= p) {
      t_mm += off_diag_[q - 2] * off_diag_[q - 2];
    }

    auto delta = (t_mm - t_nn) / ValueType{2};
    auto sign_d = (delta >= ValueType{0}) ? ValueType{1} : ValueType{-1};
    auto denom = delta + sign_d * hypot(delta, t_nm);
    auto mu = (denom == ValueType{0}) ? ValueType{0}
                                      : t_nn - t_nm * t_nm / denom;

    auto y = diag_[p] * diag_[p] - mu;
    auto z = diag_[p] * off_diag_[p];

    for (int64_t k = p; k < q; ++k) {
      // Right Givens: zero z by mixing columns k, k+1
      auto r = hypot(y, z);
      auto c_r = (r == ValueType{0}) ? ValueType{1} : y / r;
      auto s_r = (r == ValueType{0}) ? ValueType{0} : z / r;

      if (k > p) off_diag_[k - 1] = r;

      auto f = c_r * diag_[k] + s_r * off_diag_[k];
      off_diag_[k] = -s_r * diag_[k] + c_r * off_diag_[k];
      auto g = s_r * diag_[k + 1];
      diag_[k + 1] *= c_r;

      // Accumulate V
      for (int64_t i = 0; i < n; ++i) {
        auto t = c_r * v_[i, k] + s_r * v_[i, k + 1];
        v_[i, k + 1] = -s_r * v_[i, k] + c_r * v_[i, k + 1];
        v_[i, k] = t;
      }

      // Left Givens: zero g (bulge at row k+1, col k)
      r = hypot(f, g);
      auto c_l = (r == ValueType{0}) ? ValueType{1} : f / r;
      auto s_l = (r == ValueType{0}) ? ValueType{0} : g / r;

      diag_[k] = r;

      f = c_l * off_diag_[k] + s_l * diag_[k + 1];
      diag_[k + 1] = -s_l * off_diag_[k] + c_l * diag_[k + 1];
      off_diag_[k] = f;

      if (k + 1 < q) {
        g = s_l * off_diag_[k + 1];
        off_diag_[k + 1] *= c_l;
        y = off_diag_[k];
        z = g;
      }

      // Accumulate U
      for (int64_t i = 0; i < n; ++i) {
        auto t = c_l * u_[i, k] + s_l * u_[i, k + 1];
        u_[i, k + 1] = -s_l * u_[i, k] + c_l * u_[i, k + 1];
        u_[i, k] = t;
      }
    }
  }

  // Make all singular values non-negative by flipping U column signs
  constexpr void ensurePositive() {
    auto n = diag_.size();
    for (std::size_t i = 0; i < n; ++i) {
      if (diag_[i] < ValueType{0}) {
        diag_[i] = -diag_[i];
        for (std::size_t k = 0; k < n; ++k) {
          u_[k, i] = -u_[k, i];
        }
      }
    }
  }

  // Sort singular values descending, reorder U and V columns to match
  constexpr void sort() {
    auto n = diag_.size();
    if (n <= 1) return;

    for (std::size_t i = 0; i < n - 1; ++i) {
      auto max_idx = i;
      for (std::size_t j = i + 1; j < n; ++j) {
        if (diag_[j] > diag_[max_idx]) max_idx = j;
      }
      if (max_idx != i) {
        std::swap(diag_[i], diag_[max_idx]);
        for (std::size_t k = 0; k < n; ++k) {
          std::swap(u_[k, i], u_[k, max_idx]);
          std::swap(v_[k, i], v_[k, max_idx]);
        }
      }
    }
  }

  MatrixType u_;
  MatrixType v_;
  std::vector<ValueType> diag_;
  std::vector<ValueType> off_diag_;
  std::vector<ValueType> tau_left_;
  std::vector<ValueType> tau_right_;
};

// Deduction guide
template <typename MatrixType>
SVD(MatrixType) -> SVD<std::remove_cvref_t<MatrixType>>;

}  // namespace tempura::matrix3
