#pragma once

#include "symbolic4/compressed.h"
#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// diff.h - Symbolic differentiation
// ============================================================================
//
// This interpreter computes symbolic derivatives of expressions. Unlike
// evaluation (which produces numbers), differentiation produces NEW SYMBOLIC
// EXPRESSIONS representing the derivative.
//
// Usage:
//   Symbol<struct X> x;
//   auto expr = x * x;
//   auto deriv = diff(expr, x);  // Returns the type representing 2*x
//   evaluate(deriv, x = 3.0);    // Returns 6.0
//
// Why paramorphism (para) instead of catamorphism (fold)?
//
// Differentiation rules often need the ORIGINAL subexpressions, not just
// their derivatives. Consider the product rule:
//
//   d/dx(f * g) = f * (dg/dx) + (df/dx) * g
//
// To build this result, we need:
//   - f:  the original left child (to multiply by dg)
//   - g:  the original right child (to multiply by df)
//   - df: the derivative of f (result of recursion)
//   - dg: the derivative of g (result of recursion)
//
// Para provides exactly this: Pair{f, df} and Pair{g, dg}.
// Fold would only give us df and dg, losing f and g.
//
// Implemented rules:
//
//   Constant/other variable: d/dx(c) = 0, d/dx(y) = 0
//   Same variable:           d/dx(x) = 1
//   Addition:                d/dx(f + g) = df + dg
//   Subtraction:             d/dx(f - g) = df - dg
//   Negation:                d/dx(-f) = -df
//   Multiplication:          d/dx(f * g) = f*dg + df*g    (product rule)
//   Division:                d/dx(f/g) = (df*g - f*dg)/g² (quotient rule)
//   Power:                   d/dx(f^n) = n*f^(n-1)*df     (power rule, const exp)
//   Exponential:             d/dx(e^f) = e^f * df         (chain rule)
//   Logarithm:               d/dx(log(f)) = df/f
//   Sine:                    d/dx(sin(f)) = cos(f) * df
//   Cosine:                  d/dx(cos(f)) = -sin(f) * df
//   Tangent:                 d/dx(tan(f)) = df/cos²(f)
//   Square root:             d/dx(√f) = df/(2√f)
//
// Note: The result is NOT simplified. x + x becomes x*1 + 1*x, not 2*x.
// Simplification would be a separate interpreter pass.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Diff Interpreter - symbolic differentiation with respect to a variable
// ============================================================================
//
// Var is the Symbol type we're differentiating with respect to.
// The result is a new symbolic expression type (not a numeric value).

template <typename Var>
struct Diff {
  // Result is a symbolic expression type. We don't specify it here because
  // the actual return type varies based on the input expression structure.
  struct result_type_tag {};

  struct context_type {};

  // Terminal rule: derivatives of leaves
  //   d/dx(x) = 1  (differentiating with respect to ourselves)
  //   d/dx(c) = 0  (constants)
  //   d/dx(y) = 0  (different variables treated as constants)
  template <typename T>
  static constexpr auto terminal(T, context_type&) {
    if constexpr (std::is_same_v<T, Var>) {
      return Constant<1>{};  // dx/dx = 1
    } else {
      return Constant<0>{};  // Everything else is constant w.r.t. x
    }
  }

  // Dispatch to the appropriate differentiation rule based on operator
  // Para gives us Pair{original, derivative} for each child
  template <typename Op, typename... Ps>
  static constexpr auto combine(context_type&, Op, Ps... pairs) {
    return diffOp<Op>(pairs...);
  }

  // =========================================================================
  // Differentiation rules (diffOp overloads)
  // =========================================================================
  // Each rule receives Pair{f, df} for children where:
  //   - .first  = original expression (f, g)
  //   - .second = derivative (df, dg)
  // =========================================================================

  // Sum rule: d/dx(f + g) = df + dg
  template <typename Op, typename L, typename R>
    requires std::is_same_v<Op, AddOp>
  static constexpr auto diffOp(L lpair, R rpair) {
    return lpair.second + rpair.second;
  }

  // Difference rule: d/dx(f - g) = df - dg
  template <typename Op, typename L, typename R>
    requires std::is_same_v<Op, SubOp>
  static constexpr auto diffOp(L lpair, R rpair) {
    return lpair.second - rpair.second;
  }

  // Negation: d/dx(-f) = -df
  template <typename Op, typename P>
    requires std::is_same_v<Op, NegOp>
  static constexpr auto diffOp(P pair) {
    return -pair.second;
  }

  // Product rule: d/dx(f * g) = f*dg + df*g
  // Note: requires BOTH original children (f, g) AND their derivatives (df, dg)
  template <typename Op, typename L, typename R>
    requires std::is_same_v<Op, MulOp>
  static constexpr auto diffOp(L lpair, R rpair) {
    return lpair.first * rpair.second + lpair.second * rpair.first;
  }

  // Quotient rule: d/dx(f/g) = (df*g - f*dg) / g²
  template <typename Op, typename L, typename R>
    requires std::is_same_v<Op, DivOp>
  static constexpr auto diffOp(L lpair, R rpair) {
    return (lpair.second * rpair.first - lpair.first * rpair.second) /
           (rpair.first * rpair.first);
  }

  // Power rule: d/dx(f^n) = n * f^(n-1) * df
  // Note: Assumes exponent n is constant (doesn't depend on x)
  template <typename Op, typename L, typename R>
    requires std::is_same_v<Op, PowOp>
  static constexpr auto diffOp(L lpair, R rpair) {
    return rpair.first * pow(lpair.first, rpair.first - Constant<1>{}) *
           lpair.second;
  }

  // Chain rule for exp: d/dx(e^f) = e^f * df
  template <typename Op, typename P>
    requires std::is_same_v<Op, ExpOp>
  static constexpr auto diffOp(P pair) {
    return exp(pair.first) * pair.second;
  }

  // Logarithm: d/dx(log(f)) = df/f
  template <typename Op, typename P>
    requires std::is_same_v<Op, LogOp>
  static constexpr auto diffOp(P pair) {
    return pair.second / pair.first;
  }

  // Sine: d/dx(sin(f)) = cos(f) * df
  template <typename Op, typename P>
    requires std::is_same_v<Op, SinOp>
  static constexpr auto diffOp(P pair) {
    return cos(pair.first) * pair.second;
  }

  // Cosine: d/dx(cos(f)) = -sin(f) * df
  template <typename Op, typename P>
    requires std::is_same_v<Op, CosOp>
  static constexpr auto diffOp(P pair) {
    return -sin(pair.first) * pair.second;
  }

  // Tangent: d/dx(tan(f)) = sec²(f) * df = df / cos²(f)
  template <typename Op, typename P>
    requires std::is_same_v<Op, TanOp>
  static constexpr auto diffOp(P pair) {
    return pair.second / pow(cos(pair.first), Constant<2>{});
  }

  // Square root: d/dx(√f) = df / (2√f)
  template <typename Op, typename P>
    requires std::is_same_v<Op, SqrtOp>
  static constexpr auto diffOp(P pair) {
    return pair.second / (Constant<2>{} * sqrt(pair.first));
  }

  // Mathematical constants π and e: derivatives are 0
  template <typename Op>
    requires(std::is_same_v<Op, PiOp> || std::is_same_v<Op, EOp>)
  static constexpr auto diffOp() {
    return Constant<0>{};
  }
};

// ============================================================================
// Convenience function: diff(expr, var)
// ============================================================================
//
// The main user-facing API for differentiation.
//
// Usage:
//   auto deriv = diff(expr, x);  // differentiate with respect to x
//
// Note: Unlike evaluate(), diff() doesn't use the standard fold mechanism
// because the result type varies based on input (it's a new expression type,
// not a fixed type like double). We implement it directly here.

template <Symbolic E, typename Var>
constexpr auto diff(E expr, Var var);

namespace detail {

// Forward declarations for the recursive implementation
template <typename Var, Symbolic E>
constexpr auto diffImpl(E expr);

template <typename Var, typename Op, typename... Args, SizeT... Is>
constexpr auto diffExpression(Expression<Op, Args...> expr, IndexSequence<Is...>);

// Recursive implementation: builds Pair{child, diff(child)} and calls diffOp
template <typename Var, typename Op, typename... Args, SizeT... Is>
constexpr auto diffExpression(Expression<Op, Args...> expr, IndexSequence<Is...>) {
  return Diff<Var>::template diffOp<Op>(
      Pair{expr.template arg<Is>(), diffImpl<Var>(expr.template arg<Is>())}...);
}

// Main recursive diff: terminal base case + expression recursive case
template <typename Var, Symbolic E>
constexpr auto diffImpl(E expr) {
  if constexpr (is_terminal_v<E>) {
    // Base case: d/dx(x) = 1, d/dx(anything else) = 0
    if constexpr (std::is_same_v<E, Var>) {
      return Constant<1>{};
    } else {
      return Constant<0>{};
    }
  } else {
    // Recursive case: differentiate children, apply rule
    return diffExpression<Var>(expr, MakeIndexSequence<E::arity>{});
  }
}

}  // namespace detail

// Main entry point: differentiate expr with respect to var
template <Symbolic E, typename Var>
constexpr auto diff(E expr, [[maybe_unused]] Var var) {
  return detail::diffImpl<Var>(expr);
}

}  // namespace tempura::symbolic4
