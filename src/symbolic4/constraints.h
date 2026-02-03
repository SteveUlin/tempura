#pragma once

#include <cmath>

#include "symbolic4/core.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/operators.h"

// ============================================================================
// constraints.h - Unified constraint transforms for distribution support types
// ============================================================================
//
// Distributions with constrained support (Positive, Unit) require transforms
// between unconstrained space (where HMC samples) and constrained space
// (where the distribution is defined).
//
// This header provides both:
//   - Numeric transforms: double → double  (for evaluation)
//   - Symbolic transforms: Expr → Expr     (for log-prob collection, Jacobian)
//
// ============================================================================

namespace tempura::symbolic4::constraints {

// ============================================================================
// Numeric: apply constraint transform to a double value
// ============================================================================

template <typename Support>
constexpr auto applyNumeric(double z) -> double {
  if constexpr (is_positive_support_v<Support>) {
    return std::exp(z);  // Positive: x = exp(z)
  } else if constexpr (is_unit_support_v<Support>) {
    return 1.0 / (1.0 + std::exp(-z));  // Unit: x = sigmoid(z)
  } else {
    return z;  // Real: x = z (no transform)
  }
}

// ============================================================================
// Symbolic: return the constrained expression for a given variable
// ============================================================================

template <typename Support, Symbolic Z>
constexpr auto expr(Z z) {
  if constexpr (is_positive_support_v<Support>) {
    return exp(z);
  } else if constexpr (is_unit_support_v<Support>) {
    return 1_c / (1_c + exp(-z));
  } else {
    return z;
  }
}

// ============================================================================
// Symbolic: return the log-Jacobian expression: log |dx/dz|
// ============================================================================

template <typename Support, Symbolic Z>
constexpr auto logJacobian(Z z) {
  if constexpr (is_positive_support_v<Support>) {
    return z;  // log|d(exp(z))/dz| = log(exp(z)) = z
  } else if constexpr (is_unit_support_v<Support>) {
    auto s = 1_c / (1_c + exp(-z));
    return log(s) + log(1_c - s);  // log(s(1-s))
  } else {
    return 0_c;
  }
}

}  // namespace tempura::symbolic4::constraints
