#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>

#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// matrix/lkj.h - LKJ prior distribution for correlation matrices
// ============================================================================
//
// The LKJ distribution (Lewandowski, Kurowicka, Joe 2009) is a prior over
// correlation matrices, commonly used in Bayesian hierarchical models.
//
// For a K×K correlation matrix Ω with Cholesky factor L (where Ω = LLᵀ):
//
//   log p(L | η) = Σᵢ₌₂^K (K - i + 2η - 2) × log(L[i,i])
//
// Where:
//   - η (eta) is the shape parameter
//   - L[i,i] is the i-th diagonal element of the Cholesky factor
//   - The sum starts at i=2 because L[1,1] = 1 for correlation matrices
//
// Interpretation of η:
//   η = 1: Uniform distribution over correlation matrices
//   η > 1: Shrinkage toward identity matrix (stronger correlations less likely)
//   η < 1: Encourages extreme correlations (rarely used)
//
// This implementation uses the Cholesky parameterization which is more
// numerically stable and efficient for MCMC sampling.
//
// ============================================================================

namespace tempura {

// ============================================================================
// LKJ Cholesky Log-Probability Operator
// ============================================================================
//
// Computes log p(L | η) for a Cholesky factor L of a correlation matrix.
// The dimension K is inferred from the matrix size at runtime.
//
// Template parameters determine whether this is the full or unnormalized form.

struct LogProbLKJCholOp {
  // Evaluates the log-probability for Cholesky factor of correlation matrix.
  // L must have unit diagonal row norms (i.e., each row's squared elements sum to 1).
  // In practice, L[i,i] = sqrt(1 - sum of squared off-diagonal elements in row i).
  //
  // For correlation Cholesky, L[0,0] = 1, and subsequent diagonals are determined
  // by the constraint that the resulting correlation matrix has ones on diagonal.
  template <typename L, typename Eta>
  constexpr auto operator()(const L& chol, Eta eta) const {
    using std::log;
    auto k = chol.rows();
    using T = decltype(log(chol[0, 0]) * double(eta));
    T log_prob{};

    // LKJ log density: Σᵢ₌₂^K (K - i + 2η - 2) × log(L[i,i])
    // Indexing from i=1 in 0-based (i.e., skip i=0 since L[0,0] = 1)
    for (size_t i = 1; i < k; ++i) {
      // Coefficient: K - i + 2η - 2, where i is 1-based
      // In 0-based: K - (i+1) + 2η - 2 = K - i - 1 + 2η - 2 = K - i + 2η - 3
      // Wait, let me recalculate for 0-based indexing:
      //   Original: i ∈ [2, K], coefficient = K - i + 2η - 2
      //   0-based:  i ∈ [1, K-1], so 1-based index is (i+1)
      //   Coefficient = K - (i+1) + 2η - 2 = K - i - 3 + 2η
      //
      // Actually simpler: the 1-based formula uses i from 2 to K.
      // In 0-based, that's i from 1 to K-1.
      // Original coefficient for 1-based index j: K - j + 2η - 2
      // For 0-based i, j = i + 1, so: K - (i+1) + 2η - 2 = K - i - 1 + 2η - 2 = K - i + 2η - 3
      //
      // Hmm, let me verify with K=3:
      //   1-based: i=2: K-2+2η-2 = 3-2+2η-2 = 2η-1
      //            i=3: K-3+2η-2 = 3-3+2η-2 = 2η-2
      //   0-based: i=1: K-i+2η-3 = 3-1+2η-3 = 2η-1 ✓
      //            i=2: K-i+2η-3 = 3-2+2η-3 = 2η-2 ✓
      auto coeff = static_cast<double>(k) - static_cast<double>(i) + 2.0 * double(eta) - 3.0;
      log_prob += coeff * log(chol[i, i]);
    }

    return log_prob;
  }
};

}  // namespace tempura

namespace tempura::symbolic4 {

// Import operator into symbolic4 namespace
using tempura::LogProbLKJCholOp;

// ============================================================================
// Symbolic LKJ Log-Probability Function
// ============================================================================
//
// Returns a symbolic expression for the LKJ Cholesky log-probability.
// The Expression<LogProbLKJCholOp, L, Eta> is evaluated at runtime when
// concrete values are provided via an interpreter.

template <Symbolic L, Symbolic Eta>
constexpr auto logProbLKJChol(L chol, Eta eta) {
  return Expression<LogProbLKJCholOp, L, Eta>{chol, eta};
}

// ============================================================================
// LKJ Cholesky Distribution Wrapper
// ============================================================================
//
// Parameterized by eta (shape parameter). Provides logProbFor() method
// that takes a CholeskyCorrSymbol and returns the symbolic log-probability.
//
// Usage:
//   Symbol<struct Eta> eta;
//   auto dist = LKJCholeskyDist{eta};
//   auto lp = dist.logProbFor(L_corr);

template <Symbolic Eta>
struct LKJCholeskyDist {
  [[no_unique_address]] Eta eta_;

  constexpr explicit LKJCholeskyDist(Eta eta) : eta_{eta} {}

  // Compute log-probability for a Cholesky factor of correlation matrix.
  // L should be a CholeskyCorrSymbol or compatible type.
  template <Symbolic L>
  constexpr auto logProbFor(L chol) const {
    return logProbLKJChol(chol, eta_);
  }

  // LKJ has no simple additive normalizing constant that can be dropped,
  // so unnormalized is the same as normalized for gradient-based inference.
  template <Symbolic L>
  constexpr auto unnormalizedLogProbFor(L chol) const {
    return logProbLKJChol(chol, eta_);
  }

  constexpr auto eta() const { return eta_; }
};

// Deduction guide
template <typename Eta>
LKJCholeskyDist(Eta) -> LKJCholeskyDist<Eta>;

// ============================================================================
// Factory Function
// ============================================================================
//
// lkjCholesky(eta) creates an LKJCholeskyDist with the given shape parameter.
//
// Examples:
//   auto prior = lkjCholesky(2.0);        // η=2, shrinks toward identity
//   auto uniform = lkjCholesky(1.0);      // η=1, uniform over correlations
//   auto prior = lkjCholesky(eta_symbol); // Symbolic eta for inference

template <typename Eta>
constexpr auto lkjCholesky(Eta eta) {
  if constexpr (Symbolic<Eta>) {
    return LKJCholeskyDist{eta};
  } else {
    // Convert numeric literals to symbolic
    return LKJCholeskyDist{lit(static_cast<double>(eta))};
  }
}

// ============================================================================
// Convenience aliases for common parameterizations
// ============================================================================

// LKJ(1) is uniform over correlation matrices
inline auto uniformCorrelation() {
  return lkjCholesky(1.0);
}

// LKJ(2) is a common weakly informative prior
inline auto weaklyInformativeCorrelation() {
  return lkjCholesky(2.0);
}

}  // namespace tempura::symbolic4
