#pragma once

#include "symbolic4/core.h"
#include "symbolic4/matrix/ops.h"
#include "symbolic4/matrix/types.h"

// ============================================================================
// matrix/diff.h - Derivative placeholder types for matrix operations
// ============================================================================
//
// Defines placeholder operation types used by matrix evaluation (eval.h)
// to represent derivatives of matrix operations symbolically. These types
// appear in expression trees built during differentiation and are resolved
// to numeric values during evaluation.
//
// The actual differentiation rules for matrix ops were previously defined
// as Diff<Var>::Rule specializations for the cata-based diff system. Those
// rules are no longer needed since we've migrated to strategy-based
// differentiation (strategy/diff.h). The strategy system handles matrix
// ops via generic recursive rules; these placeholder types remain for
// evaluation support.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Derivative placeholder operations
// ============================================================================

// Placeholder for the derivative of a dot product w.r.t. its arguments
struct DotDerivOp {};

// Placeholder for the derivative of logDetChol w.r.t. its argument
struct LogDetCholDerivOp {};

// Placeholder for the derivative of quadFormChol w.r.t. x
struct QuadFormCholDerivXOp {};

// Placeholder for the derivative of quadFormChol w.r.t. L
struct QuadFormCholDerivLOp {};

// ============================================================================
// Gradient operations for multivariate normal log-probability
// ============================================================================

// Gradient of log p(x | mu, L) w.r.t. x: returns Sigma^-1 (mu - x) = L^-T L^-1 (mu - x)
struct MvnGradXOp {};

// Gradient of log p(x | mu, L) w.r.t. L: involves trace and outer product terms
struct MvnGradLOp {};

// Symbolic function to compute MVN gradient w.r.t. x
template <Symbolic X, Symbolic Mu, Symbolic L>
constexpr auto mvnGradX(X x, Mu mu, L chol) {
  return Expression<MvnGradXOp, X, Mu, L>{x, mu, chol};
}

// Symbolic function to compute MVN gradient w.r.t. L
template <Symbolic X, Symbolic Mu, Symbolic L>
constexpr auto mvnGradL(X x, Mu mu, L chol) {
  return Expression<MvnGradLOp, X, Mu, L>{x, mu, chol};
}

}  // namespace tempura::symbolic4
