#pragma once

#include "symbolic4/core.h"
#include "symbolic4/interpreter/diff.h"
#include "symbolic4/matrix/ops.h"
#include "symbolic4/matrix/types.h"

// ============================================================================
// matrix/diff.h - Differentiation rules for matrix operations
// ============================================================================
//
// Extends the Diff interpreter with rules for matrix-valued operations that
// return scalars. These operations are common in probabilistic programming
// (multivariate normals, Mahalanobis distances, log-determinants).
//
// Operations supported:
//   dot(v1, v2)          - Vector dot product
//   logDetChol(L)        - Log-determinant of Cholesky factor
//   quadFormChol(x, L)   - Quadratic form via Cholesky: ||L^-1 x||^2
//
// Design notes:
//   Since our symbolic system differentiates scalars w.r.t. scalars, matrix
//   operations present a challenge: the inputs may be matrix/vector symbols
//   that depend on scalar parameters. We handle this by:
//
//   1. Treating matrix/vector symbols as opaque when they don't depend on
//      the differentiation variable (derivative is zero)
//
//   2. When the matrix operations depend on scalar parameters, we use the
//      chain rule structure and symbolic placeholders that will be resolved
//      during evaluation
//
//   3. Specific derivatives for structured cases (e.g., diagonal-only
//      contribution for logDetChol)
//
// Mathematical background:
//
//   DotOp: d/dx (v1 . v2) = (dv1/dx) . v2 + v1 . (dv2/dx)
//
//   LogDetCholOp: d/dx log|det(L)| = sum_i (1/L[i,i]) * dL[i,i]/dx
//     For Cholesky factor, only diagonal elements contribute.
//
//   QuadFormCholOp: d/dx ||L^-1 x||^2
//     Let y = L^-1 x. Then ||y||^2 = y^T y.
//     d/dx ||y||^2 = 2 y^T dy/dx
//     For x: dy/dx = L^-1 dx/dx, so derivative = 2 y^T L^-1 dx = 2 x^T Sigma^-1 dx
//     For L: more complex, involves -L^-1 dL L^-1 x
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Forward declarations for derivative placeholders
// ============================================================================
//
// When matrix symbols depend on scalar parameters, we need placeholder
// expressions that represent partial derivatives. These are resolved during
// evaluation when the concrete parameter structure is known.

// Placeholder for the derivative of a dot product w.r.t. its arguments
struct DotDerivOp {};

// Placeholder for the derivative of logDetChol w.r.t. its argument
struct LogDetCholDerivOp {};

// Placeholder for the derivative of quadFormChol w.r.t. x
struct QuadFormCholDerivXOp {};

// Placeholder for the derivative of quadFormChol w.r.t. L
struct QuadFormCholDerivLOp {};

// ============================================================================
// Diff<Var>::Rule specializations for matrix operations
// ============================================================================

template <typename Var>
struct Diff;

// DotOp: d/dx dot(v1, v2) = dot(dv1, v2) + dot(v1, dv2)
// For the paramorphism, P contains Pair{original, derivative} for each arg
template <typename Var>
template <typename P1, typename P2>
struct Diff<Var>::Rule<DotOp, P1, P2> {
  static constexpr auto apply(P1 p1, P2 p2) {
    // p1.first = v1 (original), p1.second = dv1/dVar
    // p2.first = v2 (original), p2.second = dv2/dVar
    //
    // If both derivatives are zero (vectors don't depend on Var), return 0
    // Otherwise, apply product rule: dot(dv1, v2) + dot(v1, dv2)

    if constexpr (is_constant_v<decltype(p1.second)> &&
                  is_constant_v<decltype(p2.second)>) {
      // Both derivatives are constants (likely zero)
      if constexpr (decltype(p1.second)::value == 0 &&
                    decltype(p2.second)::value == 0) {
        return Constant<0>{};
      } else if constexpr (decltype(p1.second)::value == 0) {
        // Only v2 varies: dot(v1, dv2)
        return dot(p1.first, p2.second);
      } else if constexpr (decltype(p2.second)::value == 0) {
        // Only v1 varies: dot(dv1, v2)
        return dot(p1.second, p2.first);
      } else {
        // Both vary: dot(dv1, v2) + dot(v1, dv2)
        return dot(p1.second, p2.first) + dot(p1.first, p2.second);
      }
    } else {
      // General case: both may vary
      return dot(p1.second, p2.first) + dot(p1.first, p2.second);
    }
  }
};

// LogDetCholOp: d/dx log|det(L)| = trace(L^-1 dL/dx)
// For Cholesky (lower triangular), this simplifies to sum_i (1/L[i,i]) dL[i,i]/dx
//
// When L is a CholeskyCovSymbol or CholeskyCorrSymbol, the derivative depends
// on how L is parameterized. We return a symbolic expression that encodes
// the derivative structure.
template <typename Var>
template <typename P>
struct Diff<Var>::Rule<LogDetCholOp, P> {
  static constexpr auto apply(P p) {
    // p.first = L (original Cholesky factor)
    // p.second = dL/dVar (derivative of L)

    if constexpr (is_constant_v<decltype(p.second)>) {
      if constexpr (decltype(p.second)::value == 0) {
        // L doesn't depend on Var
        return Constant<0>{};
      }
    }

    // L depends on Var. The derivative is trace(L^-1 dL).
    // For lower triangular L, trace(L^-1 dL) = sum_i dL[i,i] / L[i,i]
    //
    // We create a symbolic expression that represents this operation.
    // The LogDetCholDerivOp takes (L, dL) and computes the trace.
    return Expression<LogDetCholDerivOp, decltype(p.first), decltype(p.second)>{
        p.first, p.second};
  }
};

// QuadFormCholOp: d/dx ||L^-1 x||^2
// Let y = L^-1 x (solved via forward substitution)
// ||y||^2 = y^T y
//
// By chain rule:
//   d/dx ||y||^2 = 2 y^T (dy/dx)
//
// Case 1: Differentiating w.r.t. x only (L is constant)
//   dy/dx = L^-1 dx/dx
//   d/dx ||y||^2 = 2 y^T L^-1 dx = 2 x^T (L L^T)^-1 dx = 2 x^T Sigma^-1 dx
//
// Case 2: Differentiating w.r.t. L only (x is constant)
//   y = L^-1 x implies L y = x, so dL y + L dy = 0
//   Thus dy = -L^-1 dL y = -L^-1 dL L^-1 x
//   d/dL ||y||^2 = 2 y^T (-L^-1 dL y) = -2 y^T L^-1 dL L^-1 x
//
// Case 3: Both vary - sum of cases 1 and 2
template <typename Var>
template <typename PX, typename PL>
struct Diff<Var>::Rule<QuadFormCholOp, PX, PL> {
  static constexpr auto apply(PX px, PL pl) {
    // px.first = x (original vector)
    // px.second = dx/dVar
    // pl.first = L (original Cholesky factor)
    // pl.second = dL/dVar

    constexpr bool x_is_const = is_constant_v<decltype(px.second)> &&
                                decltype(px.second)::value == 0;
    constexpr bool L_is_const = is_constant_v<decltype(pl.second)> &&
                                decltype(pl.second)::value == 0;

    if constexpr (x_is_const && L_is_const) {
      // Neither depends on Var
      return Constant<0>{};
    } else if constexpr (L_is_const) {
      // Only x varies: 2 x^T Sigma^-1 dx
      // = 2 * dot(solveTriangular(L, x), solveTriangular(L, dx))
      // = 2 * dot(L^-1 x, L^-1 dx)
      auto y = solveTriangular(pl.first, px.first);
      auto dy = solveTriangular(pl.first, px.second);
      return Constant<2>{} * dot(y, dy);
    } else if constexpr (x_is_const) {
      // Only L varies: -2 y^T L^-1 dL y where y = L^-1 x
      // This requires a more complex symbolic expression
      return Expression<QuadFormCholDerivLOp,
                        decltype(px.first), decltype(pl.first), decltype(pl.second)>{
          px.first, pl.first, pl.second};
    } else {
      // Both vary: sum of the two cases
      auto y = solveTriangular(pl.first, px.first);
      auto dy = solveTriangular(pl.first, px.second);
      auto x_contrib = Constant<2>{} * dot(y, dy);

      auto L_contrib = Expression<QuadFormCholDerivLOp,
                                  decltype(px.first), decltype(pl.first), decltype(pl.second)>{
          px.first, pl.first, pl.second};

      return x_contrib + L_contrib;
    }
  }
};

// ============================================================================
// Terminal rules for matrix symbols
// ============================================================================
//
// When differentiating w.r.t. a scalar variable, matrix symbols are treated
// as either:
//   - Independent of the variable (derivative = 0)
//   - Dependent on the variable (need special handling)
//
// For DimVectorSymbol and CholeskyCov/CorrSymbol, we check if they match
// the differentiation variable.

template <typename Var>
struct MatrixDiff {
  // DimVectorSymbol terminal: returns 0 unless it's the differentiation variable
  template <typename Id, typename DimTag>
  static constexpr auto terminal(DimVectorSymbol<Id, DimTag> v) {
    if constexpr (std::is_same_v<DimVectorSymbol<Id, DimTag>, Var>) {
      // This IS the differentiation variable - return identity marker
      return Constant<1>{};
    } else {
      return Constant<0>{};
    }
  }

  // CholeskyCovSymbol terminal
  template <typename Id, typename DimTag>
  static constexpr auto terminal(CholeskyCovSymbol<Id, DimTag>) {
    if constexpr (std::is_same_v<CholeskyCovSymbol<Id, DimTag>, Var>) {
      return Constant<1>{};
    } else {
      return Constant<0>{};
    }
  }

  // CholeskyCorrSymbol terminal
  template <typename Id, typename DimTag>
  static constexpr auto terminal(CholeskyCorrSymbol<Id, DimTag>) {
    if constexpr (std::is_same_v<CholeskyCorrSymbol<Id, DimTag>, Var>) {
      return Constant<1>{};
    } else {
      return Constant<0>{};
    }
  }
};

// ============================================================================
// Gradient computation for multivariate normal log-probability
// ============================================================================
//
// The multivariate normal log-density (up to constant) is:
//   log p(x | mu, Sigma) = -1/2 (x-mu)^T Sigma^-1 (x-mu) + log|det(Sigma)|^-1/2
//                        = -1/2 quadFormChol(x-mu, L) - logDetChol(L)
//                        where Sigma = L L^T
//
// Gradients:
//   d/dx: L^-T L^-1 (mu - x) = Sigma^-1 (mu - x)
//   d/dmu: Sigma^-1 (x - mu)
//   d/dL: (more complex, involves L^-1 and outer products)
//
// These are implemented as special functions that can be used in HMC/ADVI.

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
