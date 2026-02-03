#pragma once

#include "symbolic4/distributions/random_var.h"
#include "symbolic4/operators.h"

// ============================================================================
// joint.h - Joint log-probability as simple sum
// ============================================================================
//
// Computes joint log-probability by summing individual log-probs.
// No DAG traversal, no deduplication - just explicit composition.
//
// Usage:
//   auto mu = normal(lit(0), lit(10));
//   auto sigma = halfNormal(lit(5));
//   auto y = normal(mu, sigma);
//
//   // Joint: user explicitly lists all RVs
//   auto joint = logProb(mu, sigma, y);
//   // = log P(mu) + log P(sigma) + log P(y|mu,sigma)
//
//   // Likelihood only (for point estimates of parameters)
//   auto likelihood = logProb(y);
//
//   // Prior only
//   auto prior = logProb(mu, sigma);
//
// The user controls which terms to include. Dependencies between RVs are
// encoded implicitly in their symbolic log-prob expressions.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// logProb - Sum of log-probabilities for listed random variables
// ============================================================================

// Single random variable
template <IsRandomVar RV>
constexpr auto logProb(const RV& rv) {
  return rv.logProb();
}

// Multiple random variables - variadic fold
template <IsRandomVar RV1, IsRandomVar RV2, IsRandomVar... Rest>
constexpr auto logProb(const RV1& rv1, const RV2& rv2, const Rest&... rest) {
  if constexpr (sizeof...(rest) == 0) {
    return rv1.logProb() + rv2.logProb();
  } else {
    return rv1.logProb() + logProb(rv2, rest...);
  }
}

// ============================================================================
// jointLogProb - Alias for logProb (clearer name for full model)
// ============================================================================

template <typename... RVs>
constexpr auto jointLogProb(const RVs&... rvs) {
  return logProb(rvs...);
}

// ============================================================================
// unnormalizedLogProb - Unnormalized variant (for MCMC)
// ============================================================================

template <IsRandomVar RV>
constexpr auto unnormalizedLogProb(const RV& rv) {
  return rv.unnormalizedLogProb();
}

template <IsRandomVar RV1, IsRandomVar RV2, IsRandomVar... Rest>
constexpr auto unnormalizedLogProb(const RV1& rv1, const RV2& rv2,
                                   const Rest&... rest) {
  if constexpr (sizeof...(rest) == 0) {
    return rv1.unnormalizedLogProb() + rv2.unnormalizedLogProb();
  } else {
    return rv1.unnormalizedLogProb() + unnormalizedLogProb(rv2, rest...);
  }
}

}  // namespace tempura::symbolic4
