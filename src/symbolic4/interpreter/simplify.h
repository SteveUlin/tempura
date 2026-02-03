#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/interpreter/partial_eval.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/combinator.h"
#include "symbolic4/strategy/pattern.h"
#include "symbolic4/strategy/recursive.h"
#include "symbolic4/strategy/rule.h"

// ============================================================================
// simplify.h - Algebraic simplification via rewrite rules
// ============================================================================
//
// Simplification is split into two phases:
//
// 1. Structural simplification (exact, no numerical computation):
//    - Identity elimination: x + 0 → x, x * 1 → x
//    - Zero annihilation: x * 0 → 0
//    - Self cancellation: x - x → 0, x / x → 1
//    - Double negation: -(-x) → x
//    - Inverse functions: exp(log(x)) → x, log(exp(x)) → x
//
// 2. Partial evaluation (optional, may introduce floating-point error):
//    - Constant folding: 2 + 3 → 5, sin(0) → 0
//    - Only applied to ground subexpressions (no free symbols)
//
// Usage:
//   simplify(expr)        // structural only (exact)
//   simplifyFull(expr)    // structural + constant folding
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Structural simplification rules
// ============================================================================
//
// These rules are exact - no numerical computation, no precision loss.
// Uses non-linear patterns: x_ - x_ matches only when both sides are
// the same type (compile-time equality).
//
// ============================================================================

inline constexpr auto makeSimplifyRules() {
  constexpr auto x_ = PatternVar<struct SimplifyX_>{};
  constexpr auto y_ = PatternVar<struct SimplifyY_>{};

  return
      // -----------------------------------------------------------------------
      // Addition: x + 0 → x, 0 + x → x
      // -----------------------------------------------------------------------
      rule(x_ + 0_c, x_)
    | rule(0_c + x_, x_)

      // -----------------------------------------------------------------------
      // Subtraction: x - 0 → x, x - x → 0
      // -----------------------------------------------------------------------
    | rule(x_ - 0_c, x_)
    | rule(x_ - x_, 0_c)

      // -----------------------------------------------------------------------
      // Multiplication: x * 1 → x, 1 * x → x, x * 0 → 0, 0 * x → 0
      // -----------------------------------------------------------------------
    | rule(x_ * 1_c, x_)
    | rule(1_c * x_, x_)
    | rule(x_ * 0_c, 0_c)
    | rule(0_c * x_, 0_c)

      // -----------------------------------------------------------------------
      // Division: x / 1 → x, 0 / x → 0, x / x → 1
      // -----------------------------------------------------------------------
    | rule(x_ / 1_c, x_)
    | rule(0_c / x_, 0_c)
    | rule(x_ / x_, 1_c)

      // -----------------------------------------------------------------------
      // Power: x^0 → 1, x^1 → x, 0^x → 0, 1^x → 1
      // -----------------------------------------------------------------------
    | rule(pow(x_, 0_c), 1_c)
    | rule(pow(x_, 1_c), x_)
    | rule(pow(0_c, x_), 0_c)
    | rule(pow(1_c, x_), 1_c)

      // -----------------------------------------------------------------------
      // Double negation: -(-x) → x
      // -----------------------------------------------------------------------
    | rule(-(-x_), x_)

      // -----------------------------------------------------------------------
      // Inverse function pairs
      // -----------------------------------------------------------------------
    | rule(exp(log(x_)), x_)
    | rule(log(exp(x_)), x_)
    | rule(sqrt(pow(x_, 2_c)), abs(x_))
    | rule(pow(sqrt(x_), 2_c), x_)

      // -----------------------------------------------------------------------
      // Trig identities
      // -----------------------------------------------------------------------
    | rule(sin(0_c), 0_c)
    | rule(cos(0_c), 1_c)
    | rule(tan(0_c), 0_c)

      // -----------------------------------------------------------------------
      // Hyperbolic identities
      // -----------------------------------------------------------------------
    | rule(sinh(0_c), 0_c)
    | rule(cosh(0_c), 1_c)
    | rule(tanh(0_c), 0_c)

      // -----------------------------------------------------------------------
      // Fraction identities
      // -----------------------------------------------------------------------
      // Note: Pattern matching compares reduced values, so Fraction<1,1>
      // matches Fraction<2,2>, Fraction<3,3>, etc. No need for duplicates.
      //
      // Addition/subtraction with zero
    | rule(x_ + Fraction<0, 1>{}, x_)
    | rule(Fraction<0, 1>{} + x_, x_)
    | rule(x_ - Fraction<0, 1>{}, x_)
      // Multiplication with one
    | rule(x_ * Fraction<1, 1>{}, x_)
    | rule(Fraction<1, 1>{} * x_, x_)
      // Division by one
    | rule(x_ / Fraction<1, 1>{}, x_)
      // Multiplication with zero
    | rule(x_ * Fraction<0, 1>{}, Fraction<0, 1>{})
    | rule(Fraction<0, 1>{} * x_, Fraction<0, 1>{})
      // Zero divided by anything
    | rule(Fraction<0, 1>{} / x_, Fraction<0, 1>{})
      // Power rules for fractions
    | rule(pow(x_, Fraction<0, 1>{}), Fraction<1, 1>{})
    | rule(pow(x_, Fraction<1, 1>{}), x_)
    | rule(pow(Fraction<0, 1>{}, x_), Fraction<0, 1>{})
    | rule(pow(Fraction<1, 1>{}, x_), Fraction<1, 1>{})
  ;
}

// The structural simplification strategy (no constant folding)
inline constexpr auto simplifyStrategy() {
  return innermost(makeSimplifyRules());
}

// ============================================================================
// Main API
// ============================================================================

// Structural simplification only (exact, no precision loss)
template <Symbolic E>
constexpr auto simplify(E expr) {
  return simplifyStrategy().apply(expr);
}

// Full simplification: structural + partial evaluation (may fold constants)
template <Symbolic E>
constexpr auto simplifyFull(E expr) {
  auto structural = simplify(expr);
  auto folded = partialEval(structural);
  return simplify(folded);  // Clean up after folding
}

}  // namespace tempura::symbolic4
