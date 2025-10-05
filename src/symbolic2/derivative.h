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
    RecursiveRewrite{f_ + g_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       constexpr auto g = get(ctx, g_);
                       return diff_fn(f, var) + diff_fn(g, var);
                     }};

// Difference rule: d/dx(f - g) = df/dx - dg/dx
constexpr auto DiffDifference =
    RecursiveRewrite{f_ - g_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       constexpr auto g = get(ctx, g_);
                       return diff_fn(f, var) - diff_fn(g, var);
                     }};

// Negation rule: d/dx(-f) = -df/dx
constexpr auto DiffNegation =
    RecursiveRewrite{-f_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return -diff_fn(f, var);
                     }};

// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
constexpr auto DiffProduct =
    RecursiveRewrite{f_ * g_, [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       constexpr auto g = get(ctx, g_);
                       return diff_fn(f, var) * g + f * diff_fn(g, var);
                     }};

// Quotient rule: d/dx(f / g) = (df/dx * g - f * dg/dx) / g²
constexpr auto DiffQuotient = RecursiveRewrite{
    f_ / g_, [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, f_);
      constexpr auto g = get(ctx, g_);
      return (diff_fn(f, var) * g - f * diff_fn(g, var)) / pow(g, 2_c);
    }};

// =============================================================================
// POWER AND EXPONENTIAL RULES
// =============================================================================

// Power rule: d/dx(f^n) = n * f^(n-1) * df/dx (with chain rule)
constexpr auto DiffPower =
    RecursiveRewrite{pow(f_, n_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       constexpr auto n = get(ctx, n_);
                       return n * pow(f, n - 1_c) * diff_fn(f, var);
                     }};

// Square root: d/dx(√f) = 1/(2√f) * df/dx (with chain rule)
constexpr auto DiffSqrt =
    RecursiveRewrite{sqrt(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return (1_c / (2_c * sqrt(f))) * diff_fn(f, var);
                     }};

// Exponential: d/dx(e^f) = e^f * df/dx (with chain rule)
constexpr auto DiffExp =
    RecursiveRewrite{exp(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return exp(f) * diff_fn(f, var);
                     }};

// Logarithm: d/dx(log(f)) = 1/f * df/dx (with chain rule)
constexpr auto DiffLog =
    RecursiveRewrite{log(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return (1_c / f) * diff_fn(f, var);
                     }};

// =============================================================================
// TRIGONOMETRIC RULES
// =============================================================================

// Sine: d/dx(sin(f)) = cos(f) * df/dx (with chain rule)
constexpr auto DiffSin =
    RecursiveRewrite{sin(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return cos(f) * diff_fn(f, var);
                     }};

// Cosine: d/dx(cos(f)) = -sin(f) * df/dx (with chain rule)
constexpr auto DiffCos =
    RecursiveRewrite{cos(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return -sin(f) * diff_fn(f, var);
                     }};

// Tangent: d/dx(tan(f)) = sec²(f) * df/dx = 1/cos²(f) * df/dx (with chain rule)
constexpr auto DiffTan =
    RecursiveRewrite{tan(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return (1_c / pow(cos(f), 2_c)) * diff_fn(f, var);
                     }};

// =============================================================================
// INVERSE TRIGONOMETRIC RULES
// =============================================================================

// Arc sine: d/dx(asin(f)) = 1/√(1-f²) * df/dx (with chain rule)
constexpr auto DiffAsin =
    RecursiveRewrite{asin(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return (1_c / sqrt(1_c - pow(f, 2_c))) * diff_fn(f, var);
                     }};

// Arc cosine: d/dx(acos(f)) = -1/√(1-f²) * df/dx (with chain rule)
constexpr auto DiffAcos = RecursiveRewrite{
    acos(f_), [](auto ctx, auto diff_fn, auto var) {
      constexpr auto f = get(ctx, f_);
      return (-1_c / sqrt(1_c - pow(f, 2_c))) * diff_fn(f, var);
    }};

// Arc tangent: d/dx(atan(f)) = 1/(1+f²) * df/dx (with chain rule)
constexpr auto DiffAtan =
    RecursiveRewrite{atan(f_), [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, f_);
                       return (1_c / (1_c + pow(f, 2_c))) * diff_fn(f, var);
                     }};

// =============================================================================
// BASE CASE RULES
// =============================================================================

// Constant rule: d/dx(c) = 0 for any constant
constexpr auto DiffConstant =
    RecursiveRewrite{𝐜, [](auto, auto, auto) { return 0_c; }};

// =============================================================================
// REWRITE SYSTEM
// =============================================================================

// All differentiation rules collected into a single rewrite system
constexpr auto DiffRules = RecursiveRewriteSystem{
    DiffConstant, DiffSum,      DiffDifference, DiffNegation,
    DiffProduct,  DiffQuotient, DiffPower,      DiffSqrt,
    DiffExp,      DiffLog,      DiffSin,        DiffCos,
    DiffTan,      DiffAsin,     DiffAcos,       DiffAtan};

// =============================================================================
// MAIN DIFF FUNCTION
// =============================================================================

// Compute the derivative of a symbolic expression with respect to a variable.
//
// This function handles special cases that can't be expressed as patterns,
// then delegates everything else to the RecursiveRewriteSystem.
//
// Why keep these cases outside the rewrite system?
// - d/dx(x) = 1: Requires checking if expr EQUALS the var parameter
//   (runtime comparison against function parameter, not a static pattern)
// - d/dx(y) = 0 for symbols: Must exclude the case where y equals var
//   (already handled above, so any remaining symbol is a different variable)
//
// Everything else (including constants) is handled by DiffRules!
//
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var) {
  // Special case: d/dx(x) = 1
  // Must check against the var parameter itself (not a static pattern)
  if constexpr (match(expr, var)) {
    return 1_c;
  }
  // Special case: d/dx(y) = 0 where y is a different symbol
  // Uses the 𝐬 wildcard (AnySymbol) - var case already handled above
  else if constexpr (match(expr, 𝐬)) {
    return 0_c;
  }
  // All other cases: delegate to the recursive rewrite system
  // This includes constants (via DiffConstant) and all compound expressions
  else {
    auto diff_fn = []<Symbolic E, Symbolic V>(E e, V v) { return diff(e, v); };
    return DiffRules.apply(expr, diff_fn, var);
  }
}

}  // namespace tempura
