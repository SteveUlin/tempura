#pragma once

#include "symbolic4/core.h"
#include "symbolic4/matrix/ops.h"
#include "symbolic4/matrix/types.h"
#include "symbolic4/operators.h"

// ============================================================================
// mvn.h - Multivariate Normal distribution with Cholesky parameterization
// ============================================================================
//
// Provides symbolic multivariate normal distribution parameterized by mean
// vector and Cholesky factor of covariance matrix. This parameterization is
// numerically stable and efficient for sampling/evaluation.
//
// Distribution:
//   y ~ MVN(μ, Σ)  where Σ = LLᵀ
//
// Log-probability:
//   log p(y|μ,L) = -K/2 log(2π) - log|L| - ½(y-μ)ᵀΣ⁻¹(y-μ)
//                = -K/2 log(2π) - Σᵢlog(Lᵢᵢ) - ½||L⁻¹(y-μ)||²
//
// Usage:
//   struct Dims;  // Dimension tag
//   auto mu = dimVector<Dims>();
//   auto l_cov = cholCov<Dims>();
//   auto y = dimVector<Dims>();
//
//   auto dist = MVNormalCholesky(mu, l_cov);
//   auto lp = dist.logProbFor(y);  // Symbolic log-probability
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// DimensionSize - Represents runtime dimension K for normalizing constant
// ============================================================================
//
// Since the dimension K is determined at runtime by the DimTag plate size,
// we need a symbolic representation. DimensionSize<DimTag> evaluates to the
// integer size of that dimension when the expression is evaluated.

template <typename DimTag>
struct DimensionSize : SymbolicTag {
  using dim_tag = DimTag;
};

// Factory function
template <typename DimTag>
constexpr auto dimensionSize() {
  return DimensionSize<DimTag>{};
}

// ============================================================================
// Log-probability functions for Multivariate Normal (Cholesky parameterization)
// ============================================================================
//
// logMVNormalCholesky: Full normalized log-probability
//   log p(y|μ,L) = -K/2 log(2π) - logDetChol(L) - ½ quadFormChol(y-μ, L)
//
// unnormalizedLogMVNormalCholesky: Omits normalizing constant (for MCMC)
//   log p(y|μ,L) ∝ -logDetChol(L) - ½ quadFormChol(y-μ, L)
//
// Arguments:
//   y      - Observation vector (DimVectorSymbol)
//   mu     - Mean vector (DimVectorSymbol)
//   l_cov  - Cholesky factor of covariance (CholeskyCovSymbol)
//   k      - Dimension size (Symbolic, typically DimensionSize<DimTag>)

// Full log-probability with normalizing constant
template <Symbolic Y, Symbolic Mu, Symbolic LCov, Symbolic K>
constexpr auto logMVNormalCholesky(Y y, Mu mu, LCov l_cov, K k) {
  // log(2π) ≈ 1.8378770664093453
  constexpr double log_2pi = 1.8378770664093453;
  auto residual = y - mu;
  // -K/2 log(2π) - logDetChol(L) - ½ ||L⁻¹(y-μ)||²
  return Fraction<-1, 2>{} * k * lit(log_2pi) - logDetChol(l_cov) -
         Fraction<1, 2>{} * quadFormChol(residual, l_cov);
}

// Unnormalized log-probability (drops -K/2 log(2π))
template <Symbolic Y, Symbolic Mu, Symbolic LCov>
constexpr auto unnormalizedLogMVNormalCholesky(Y y, Mu mu, LCov l_cov) {
  auto residual = y - mu;
  // -logDetChol(L) - ½ ||L⁻¹(y-μ)||²
  return -logDetChol(l_cov) - Fraction<1, 2>{} * quadFormChol(residual, l_cov);
}

// ============================================================================
// MVNormalCholeskyDist - Distribution wrapper with symbolic parameters
// ============================================================================
//
// Stores mean and Cholesky factor as symbolic expressions. The dimension
// is inferred from the DimTag of the parameters.
//
// Template parameters:
//   Mu    - Type of mean vector (should be DimVectorSymbol or expression)
//   LCov  - Type of Cholesky factor (should be CholeskyCovSymbol or expression)
//
// Methods:
//   logProbFor(y)             - Full normalized log p(y|μ,L)
//   unnormalizedLogProbFor(y) - Drops normalizing constant (for MCMC)
//   mu()                      - Access mean parameter
//   lCov()                    - Access Cholesky factor parameter

template <Symbolic Mu, Symbolic LCov>
struct MVNormalCholeskyDist {
  [[no_unique_address]] Mu mu_;
  [[no_unique_address]] LCov l_cov_;

  constexpr MVNormalCholeskyDist(Mu mu, LCov l_cov) : mu_{mu}, l_cov_{l_cov} {}

  // Full log-probability with normalizing constant
  // Uses DimensionSize to get K at evaluation time
  template <Symbolic Y>
  constexpr auto logProbFor(Y y) const {
    // Extract DimTag from the Cholesky factor type
    using DimTag = get_dim_tag_t<LCov>;
    return logMVNormalCholesky(y, mu_, l_cov_, dimensionSize<DimTag>());
  }

  // Unnormalized log-probability (for MCMC where constants don't matter)
  template <Symbolic Y>
  constexpr auto unnormalizedLogProbFor(Y y) const {
    return unnormalizedLogMVNormalCholesky(y, mu_, l_cov_);
  }

  constexpr auto mu() const { return mu_; }
  constexpr auto lCov() const { return l_cov_; }
};

// Deduction guide
template <typename Mu, typename LCov>
MVNormalCholeskyDist(Mu, LCov) -> MVNormalCholeskyDist<Mu, LCov>;

// ============================================================================
// Factory Functions
// ============================================================================

// Create MVN distribution from mean vector and Cholesky factor of covariance
//
// Usage:
//   auto mu = dimVector<Dims>();
//   auto L = cholCov<Dims>();
//   auto dist = mvnCholesky(mu, L);
//   auto lp = dist.logProbFor(y);
template <Symbolic Mu, Symbolic LCov>
constexpr auto mvnCholesky(Mu mu, LCov l_cov) {
  return MVNormalCholeskyDist{mu, l_cov};
}

// Convenience alias using uppercase naming convention
template <Symbolic Mu, Symbolic LCov>
constexpr auto MVNCholesky(Mu mu, LCov l_cov) {
  return MVNormalCholeskyDist{mu, l_cov};
}

}  // namespace tempura::symbolic4
