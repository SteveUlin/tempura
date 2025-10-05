#pragma once

#include "symbolic2/constants.h"
#include "symbolic2/operators.h"
#include "symbolic2/symbolic_diff_notation.h"

namespace tempura {

// Symbolic differentiation using pattern-based rewrite rules.
//
// Rules are expressed as pure symbolic expressions without lambda boilerplate:
//   ∂/∂x(f+g) = ∂f/∂x + ∂g/∂x
//
// encoded as:
//   Rewrite{f_ + g_, diff_(f_, var_) + diff_(g_, var_)}
//
// Example:
//   Symbol x;
//   auto f = x*x + 2_c*x + 1_c;
//   auto df_dx = diff(f, x);  // Returns: 2·x + 2
//
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var);

// Differentiation rules using symbolic notation
// Pattern variables (f_, g_, n_) match subexpressions
// diff_(expr, var_) encodes recursive differentiation

// Arithmetic differentiation rules
constexpr auto DiffSum =
    SymbolicRecursiveRewrite{f_ + g_, diff_(f_, var_) + diff_(g_, var_)};

constexpr auto DiffDifference =
    SymbolicRecursiveRewrite{f_ - g_, diff_(f_, var_) - diff_(g_, var_)};

constexpr auto DiffNegation = SymbolicRecursiveRewrite{-f_, -diff_(f_, var_)};

constexpr auto DiffProduct = SymbolicRecursiveRewrite{
    f_ * g_, diff_(f_, var_) * g_ + f_* diff_(g_, var_)};

constexpr auto DiffQuotient = SymbolicRecursiveRewrite{
    f_ / g_, (diff_(f_, var_) * g_ - f_ * diff_(g_, var_)) / pow(g_, 2_c)};

// Power and exponential rules
constexpr auto DiffPower = SymbolicRecursiveRewrite{
    pow(f_, n_), n_* pow(f_, n_ - 1_c) * diff_(f_, var_)};

constexpr auto DiffSqrt = SymbolicRecursiveRewrite{
    sqrt(f_), (1_c / (2_c * sqrt(f_))) * diff_(f_, var_)};

constexpr auto DiffExp =
    SymbolicRecursiveRewrite{exp(f_), exp(f_) * diff_(f_, var_)};

constexpr auto DiffLog =
    SymbolicRecursiveRewrite{log(f_), (1_c / f_) * diff_(f_, var_)};

// Trigonometric rules
constexpr auto DiffSin =
    SymbolicRecursiveRewrite{sin(f_), cos(f_) * diff_(f_, var_)};

constexpr auto DiffCos =
    SymbolicRecursiveRewrite{cos(f_), -sin(f_) * diff_(f_, var_)};

constexpr auto DiffTan = SymbolicRecursiveRewrite{
    tan(f_), (1_c / pow(cos(f_), 2_c)) * diff_(f_, var_)};

// Inverse trigonometric rules
constexpr auto DiffAsin = SymbolicRecursiveRewrite{
    asin(f_), (1_c / sqrt(1_c - pow(f_, 2_c))) * diff_(f_, var_)};

constexpr auto DiffAcos = SymbolicRecursiveRewrite{
    acos(f_), (-1_c / sqrt(1_c - pow(f_, 2_c))) * diff_(f_, var_)};

constexpr auto DiffAtan = SymbolicRecursiveRewrite{
    atan(f_), (1_c / (1_c + pow(f_, 2_c))) * diff_(f_, var_)};

// Hyperbolic function rules
constexpr auto DiffSinh =
    SymbolicRecursiveRewrite{sinh(f_), cosh(f_) * diff_(f_, var_)};

constexpr auto DiffCosh =
    SymbolicRecursiveRewrite{cosh(f_), sinh(f_) * diff_(f_, var_)};

constexpr auto DiffTanh = SymbolicRecursiveRewrite{
    tanh(f_), (1_c / pow(cosh(f_), 2_c)) * diff_(f_, var_)};

// Base case: constants differentiate to zero
constexpr auto DiffConstant = SymbolicRecursiveRewrite{𝐜, 0_c};

// Complete differentiation rule system
constexpr auto DiffRules = RecursiveRewriteSystem{
    DiffConstant, DiffSum,   DiffDifference, DiffNegation, DiffProduct,
    DiffQuotient, DiffPower, DiffSqrt,       DiffExp,      DiffLog,
    DiffSin,      DiffCos,   DiffTan,        DiffAsin,     DiffAcos,
    DiffAtan,     DiffSinh,  DiffCosh,       DiffTanh};

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
