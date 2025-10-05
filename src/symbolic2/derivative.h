#pragma once

#include "symbolic2/constants.h"
#include "symbolic2/operators.h"
#include "symbolic2/recursive_rewrite.h"

namespace tempura {

// Compute the derivative of a symbolic expression with respect to a variable.
//
// Implements standard calculus differentiation rules using the RecursiveRewrite
// system for clean, declarative rule definitions:
//
// - Constant rule: d/dx(c) = 0
// - Power rule: d/dx(x^n) = n*x^(n-1)
// - Sum rule: d/dx(f+g) = df/dx + dg/dx
// - Product rule: d/dx(f*g) = df/dx*g + f*dg/dx
// - Quotient rule: d/dx(f/g) = (df/dx*g - f*dg/dx)/g²
// - Chain rule: d/dx(f(g(x))) = f'(g(x))*g'(x)
//
// Example:
//   Symbol x;
//   auto expr = x * x + 2_c * x + 1_c;
//   auto derivative = diff(expr, x);  // Returns: 2*x + 2
//
// Each rule is defined as a RecursiveRewrite with:
//   1. A pattern (e.g., x_ + y_)
//   2. A lambda that receives (context, diff_fn, var) and returns the result
//
// This makes rules easy to write and understand without template
// metaprogramming!
//
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var);

// =============================================================================
// DIFFERENTIATION RULES USING RECURSIVE REWRITE SYSTEM
// =============================================================================
//
// Each rule is defined as:
//   constexpr auto RuleName = RecursiveRewrite{
//     pattern,
//     [](auto ctx, auto diff_fn, auto var) { return transformation; }
//   };
//
// The lambda receives:
//   - ctx: Binding context with matched pattern variables
//   - diff_fn: Recursive differentiation function to call on subexpressions
//   - var: The variable we're differentiating with respect to
//
// This approach is much easier to read and write than template specializations!
//
// ALL rules (including base cases) are now in the pattern matching system!

// =============================================================================
// =============================================================================
// ARITHMETIC RULES
// =============================================================================

// Sum rule: d/dx(f + g) = df/dx + dg/dx
constexpr auto DiffSum =
    RecursiveRewrite{x_ + y_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto g = get(ctx, y_);
                       return diff_fn(f, var) + diff_fn(g, var);
                     }};

// Difference rule: d/dx(f - g) = df/dx - dg/dx
constexpr auto DiffDifference =
    RecursiveRewrite{x_ - y_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto g = get(ctx, y_);
                       return diff_fn(f, var) - diff_fn(g, var);
                     }};

// Negation rule: d/dx(-f) = -df/dx
constexpr auto DiffNegation =
    RecursiveRewrite{-x_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return -diff_fn(f, var);
                     }};

// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
constexpr auto DiffProduct =
    RecursiveRewrite{x_ * y_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto g = get(ctx, y_);
                       return diff_fn(f, var) * g + f * diff_fn(g, var);
                     }};

// Quotient rule: d/dx(f / g) = (df/dx * g - f * dg/dx) / g²
constexpr auto DiffQuotient = RecursiveRewrite{
    x_ / y_, [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, x_);
      constexpr auto g = get(ctx, y_);
      return (diff_fn(f, var) * g - f * diff_fn(g, var)) / pow(g, 2_c);
    }};

// =============================================================================
// POWER AND EXPONENTIAL RULES
// =============================================================================

// Power rule: d/dx(f^n) = n * f^(n-1) * df/dx (with chain rule)
constexpr auto DiffPower =
    RecursiveRewrite{pow(x_, y_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto n = get(ctx, y_);
                       return n * pow(f, n - 1_c) * diff_fn(f, var);
                     }};

// Square root: d/dx(√f) = 1/(2√f) * df/dx (with chain rule)
constexpr auto DiffSqrt =
    RecursiveRewrite{sqrt(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return (1_c / (2_c * sqrt(f))) * diff_fn(f, var);
                     }};

// Exponential: d/dx(e^f) = e^f * df/dx (with chain rule)
constexpr auto DiffExp =
    RecursiveRewrite{exp(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return exp(f) * diff_fn(f, var);
                     }};

// Logarithm: d/dx(log(f)) = 1/f * df/dx (with chain rule)
constexpr auto DiffLog =
    RecursiveRewrite{log(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return (1_c / f) * diff_fn(f, var);
                     }};

// =============================================================================
// TRIGONOMETRIC RULES
// =============================================================================

// Sine: d/dx(sin(f)) = cos(f) * df/dx (with chain rule)
constexpr auto DiffSin =
    RecursiveRewrite{sin(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return cos(f) * diff_fn(f, var);
                     }};

// Cosine: d/dx(cos(f)) = -sin(f) * df/dx (with chain rule)
constexpr auto DiffCos =
    RecursiveRewrite{cos(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return -sin(f) * diff_fn(f, var);
                     }};

// Tangent: d/dx(tan(f)) = sec²(f) * df/dx = 1/cos²(f) * df/dx (with chain rule)
constexpr auto DiffTan =
    RecursiveRewrite{tan(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return (1_c / pow(cos(f), 2_c)) * diff_fn(f, var);
                     }};

// =============================================================================
// INVERSE TRIGONOMETRIC RULES
// =============================================================================

// Arc sine: d/dx(asin(f)) = 1/√(1-f²) * df/dx (with chain rule)
constexpr auto DiffAsin =
    RecursiveRewrite{asin(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return (1_c / sqrt(1_c - pow(f, 2_c))) * diff_fn(f, var);
                     }};

// Arc cosine: d/dx(acos(f)) = -1/√(1-f²) * df/dx (with chain rule)
constexpr auto DiffAcos = RecursiveRewrite{
    acos(x_), [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, x_);
      return (-1_c / sqrt(1_c - pow(f, 2_c))) * diff_fn(f, var);
    }};

// Arc tangent: d/dx(atan(f)) = 1/(1+f²) * df/dx (with chain rule)
constexpr auto DiffAtan =
    RecursiveRewrite{atan(x_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       return (1_c / (1_c + pow(f, 2_c))) * diff_fn(f, var);
                     }};

// =============================================================================
// REWRITE SYSTEM
// =============================================================================

// All differentiation rules collected into a single rewrite system
constexpr auto DiffRules = RecursiveRewriteSystem{
    DiffSum,   DiffDifference, DiffNegation, DiffProduct, DiffQuotient,
    DiffPower, DiffSqrt,       DiffExp,      DiffLog,     DiffSin,
    DiffCos,   DiffTan,        DiffAsin,     DiffAcos,    DiffAtan};

// =============================================================================
// MAIN DIFF FUNCTION
// =============================================================================

// Compute the derivative of a symbolic expression with respect to a variable.
//
// This function handles the base cases (constants, variables, symbols),
// then delegates all compound expressions to the RecursiveRewriteSystem.
//
// Why not put these base cases in the rewrite system?
// - The "d/dx(x) = 1" rule requires checking if the expression EQUALS the
//   variable parameter, which needs runtime comparison against var parameter
// - Symbols that aren't the target variable return 0, but we need to exclude
//   the case where they ARE the target variable (already handled above)
// - Numeric constants would need a general "AnyConstant" wildcard which the
//   pattern matching system doesn't currently support
//
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var) {
  // Base case: d/dx(x) = 1
  // This must be checked first because it requires matching against the
  // variable parameter itself, not a pattern
  if constexpr (match(expr, var)) {
    return 1_c;
  }
  // Base case: d/dx(c) = 0 for numeric constants
  // Check if expr is a Constant type
  else if constexpr (requires { Expr::value; }) {
    return 0_c;
  }
  // Base case: d/dx(y) = 0 where y is a different symbol
  // We already handled the case where it matches var, so any other symbol is 0
  else if constexpr (match(expr, AnySymbol{})) {
    return 0_c;
  }
  // Base case: Mathematical constants (e, π) are treated as constants
  else if constexpr (isSame<Expr, decltype(e)> || isSame<Expr, decltype(π)>) {
    return 0_c;
  }
  // All compound expressions: delegate to the recursive rewrite system
  else {
    auto diff_fn = []<Symbolic E, Symbolic V>(E e, V v) { return diff(e, v); };
    return DiffRules.apply(expr, diff_fn, var);
  }
}

}  // namespace tempura
