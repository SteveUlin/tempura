#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/interpreter/simplify.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/recursive.h"

// ============================================================================
// diff.h - Symbolic differentiation via recursive rewrite rules
// ============================================================================
//
// Differentiation rules read like their mathematical definitions:
//
//   rrule(sin(f_), cos(f_) * rec(f_))        // d/dx[sin(f)] = cos(f) · f'
//   rrule(f_ * g_, rec(f_) * g_ + f_ * rec(g_))  // product rule
//
// The rec(f_) means "recursively differentiate whatever f_ matched".
// Use rrule() for rules with rec(), rule() for base cases without rec().
//
// Usage:
//   Symbol<struct X> x;
//   auto result = differentiate(x * sin(x), x);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Diff - Creates a differentiation strategy for a given variable
// ============================================================================

template <typename Var>
constexpr auto makeDiff() {
  // Pattern variables for expressions
  constexpr auto f_ = PatternVar<struct DiffF_>{};
  constexpr auto g_ = PatternVar<struct DiffG_>{};

  return recursive(
      // -----------------------------------------------------------------------
      // Binary operations
      // -----------------------------------------------------------------------
      rrule(f_ + g_, rec(f_) + rec(g_))
    | rrule(f_ - g_, rec(f_) - rec(g_))
    | rrule(f_ * g_, rec(f_) * g_ + f_ * rec(g_))
    | rrule(f_ / g_, (rec(f_) * g_ - f_ * rec(g_)) / (g_ * g_))
    | rrule(pow(f_, g_), pow(f_, g_) * (rec(g_) * log(f_) + g_ * rec(f_) / f_))
    | rrule(hypot(f_, g_), (f_ * rec(f_) + g_ * rec(g_)) / hypot(f_, g_))

      // -----------------------------------------------------------------------
      // Trig functions (chain rule built-in via rec)
      // -----------------------------------------------------------------------
    | rrule(sin(f_), cos(f_) * rec(f_))
    | rrule(cos(f_), -sin(f_) * rec(f_))
    | rrule(tan(f_), rec(f_) / pow(cos(f_), 2_c))

      // -----------------------------------------------------------------------
      // Inverse trig
      // -----------------------------------------------------------------------
    | rrule(asin(f_), rec(f_) / sqrt(1_c - f_ * f_))
    | rrule(acos(f_), -rec(f_) / sqrt(1_c - f_ * f_))
    | rrule(atan(f_), rec(f_) / (1_c + f_ * f_))

      // -----------------------------------------------------------------------
      // Hyperbolic
      // -----------------------------------------------------------------------
    | rrule(sinh(f_), cosh(f_) * rec(f_))
    | rrule(cosh(f_), sinh(f_) * rec(f_))
    | rrule(tanh(f_), rec(f_) / pow(cosh(f_), 2_c))

      // -----------------------------------------------------------------------
      // Exp/Log
      // -----------------------------------------------------------------------
    | rrule(exp(f_), exp(f_) * rec(f_))
    | rrule(log(f_), rec(f_) / f_)
    | rrule(sqrt(f_), rec(f_) / (2_c * sqrt(f_)))
    | rrule(log1p(f_), rec(f_) / (1_c + f_))
    | rrule(expm1(f_), exp(f_) * rec(f_))
    | rrule(log10(f_), rec(f_) / (f_ * log(10_c)))
    | rrule(log2(f_), rec(f_) / (f_ * log(2_c)))
    | rrule(exp2(f_), exp2(f_) * log(2_c) * rec(f_))
    | rrule(cbrt(f_), rec(f_) / (3_c * cbrt(f_) * cbrt(f_)))

      // -----------------------------------------------------------------------
      // Special functions
      // -----------------------------------------------------------------------
    | rrule(lgamma(f_), digamma(f_) * rec(f_))
    | rrule(erf(f_), 2_c / sqrt(pi) * exp(-f_ * f_) * rec(f_))
    | rrule(erfc(f_), -2_c / sqrt(pi) * exp(-f_ * f_) * rec(f_))

      // -----------------------------------------------------------------------
      // Misc unary
      // -----------------------------------------------------------------------
    | rrule(-f_, -rec(f_))
    | rrule(abs(f_), (f_ / abs(f_)) * rec(f_))
    | rule(floor(f_), 0_c)  // Not differentiable, but return 0
    | rule(ceil(f_), 0_c)

      // -----------------------------------------------------------------------
      // Nullary constants
      // -----------------------------------------------------------------------
    | rule(pi, 0_c)
    | rule(e, 0_c)

      // -----------------------------------------------------------------------
      // Terminal rules (order matters: specific before general)
      // -----------------------------------------------------------------------
    | rule(Var{}, 1_c)           // d/dx[x] = 1
    | rule(AnySymbol{}, 0_c)     // d/dx[y] = 0 (other symbols)
    | rule(AnyConstant{}, 0_c)   // d/dx[c] = 0 (includes Fraction)
    | rule(AnyFraction{}, 0_c)   // d/dx[n/d] = 0 (explicit for clarity)
    | rule(AnyLiteral{}, 0_c)    // d/dx[lit] = 0
  );
}

// ============================================================================
// Main API
// ============================================================================

template <Symbolic E, typename Var>
constexpr auto differentiate(E expr, Var) {
  constexpr auto diff = makeDiff<Var>();
  return simplify(diff.apply(expr));
}

// ReduceOver support: Recursive::apply() in recursive.h generically handles
// all ReduceOver nodes by recursing into the inner expression and rebuilding.
// No standalone overload needed — d/dx(Σf) = Σ(d/dx f) is automatic.

}  // namespace tempura::symbolic4
