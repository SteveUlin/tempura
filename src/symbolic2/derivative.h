#pragma once

#include "symbolic2/constants.h"
#include "symbolic2/operators.h"
#include "symbolic2/symbolic_diff_notation.h"

namespace tempura {

// Compute the derivative of a symbolic expression with respect to a variable.
//
// Implements standard calculus differentiation rules using pure symbolic
// notation - no lambda wrappers required!
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
// Each rule is defined using pure symbolic expressions:
//   1. A pattern (e.g., f_ + g_)
//   2. A symbolic expression using diff_(expr, var_) for recursion
//
// This creates rules that look exactly like mathematical notation!
//
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var);

// =============================================================================
// DIFFERENTIATION RULES USING PURE SYMBOLIC NOTATION
// =============================================================================
//
// Each rule is defined as:
//   constexpr auto RuleName = SymbolicRecursiveRewrite{
//     pattern,
//     symbolic_expression
//   };
//
// The symbolic expression can use:
//   - Pattern variables directly (f_, g_, n_, etc.)
//   - diff_(expr, var_) for recursive differentiation
//   - var_ as a placeholder for the differentiation variable
//
// This creates clean, mathematical notation without any lambda boilerplate!

// =============================================================================
// ARITHMETIC RULES
// =============================================================================

// Sum rule: d/dx(f + g) = df/dx + dg/dx
constexpr auto DiffSum =
    SymbolicRecursiveRewrite{f_ + g_, diff_(f_, var_) + diff_(g_, var_)};

// Difference rule: d/dx(f - g) = df/dx - dg/dx
constexpr auto DiffDifference =
    SymbolicRecursiveRewrite{f_ - g_, diff_(f_, var_) - diff_(g_, var_)};

// Negation rule: d/dx(-f) = -df/dx
constexpr auto DiffNegation = SymbolicRecursiveRewrite{-f_, -diff_(f_, var_)};

// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
constexpr auto DiffProduct = SymbolicRecursiveRewrite{
    f_ * g_, diff_(f_, var_) * g_ + f_* diff_(g_, var_)};

// Quotient rule: d/dx(f / g) = (df/dx * g - f * dg/dx) / g²
constexpr auto DiffQuotient = SymbolicRecursiveRewrite{
    f_ / g_, (diff_(f_, var_) * g_ - f_ * diff_(g_, var_)) / pow(g_, 2_c)};

// =============================================================================
// POWER AND EXPONENTIAL RULES
// =============================================================================

// Power rule: d/dx(f^n) = n * f^(n-1) * df/dx (with chain rule)
constexpr auto DiffPower = SymbolicRecursiveRewrite{
    pow(f_, n_), n_* pow(f_, n_ - 1_c) * diff_(f_, var_)};

// Square root: d/dx(√f) = 1/(2√f) * df/dx (with chain rule)
constexpr auto DiffSqrt = SymbolicRecursiveRewrite{
    sqrt(f_), (1_c / (2_c * sqrt(f_))) * diff_(f_, var_)};

// Exponential: d/dx(e^f) = e^f * df/dx (with chain rule)
constexpr auto DiffExp =
    SymbolicRecursiveRewrite{exp(f_), exp(f_) * diff_(f_, var_)};

// Logarithm: d/dx(log(f)) = 1/f * df/dx (with chain rule)
constexpr auto DiffLog =
    SymbolicRecursiveRewrite{log(f_), (1_c / f_) * diff_(f_, var_)};

// =============================================================================
// TRIGONOMETRIC RULES
// =============================================================================

// Sine: d/dx(sin(f)) = cos(f) * df/dx (with chain rule)
constexpr auto DiffSin =
    SymbolicRecursiveRewrite{sin(f_), cos(f_) * diff_(f_, var_)};

// Cosine: d/dx(cos(f)) = -sin(f) * df/dx (with chain rule)
constexpr auto DiffCos =
    SymbolicRecursiveRewrite{cos(f_), -sin(f_) * diff_(f_, var_)};

// Tangent: d/dx(tan(f)) = sec²(f) * df/dx = 1/cos²(f) * df/dx (with chain rule)
constexpr auto DiffTan = SymbolicRecursiveRewrite{
    tan(f_), (1_c / pow(cos(f_), 2_c)) * diff_(f_, var_)};

// =============================================================================
// INVERSE TRIGONOMETRIC RULES
// =============================================================================

// Arc sine: d/dx(asin(f)) = 1/√(1-f²) * df/dx (with chain rule)
constexpr auto DiffAsin = SymbolicRecursiveRewrite{
    asin(f_), (1_c / sqrt(1_c - pow(f_, 2_c))) * diff_(f_, var_)};

// Arc cosine: d/dx(acos(f)) = -1/√(1-f²) * df/dx (with chain rule)
constexpr auto DiffAcos = SymbolicRecursiveRewrite{
    acos(f_), (-1_c / sqrt(1_c - pow(f_, 2_c))) * diff_(f_, var_)};

// Arc tangent: d/dx(atan(f)) = 1/(1+f²) * df/dx (with chain rule)
constexpr auto DiffAtan = SymbolicRecursiveRewrite{
    atan(f_), (1_c / (1_c + pow(f_, 2_c))) * diff_(f_, var_)};

// =============================================================================
// BASE CASE RULES
// =============================================================================

// Constant rule: d/dx(c) = 0 for any constant
constexpr auto DiffConstant = SymbolicRecursiveRewrite{𝐜, 0_c};

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
