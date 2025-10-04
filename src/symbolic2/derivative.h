#pragma once

#include "symbolic2/symbolic.h"

namespace tempura {

// Compute the derivative of a symbolic expression with respect to a variable.
//
// Implements standard calculus differentiation rules:
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
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var);

// Base cases

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

// Power rule: d/dx(f^n) = n * f^(n-1) * df/dx
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

}  // namespace tempura
