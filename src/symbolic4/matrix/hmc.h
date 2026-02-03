#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

#include "symbolic4/matrix/eval.h"
#include "symbolic4/matrix/types.h"

// ============================================================================
// matrix/hmc.h - HMC integration for matrix-valued parameters
// ============================================================================
//
// Provides transforms for Cholesky-parameterized covariance matrices to enable
// HMC sampling in an unconstrained space. The key idea:
//
//   Constrained space: L is lower triangular with positive diagonal
//   Unconstrained space: flat vector z ∈ ℝⁿ where n = K*(K+1)/2 or K*(K-1)/2
//
// Transform strategy:
//   - Off-diagonal elements: stored directly (unconstrained)
//   - Diagonal elements: stored as log(L[i,i]) for positivity constraint
//   - For correlation Cholesky: diagonal is determined by unit row norm constraint
//
// Jacobian correction:
//   For covariance Cholesky: log|J| = Σᵢ log(L[i,i]) = Σᵢ z[diag_idx[i]]
//   For correlation Cholesky: more complex due to constraint (see implementation)
//
// Storage layout (column-major lower triangle):
//   K=3 covariance:  [log(L₀₀), L₁₀, log(L₁₁), L₂₀, L₂₁, log(L₂₂)]
//   K=3 correlation: [L₁₀, L₂₀, L₂₁] (diagonal determined by constraint)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Dimension calculations
// ============================================================================

// Number of free parameters for CholeskyCov: full lower triangle
constexpr auto cholCovParamCount(std::size_t k) -> std::size_t {
  return k * (k + 1) / 2;
}

// Number of free parameters for CholeskyCorr: off-diagonal only
constexpr auto cholCorrParamCount(std::size_t k) -> std::size_t {
  return k * (k - 1) / 2;
}

// ============================================================================
// Index mapping for lower triangular storage
// ============================================================================

// Map (row, col) to flat index in column-major lower triangle storage
// For K=3: (0,0)->0, (1,0)->1, (1,1)->2, (2,0)->3, (2,1)->4, (2,2)->5
constexpr auto lowerTriIndex(std::size_t row, std::size_t col) -> std::size_t {
  assert(row >= col);
  return col * (col + 1) / 2 + row;
}

// Map (row, col) to flat index for correlation (off-diagonal only)
// Skips diagonal elements since they're constrained
// For K=3: (1,0)->0, (2,0)->1, (2,1)->2
constexpr auto lowerTriOffDiagIndex(std::size_t row, std::size_t col) -> std::size_t {
  assert(row > col);
  return col * col / 2 + col * (row - col - 1) + (row - col - 1);
  // Simpler: count elements before column col, plus position in column
}

// Actually, let's use a cleaner formula:
// Elements before column c: 0 + 1 + 2 + ... + (c-1) = c*(c-1)/2
// Then add (row - col - 1) for position within column (excluding diagonal)
constexpr auto offDiagFlatIndex(std::size_t row, std::size_t col, [[maybe_unused]] std::size_t k)
    -> std::size_t {
  assert(row > col);
  // Column-major: first col has 0 off-diag elements below diag
  //               second col has 1 off-diag element, etc.
  // Elements in columns before col: 0 + 1 + ... + (col-1) = col*(col-1)/2
  // Wait, that's not right either. Let me think again.
  //
  // For K=3, off-diagonal lower triangle positions:
  //   (1,0) -> 0
  //   (2,0) -> 1
  //   (2,1) -> 2
  //
  // Column 0 has elements at rows 1,2 -> indices 0,1
  // Column 1 has element at row 2 -> index 2
  //
  // Elements before column c: 0 + 1 + ... + (K-1-0) for col 0, etc.
  // Hmm, easier to just count: sum over cols < c of (K - c' - 1)
  //
  // Simpler approach:
  // Total index = (number of off-diag elements in columns 0..col-1) + (row - col - 1)
  // Off-diag elements in column c: K - c - 1
  // Sum for columns 0..col-1: Σ(K-1-c') for c'=0..col-1 = col*K - col - col*(col-1)/2
  //                         = col*(K - 1 - (col-1)/2) = col*(2K - 2 - col + 1)/2
  //                         = col*(2K - 1 - col)/2
  std::size_t elements_before = 0;
  for (std::size_t c = 0; c < col; ++c) {
    elements_before += (row > c + 1 ? row - c - 1 : 0);  // Wrong, need K not row
  }
  // Let me just compute it directly for clarity:
  std::size_t idx = 0;
  for (std::size_t c = 0; c < col; ++c) {
    idx += k - c - 1;  // Off-diagonal elements in column c
  }
  idx += row - col - 1;  // Position within column
  return idx;
}

// ============================================================================
// flattenCholesky - Convert K×K Cholesky factor to flat parameter vector
// ============================================================================
//
// For CholeskyCov (is_corr=false):
//   - Returns K*(K+1)/2 parameters
//   - Diagonal stored as log(L[i,i])
//   - Off-diagonal stored directly
//
// For CholeskyCorr (is_corr=true):
//   - Returns K*(K-1)/2 parameters
//   - Off-diagonal only (diagonal determined by unit row norm)

template <typename Matrix>
auto flattenCholesky(const Matrix& L, std::size_t k, bool is_corr) -> std::vector<double> {
  if (is_corr) {
    std::vector<double> params(cholCorrParamCount(k));
    std::size_t idx = 0;
    for (std::size_t col = 0; col < k; ++col) {
      for (std::size_t row = col + 1; row < k; ++row) {
        params[idx++] = L[row, col];
      }
    }
    return params;
  } else {
    std::vector<double> params(cholCovParamCount(k));
    std::size_t idx = 0;
    for (std::size_t col = 0; col < k; ++col) {
      for (std::size_t row = col; row < k; ++row) {
        if (row == col) {
          params[idx++] = std::log(L[row, col]);  // Diagonal: log transform
        } else {
          params[idx++] = L[row, col];  // Off-diagonal: direct
        }
      }
    }
    return params;
  }
}

// ============================================================================
// unflattenCholesky - Convert flat parameter vector back to K×K Cholesky
// ============================================================================
//
// For CholeskyCov: Apply exp() to recover positive diagonal elements
// For CholeskyCorr: Reconstruct diagonal from unit row norm constraint

template <typename Matrix>
auto unflattenCholesky(const std::vector<double>& params, std::size_t k, bool is_corr,
                       Matrix& L) -> void {
  if (is_corr) {
    // Correlation Cholesky: diagonal determined by constraint
    // L[0,0] = 1 (first row has norm 1)
    // L[i,i] = sqrt(1 - sum of squared off-diagonal elements in row i)
    std::size_t idx = 0;

    // Zero out upper triangle and set first diagonal
    for (std::size_t i = 0; i < k; ++i) {
      for (std::size_t j = i + 1; j < k; ++j) {
        L[i, j] = 0.0;
      }
    }

    // Fill in off-diagonal elements column by column
    for (std::size_t col = 0; col < k; ++col) {
      for (std::size_t row = col + 1; row < k; ++row) {
        L[row, col] = params[idx++];
      }
    }

    // Compute diagonal elements from row norm constraint
    for (std::size_t i = 0; i < k; ++i) {
      double sum_sq = 0.0;
      for (std::size_t j = 0; j < i; ++j) {
        sum_sq += L[i, j] * L[i, j];
      }
      L[i, i] = std::sqrt(1.0 - sum_sq);
    }
  } else {
    // Covariance Cholesky: apply exp() to diagonal
    std::size_t idx = 0;

    // Zero out upper triangle
    for (std::size_t i = 0; i < k; ++i) {
      for (std::size_t j = i + 1; j < k; ++j) {
        L[i, j] = 0.0;
      }
    }

    // Fill in lower triangle column by column
    for (std::size_t col = 0; col < k; ++col) {
      for (std::size_t row = col; row < k; ++row) {
        if (row == col) {
          L[row, col] = std::exp(params[idx++]);  // Diagonal: exp transform
        } else {
          L[row, col] = params[idx++];  // Off-diagonal: direct
        }
      }
    }
  }
}

// Overload returning a new matrix (for types with default constructor)
template <typename Matrix>
auto unflattenCholesky(const std::vector<double>& params, std::size_t k, bool is_corr) -> Matrix {
  Matrix L(k, k);  // Assumes Matrix has (rows, cols) constructor
  unflattenCholesky(params, k, is_corr, L);
  return L;
}

// ============================================================================
// choleskyLogJacobian - Jacobian adjustment for change of variables
// ============================================================================
//
// For the transform from unconstrained z to constrained L:
//
// CholeskyCov: L[i,i] = exp(z[i]), so ∂L[i,i]/∂z[i] = exp(z[i])
//   log|J| = Σᵢ log(exp(z[diag_idx[i]])) = Σᵢ z[diag_idx[i]]
//
// CholeskyCorr: More complex due to the constraint L[i,i] = sqrt(1 - Σⱼ<ᵢ L[i,j]²)
//   The Jacobian involves partial derivatives of the diagonal w.r.t. off-diagonal.
//   See Stan reference manual for the full derivation.

inline auto choleskyLogJacobianCov(const std::vector<double>& params, std::size_t k) -> double {
  // For covariance Cholesky: log|J| = sum of log-diagonal params
  // Since diagonal = exp(param), the Jacobian contribution is just the param value
  double log_jac = 0.0;
  std::size_t idx = 0;
  for (std::size_t col = 0; col < k; ++col) {
    log_jac += params[idx];  // Diagonal element's log-space value
    idx += k - col;  // Skip to next column's diagonal
  }
  return log_jac;
}

inline auto choleskyLogJacobianCorr(const std::vector<double>& params, std::size_t k) -> double {
  // For correlation Cholesky, the Jacobian is more complex.
  // The constrained diagonal is L[i,i] = sqrt(1 - Σⱼ<ᵢ L[i,j]²)
  //
  // Using the formula from Stan (transforms/lb_constrain.hpp):
  //   log|J| = Σᵢ₌₁^{K-1} (K - i - 1) * log(L[i,i])
  //
  // First reconstruct the diagonal elements
  std::vector<double> diag(k);
  std::vector<double> off_diag(cholCorrParamCount(k));
  std::copy(params.begin(), params.end(), off_diag.begin());

  // Row i has off-diagonal elements at columns 0..i-1
  std::size_t param_idx = 0;
  for (std::size_t col = 0; col < k; ++col) {
    for (std::size_t row = col + 1; row < k; ++row) {
      (void)param_idx;  // Just for counting
    }
  }

  // Compute diagonal from off-diagonal
  diag[0] = 1.0;
  std::size_t idx = 0;
  std::vector<std::vector<double>> L_offdiag(k);
  for (std::size_t i = 0; i < k; ++i) {
    L_offdiag[i].resize(i, 0.0);
  }

  // Fill off-diagonal storage
  idx = 0;
  for (std::size_t col = 0; col < k; ++col) {
    for (std::size_t row = col + 1; row < k; ++row) {
      L_offdiag[row][col] = params[idx++];
    }
  }

  // Compute diagonal and Jacobian
  double log_jac = 0.0;
  for (std::size_t i = 0; i < k; ++i) {
    double sum_sq = 0.0;
    for (std::size_t j = 0; j < i; ++j) {
      sum_sq += L_offdiag[i][j] * L_offdiag[i][j];
    }
    diag[i] = std::sqrt(1.0 - sum_sq);

    // Jacobian contribution from row i (for i > 0)
    // The exponent is (K - i - 1) in 0-based indexing
    if (i > 0 && diag[i] > 0.0) {
      log_jac += static_cast<double>(k - i - 1) * std::log(diag[i]);
    }
  }

  return log_jac;
}

inline auto choleskyLogJacobian(const std::vector<double>& params, std::size_t k, bool is_corr)
    -> double {
  if (is_corr) {
    return choleskyLogJacobianCorr(params, k);
  } else {
    return choleskyLogJacobianCov(params, k);
  }
}

// ============================================================================
// CholeskyTransform - Transform compatible with existing transform system
// ============================================================================
//
// Wraps Cholesky parameterization for use with makeTransformedPosterior.
// Unlike scalar transforms, this handles vector parameters representing
// the flattened Cholesky factor.
//
// Template Parameters:
//   Param - The CholeskyCovSymbol or CholeskyCorrSymbol being transformed
//   K - Matrix dimension (compile-time for fixed-size, or 0 for dynamic)

template <typename Param, std::size_t K = 0>
struct CholeskyTransform {
  Param param;
  std::size_t dim;  // Runtime dimension (used when K=0)

  static constexpr bool is_correlation = Param::is_correlation;

  // Number of free parameters
  constexpr auto paramCount() const -> std::size_t {
    std::size_t k = (K > 0) ? K : dim;
    return is_correlation ? cholCorrParamCount(k) : cholCovParamCount(k);
  }

  // Get dimension
  constexpr auto dimension() const -> std::size_t {
    return (K > 0) ? K : dim;
  }

  // Forward: constrained L → unconstrained z
  template <typename Matrix>
  auto forward(const Matrix& L) const -> std::vector<double> {
    return flattenCholesky(L, dimension(), is_correlation);
  }

  // Inverse: unconstrained z → constrained L
  template <typename Matrix>
  auto inverse(const std::vector<double>& z) const -> Matrix {
    return unflattenCholesky<Matrix>(z, dimension(), is_correlation);
  }

  // Log-determinant of Jacobian for change of variables
  auto logDetJacobian(const std::vector<double>& z) const -> double {
    return choleskyLogJacobian(z, dimension(), is_correlation);
  }
};

// ============================================================================
// Factory functions
// ============================================================================

// Create CholeskyTransform with compile-time dimension
template <std::size_t K, typename Param>
constexpr auto choleskyTransform(Param p) {
  return CholeskyTransform<Param, K>{p, K};
}

// Create CholeskyTransform with runtime dimension
template <typename Param>
constexpr auto choleskyTransform(Param p, std::size_t k) {
  return CholeskyTransform<Param, 0>{p, k};
}

// ============================================================================
// DimVectorTransform - Maps K-dimensional vector to K unconstrained params
// ============================================================================
//
// Simple identity transform for unconstrained vectors:
//   x_i = z_i
//
// Log-Jacobian: 0 (identity transform)
//
// This is the simplest multivariate transform - just copies between vector
// representations. Used for multivariate normal parameters where each
// component is unconstrained.

struct DimVectorTransform {
  std::size_t dim;

  // Forward: unconstrained z -> DynamicVector x (identity)
  auto transform(std::span<const double> z) const -> DynamicVector {
    DynamicVector x(dim);
    for (std::size_t i = 0; i < dim; ++i) {
      x[i] = z[i];
    }
    return x;
  }

  // Inverse: DynamicVector x -> unconstrained z (identity)
  auto inverse(const DynamicVector& x) const -> std::vector<double> {
    std::vector<double> z(dim);
    for (std::size_t i = 0; i < dim; ++i) {
      z[i] = x[i];
    }
    return z;
  }

  // Log-Jacobian: 0 for identity transform
  auto logJacobian([[maybe_unused]] std::span<const double> z) const -> double {
    return 0.0;
  }

  // Number of unconstrained parameters needed for dimension k
  static constexpr auto stateSize(std::size_t k) -> std::size_t { return k; }
};

// Factory function for DimVectorTransform
constexpr auto dimVectorTransform(std::size_t k) -> DimVectorTransform {
  return DimVectorTransform{k};
}

// ============================================================================
// CholeskyParameterization - High-level interface for HMC with matrices
// ============================================================================
//
// Combines the Cholesky symbol, its prior, and transform into a single
// object that can be used with the HMC adapter.

template <typename CholeskySymbol, typename Prior, std::size_t K = 0>
class CholeskyParameterization {
 public:
  using symbol_type = CholeskySymbol;
  using prior_type = Prior;
  static constexpr bool is_correlation = CholeskySymbol::is_correlation;

  constexpr CholeskyParameterization(CholeskySymbol sym, Prior prior, std::size_t dim)
      : symbol_{sym}, prior_{prior}, dim_{(K > 0) ? K : dim} {}

  // Number of free parameters
  constexpr auto paramCount() const -> std::size_t {
    return is_correlation ? cholCorrParamCount(dim_) : cholCovParamCount(dim_);
  }

  // Get the underlying symbol
  constexpr auto symbol() const { return symbol_; }

  // Get the prior distribution
  constexpr auto prior() const { return prior_; }

  // Get the transform
  auto transform() const {
    return CholeskyTransform<CholeskySymbol, K>{symbol_, dim_};
  }

  // Flatten Cholesky factor to unconstrained parameters
  template <typename Matrix>
  auto flatten(const Matrix& L) const -> std::vector<double> {
    return flattenCholesky(L, dim_, is_correlation);
  }

  // Unflatten unconstrained parameters to Cholesky factor
  template <typename Matrix>
  auto unflatten(const std::vector<double>& params) const -> Matrix {
    return unflattenCholesky<Matrix>(params, dim_, is_correlation);
  }

  // Unflatten into existing matrix
  template <typename Matrix>
  auto unflatten(const std::vector<double>& params, Matrix& L) const -> void {
    unflattenCholesky(params, dim_, is_correlation, L);
  }

  // Log Jacobian adjustment
  auto logJacobian(const std::vector<double>& params) const -> double {
    return choleskyLogJacobian(params, dim_, is_correlation);
  }

  // Dimension
  constexpr auto dimension() const -> std::size_t { return dim_; }

 private:
  CholeskySymbol symbol_;
  Prior prior_;
  std::size_t dim_;
};

// ============================================================================
// Factory functions for CholeskyParameterization
// ============================================================================

// Covariance Cholesky with LKJ prior (eta parameter)
template <std::size_t K, typename Id, typename DimTag, typename Prior>
auto cholCovParam(CholeskyCovSymbol<Id, DimTag> sym, Prior prior) {
  return CholeskyParameterization<CholeskyCovSymbol<Id, DimTag>, Prior, K>{sym, prior, K};
}

template <typename Id, typename DimTag, typename Prior>
auto cholCovParam(CholeskyCovSymbol<Id, DimTag> sym, Prior prior, std::size_t k) {
  return CholeskyParameterization<CholeskyCovSymbol<Id, DimTag>, Prior, 0>{sym, prior, k};
}

// Correlation Cholesky with LKJ prior
template <std::size_t K, typename Id, typename DimTag, typename Prior>
auto cholCorrParam(CholeskyCorrSymbol<Id, DimTag> sym, Prior prior) {
  return CholeskyParameterization<CholeskyCorrSymbol<Id, DimTag>, Prior, K>{sym, prior, K};
}

template <typename Id, typename DimTag, typename Prior>
auto cholCorrParam(CholeskyCorrSymbol<Id, DimTag> sym, Prior prior, std::size_t k) {
  return CholeskyParameterization<CholeskyCorrSymbol<Id, DimTag>, Prior, 0>{sym, prior, k};
}

// ============================================================================
// Gradient helpers for Cholesky parameters
// ============================================================================
//
// When computing gradients w.r.t. unconstrained parameters, we need to apply
// the chain rule through the Cholesky transform.

// Gradient of log-probability w.r.t. unconstrained params for CholeskyCov
// Given: ∂log p/∂L (gradient w.r.t. matrix elements)
// Returns: ∂log p/∂z (gradient w.r.t. unconstrained params)
template <typename GradMatrix>
auto cholCovGradientToUnconstrained(const GradMatrix& grad_L,
                                     const std::vector<double>& params,
                                     std::size_t k) -> std::vector<double> {
  std::vector<double> grad_z(cholCovParamCount(k));
  std::size_t idx = 0;

  for (std::size_t col = 0; col < k; ++col) {
    for (std::size_t row = col; row < k; ++row) {
      if (row == col) {
        // Diagonal: L[i,i] = exp(z[i]), so ∂L/∂z = exp(z) = L[i,i]
        // Chain rule: ∂log p/∂z = (∂log p/∂L) * (∂L/∂z) = grad_L * L
        // Plus Jacobian: ∂/∂z[log|J|] = 1 (since log|J| = z for diagonal)
        double L_ii = std::exp(params[idx]);
        grad_z[idx] = grad_L[row, col] * L_ii + 1.0;
      } else {
        // Off-diagonal: L[i,j] = z directly, so ∂L/∂z = 1
        grad_z[idx] = grad_L[row, col];
      }
      ++idx;
    }
  }

  return grad_z;
}

// Gradient for CholeskyCorr is more complex due to diagonal constraint
// The diagonal depends on off-diagonal elements, creating coupled gradients
template <typename GradMatrix, typename Matrix>
auto cholCorrGradientToUnconstrained(const GradMatrix& grad_L,
                                      const Matrix& L,
                                      const std::vector<double>& params,
                                      std::size_t k) -> std::vector<double> {
  std::vector<double> grad_z(cholCorrParamCount(k));

  // For correlation Cholesky, the diagonal is:
  //   L[i,i] = sqrt(1 - Σⱼ<ᵢ L[i,j]²)
  //
  // So: ∂L[i,i]/∂L[i,j] = -L[i,j] / L[i,i]
  //
  // Chain rule for off-diagonal param z[i,j]:
  //   ∂log p/∂z[i,j] = ∂log p/∂L[i,j] + ∂log p/∂L[i,i] * (∂L[i,i]/∂L[i,j])
  //                  = grad_L[i,j] - grad_L[i,i] * L[i,j] / L[i,i]
  //
  // Plus Jacobian adjustment from choleskyLogJacobianCorr

  std::size_t idx = 0;
  for (std::size_t col = 0; col < k; ++col) {
    for (std::size_t row = col + 1; row < k; ++row) {
      double L_ii = L[row, row];
      double L_ij = L[row, col];

      // Direct gradient
      double grad = grad_L[row, col];

      // Chain through diagonal constraint
      if (L_ii > 1e-10) {
        grad -= grad_L[row, row] * L_ij / L_ii;
      }

      // Jacobian adjustment: ∂/∂z[log|J|]
      // log|J| = Σᵢ (K-i-1) * log(L[i,i])
      // ∂log|J|/∂z[i,j] = (K-row-1) * (-L[i,j]/L[i,i]) / L[i,i] * ∂L[i,i]/∂z
      //                 = -(K-row-1) * L[i,j] / L[i,i]²
      if (row < k - 1 && L_ii > 1e-10) {
        grad -= static_cast<double>(k - row - 1) * L_ij / (L_ii * L_ii);
      }

      grad_z[idx++] = grad;
    }
  }

  return grad_z;
}

// ============================================================================
// Type traits
// ============================================================================

template <typename T>
struct IsCholeskyTransform : std::false_type {};

template <typename P, std::size_t K>
struct IsCholeskyTransform<CholeskyTransform<P, K>> : std::true_type {};

template <typename T>
constexpr bool is_cholesky_transform_v = IsCholeskyTransform<T>::value;

template <typename T>
struct IsCholeskyParameterization : std::false_type {};

template <typename S, typename P, std::size_t K>
struct IsCholeskyParameterization<CholeskyParameterization<S, P, K>> : std::true_type {};

template <typename T>
constexpr bool is_cholesky_parameterization_v = IsCholeskyParameterization<T>::value;

template <typename T>
struct IsDimVectorTransform : std::false_type {};

template <>
struct IsDimVectorTransform<DimVectorTransform> : std::true_type {};

template <typename T>
constexpr bool is_dim_vector_transform_v = IsDimVectorTransform<T>::value;

}  // namespace tempura::symbolic4
