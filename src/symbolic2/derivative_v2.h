#pragma once

#include "symbolic2/symbolic.h"
#include "pattern_matching.h"

// Improved derivative implementation using pattern matching RewriteSystem
//
// This version demonstrates how pattern matching can be used for symbolic
// differentiation, though the original approach with template specialization
// is already quite clean for this use case.

namespace tempura {

// Forward declaration
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var);

// =============================================================================
// BASE CASES - Constants and variables
// =============================================================================

// d/dx(x) = 1
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, Var{}))
constexpr auto diff(Expr /*expr*/, Var /*var*/) {
  return 1_c;
}

// d/dx(y) = 0 (where y is a different symbol)
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnySymbol{}) && !match(Expr{}, Var{}))
constexpr auto diff(Expr /*expr*/, Var /*var*/) {
  return 0_c;
}

// d/dx(c) = 0 (constant rule)
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyConstant{}))
constexpr auto diff(Expr /*expr*/, Var /*var*/) {
  return 0_c;
}

// d/dx(e) = 0 (Euler's number is a constant)
template <Symbolic Expr, Symbolic Var>
  requires(isSame<Expr, decltype(e)>)
constexpr auto diff(Expr /*expr*/, Var /*var*/) {
  return 0_c;
}

// d/dx(π) = 0 (pi is a constant)
template <Symbolic Expr, Symbolic Var>
  requires(isSame<Expr, decltype(π)>)
constexpr auto diff(Expr /*expr*/, Var /*var*/) {
  return 0_c;
}

// =============================================================================
// ARITHMETIC OPERATIONS
// =============================================================================

// Sum rule: d/dx(f + g) = df/dx + dg/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} + AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  return diff(left(expr), var) + diff(right(expr), var);
}

// Difference rule: d/dx(f - g) = df/dx - dg/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} - AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  return diff(left(expr), var) - diff(right(expr), var);
}

// Negation rule: d/dx(-f) = -df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, -AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  return -diff(operand(expr), var);
}

// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} * AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = left(expr);
  constexpr auto g = right(expr);
  return diff(f, var) * g + f * diff(g, var);
}

// Quotient rule: d/dx(f / g) = (df/dx * g - f * dg/dx) / g²
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} / AnyArg{}))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = left(expr);
  constexpr auto g = right(expr);
  return (diff(f, var) * g - f * diff(g, var)) / pow(g, 2_c);
}

// =============================================================================
// POWER AND ROOT FUNCTIONS
// =============================================================================

// Power rule: d/dx(f^n) = n * f^(n-1) * df/dx (chain rule)
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, pow(AnyArg{}, AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = left(expr);
  constexpr auto n = right(expr);
  return n * pow(f, n - 1_c) * diff(f, var);
}

// Square root: d/dx(√f) = 1/(2√f) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, sqrt(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return (1_c / (2_c * sqrt(f))) * diff(f, var);
}

// =============================================================================
// EXPONENTIAL AND LOGARITHMIC FUNCTIONS
// =============================================================================

// Exponential: d/dx(e^f) = e^f * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, exp(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return exp(f) * diff(f, var);
}

// Logarithm: d/dx(log(f)) = 1/f * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, log(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return (1_c / f) * diff(f, var);
}

// =============================================================================
// TRIGONOMETRIC FUNCTIONS
// =============================================================================

// Sine: d/dx(sin(f)) = cos(f) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, sin(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return cos(f) * diff(f, var);
}

// Cosine: d/dx(cos(f)) = -sin(f) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, cos(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return -sin(f) * diff(f, var);
}

// Tangent: d/dx(tan(f)) = sec²(f) * df/dx = 1/cos²(f) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, tan(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return (1_c / pow(cos(f), 2_c)) * diff(f, var);
}

// =============================================================================
// INVERSE TRIGONOMETRIC FUNCTIONS
// =============================================================================

// Arc sine: d/dx(asin(f)) = 1/√(1-f²) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, asin(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return (1_c / sqrt(1_c - pow(f, 2_c))) * diff(f, var);
}

// Arc cosine: d/dx(acos(f)) = -1/√(1-f²) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, acos(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return (-1_c / sqrt(1_c - pow(f, 2_c))) * diff(f, var);
}

// Arc tangent: d/dx(atan(f)) = 1/(1+f²) * df/dx
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, atan(AnyArg{})))
constexpr auto diff(Expr expr, Var var) {
  constexpr auto f = operand(expr);
  return (1_c / (1_c + pow(f, 2_c))) * diff(f, var);
}

// =============================================================================
// ALTERNATIVE: Rewrite-based differentiation (proof of concept)
// =============================================================================
//
// The current template-based approach is already very clean for differentiation.
// However, here's how you could use RewriteSystem if you wanted a more
// declarative approach for simple cases:
//
// constexpr auto BasicDiffRules = [](auto var) {
//   return RewriteSystem{
//     // Would need runtime context for 'var' comparison
//     // This is challenging because pattern matching is purely structural
//   };
// };
//
// The challenge with using RewriteSystem for differentiation is that diff()
// needs to know which variable we're differentiating with respect to,
// and that requires runtime context that pure pattern matching doesn't provide.
//
// Pattern matching works best for purely structural transformations that
// don't depend on external context (like the variable we're differentiating).
// =============================================================================

}  // namespace tempura
