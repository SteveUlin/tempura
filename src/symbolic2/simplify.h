#pragma once

#include "accessors.h"
#include "constants.h"
#include "core.h"
#include "evaluate.h"
#include "matching.h"
#include "operators.h"
#include "ordering.h"

// Simplification rules for symbolic expressions

namespace tempura {

// --- Simplification Rules ---

template <Symbolic S>
constexpr auto simplifySymbol(S sym);

// Evaluate a Symbolic expression if every term is Constant<...>.
template <typename Op, Symbolic... Args>
  requires((match(Args{}, AnyConstant{}) && ...) && sizeof...(Args) > 0)
constexpr auto evalConstantExpr(Expression<Op, Args...> expr) {
  return Constant<evaluate(expr, BinderPack{})>{};
}

template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  constexpr auto base = left(expr);
  constexpr auto exponent = right(expr);

  // x ^ 0 == 1
  if constexpr (match(exponent, 0_c)) {
    return 1_c;
  }
  // x ^ 1 == x
  else if constexpr (match(exponent, 1_c)) {
    return base;
  }
  // 1 ^ x == 1
  else if constexpr (match(base, 1_c)) {
    return 1_c;
  }
  // 0 ^ x == 0
  else if constexpr (match(base, 0_c)) {
    return 0_c;
  }

  // (x ^ a) ^ b == x ^ (a * b)
  else if constexpr (match(expr, pow(pow(AnyArg{}, AnyArg{}), AnyArg{}))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(expr);
    return pow(x, a * b);  // Don't recurse - let outer loop handle it
  }

  // default
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  // 0 + x = x
  if constexpr (match(expr, 0_c + AnyArg{})) {
    return right(expr);
  }

  // x + 0 = x
  else if constexpr (match(expr, AnyArg{} + 0_c)) {
    return left(expr);
  }

  // x + x = x * 2
  else if constexpr (match(left(expr), right(expr))) {
    constexpr auto x = left(expr);
    return x * 2_c;  // Don't recurse - let outer loop handle it
  }

  // Canonical Form
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    return right(expr) +
           left(expr);  // Don't recurse - let outer loop handle it
  }

  // Associativity - defer recursive simplification to outer loop
  // If b < c
  // (a + c) + b = (a + b) + c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) + AnyArg{}) &&
                     symbolicLessThan(right(expr), right(left(expr)))) {
    constexpr auto a = left(left(expr));
    constexpr auto c = right(left(expr));
    constexpr auto b = right(expr);
    return (a + b) + c;  // Don't recurse - let outer loop handle it
  }

  // Factoring

  // x * a + x = x * (a + 1);
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{}) &&
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    return x * (a + 1_c);  // Don't recurse - let outer loop handle it
  }

  // x * a + x * b = x * (a + b);
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{} * AnyArg{}) &&
                     match(left(left(expr)), left(right(expr)))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    return x * (a + b);  // Don't recurse - let outer loop handle it
  }

  // Associativity: (a + b) + c = a + (b + c) if it helps
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) + AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    return a + (b + c);  // Don't recurse - let outer loop handle it
  }

  // default
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} * AnyArg{}))
constexpr auto multiplicationIdentities(S expr) {
  // 0 * x = 0
  if constexpr (match(expr, 0_c * AnyArg{})) {
    return 0_c;
  }
  // x * 0 = 0
  else if constexpr (match(expr, AnyArg{} * 0_c)) {
    return 0_c;
  }
  // 1 * x = x
  else if constexpr (match(expr, 1_c * AnyArg{})) {
    return right(expr);
  }
  // x * 1 = x
  else if constexpr (match(expr, AnyArg{} * 1_c)) {
    return left(expr);
  }

  // x * xᵃ = xᵃ⁺¹
  else if constexpr (match(expr, AnyArg{} * pow(AnyArg{}, AnyArg{})) and
                     match(left(expr), left(right(expr)))) {
    constexpr auto x = left(expr);
    constexpr auto a = right(right(expr));
    return pow(x, a + 1_c);  // Don't recurse - let outer loop handle it
  }

  // xᵃ * x = xᵃ⁺¹
  else if constexpr (match(expr, pow(AnyArg{}, AnyArg{}) * AnyArg{}) and
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    return pow(x, a + 1_c);  // Don't recurse - let outer loop handle it
  }

  // xᵃ * xᵇ = xᵃ⁺ᵇ
  else if constexpr (match(expr, (pow(AnyArg{}, AnyArg{}) *
                                  pow(AnyArg{}, AnyArg{}))) and
                     (match(left(left(expr)), left(right(expr))))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    return pow(x, a + b);  // Don't recurse - let outer loop handle it
  }

  // Distributive Property
  // (a + b) * c = a * c + b * c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) * AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    return (a * c) + (b * c);  // Don't recurse - let outer loop handle it
  }

  // a * (b + c) = a * b + a * c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} + AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    return (a * b) + (a * c);  // Don't recurse - let outer loop handle it
  }

  // Canonical Form
  // if a < b
  // b * a = a * b
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    constexpr auto a = left(expr);
    constexpr auto b = right(expr);
    return b * a;  // Don't recurse - let outer loop handle it
  }

  // Associativity: a * (b * c) = (a * b) * c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} * AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    return (a * b) * c;  // Don't recurse - let outer loop handle it
  }

  // Associativity: if b < c then (a * c) * b = (a * b) * c
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{}) and
                     symbolicLessThan(right(expr), right(left(expr)))) {
    constexpr auto a = left(left(expr));
    constexpr auto c = right(left(expr));
    constexpr auto b = right(expr);
    return (a * b) * c;  // Don't recurse - let outer loop handle it
  }

  // Associativity: (a * b) * c = a * (b * c) if it helps
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    return a * (b * c);  // Don't recurse - let outer loop handle it
  }

  // default
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} - AnyArg{}))
constexpr auto subtractionIdentities(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a + simplifySymbol(Constant<-1>{} * b));
}

template <Symbolic S>
  requires(match(S{}, AnyArg{} / AnyArg{}))
constexpr auto divisionIdentities(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a * simplifySymbol(pow(b, Constant<-1>{})));
}

template <Symbolic S>
  requires(match(S{}, exp(AnyArg{})))
constexpr auto expIdentities(S expr) {
  // exp(log(x)) == x
  if constexpr (match(expr, exp(log(AnyArg{})))) {
    return operand(
        operand(expr));  // operand(exp(...)) = log(...), operand(log(...)) = x
  }

  // exp(x) = e^x (convert to power form)
  else {
    return pow(e, operand(expr));  // Don't recurse - let outer loop handle it
  }
}

template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  // log(1) == 0
  if constexpr (match(expr, log(1_c))) {
    return 0_c;
  }

  // log(e) == 1
  else if constexpr (match(expr, log(e))) {
    return 1_c;
  }

  // log(x^a) == a * log(x)
  else if constexpr (match(expr, log(pow(AnyArg{}, AnyArg{})))) {
    constexpr auto x = left(operand(expr));
    constexpr auto a = right(operand(expr));
    return simplifySymbol(a * simplifySymbol(log(x)));
  }

  // log(a * b) == log(a) + log(b)
  else if constexpr (match(expr, log(AnyArg{} * AnyArg{}))) {
    constexpr auto a = left(operand(expr));
    constexpr auto b = right(operand(expr));
    return simplifySymbol(simplifySymbol(log(a)) + simplifySymbol(log(b)));
  }

  // log(a / b) == log(a) - log(b)
  else if constexpr (match(expr, log(AnyArg{} / AnyArg{}))) {
    constexpr auto a = left(operand(expr));
    constexpr auto b = right(operand(expr));
    return simplifySymbol(simplifySymbol(log(a)) - simplifySymbol(log(b)));
  }

  // log(exp(x)) == x
  else if constexpr (match(expr, log(exp(AnyArg{})))) {
    return operand(operand(expr));
  }

  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, sin(AnyArg{})))
constexpr auto sinIdentities(S expr) {
  // sin(π/2) = 1
  if constexpr (match(expr, sin(π * 0.5_c))) {
    return 1_c;
  }
  // sin(π) = 0
  else if constexpr (match(expr, sin(π))) {
    return 0_c;
  }
  // sin(3π/2) = -1
  else if constexpr (match(expr, sin(π * 1.5_c))) {
    return Constant<-1>{};
  }
  // TODO: Make this reduction more general, maybe any negative number?
  // sin(-x) = -sin(x)
  else if constexpr (match(expr, sin(AnyArg{} * Constant<-1>{}))) {
    return Constant<-1>{} * simplifySymbol(sin(operand(expr)));
  }
  // TODO: sin(x + 2π) = sin(x)
  else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, cos(AnyArg{})))
constexpr auto cosIdentities(S expr) {
  // cos(π/2) = 0
  if constexpr (match(expr, cos(π * 0.5_c))) {
    return 0_c;
  }
  // cos(π) = -1
  else if constexpr (match(expr, cos(π))) {
    return Constant<-1>{};
  }
  // cos(3π/2) = 0
  else if constexpr (match(expr, cos(π * 1.5_c))) {
    return 0_c;
  } else {
    return expr;
  }
}

template <Symbolic S>
  requires(match(S{}, tan(AnyArg{})))
constexpr auto tanIdentities(S expr) {
  // tan(π) = 0
  if constexpr (match(expr, tan(π))) {
    return 0_c;
  } else {
    return expr;
  }
}

// Forward declaration for depth-limited simplification
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym);

// Simplify a single term (no depth limit for backward compatibility)
template <Symbolic S>
constexpr auto simplifySymbol(S sym) {
  return simplifySymbolWithDepth<S, 0>(sym);
}

// Simplify a single term with depth tracking
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
  // Stop if we've gone too deep
  if constexpr (depth >= 20) {
    return S{};
  }

  else if constexpr (requires { evalConstantExpr(sym); }) {
    return evalConstantExpr(sym);
  }

  else if constexpr (requires { powerIdentities(sym); }) {
    constexpr auto result = powerIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { additionIdentities(sym); }) {
    constexpr auto result = additionIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { subtractionIdentities(sym); }) {
    constexpr auto result = subtractionIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { multiplicationIdentities(sym); }) {
    constexpr auto result = multiplicationIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { divisionIdentities(sym); }) {
    constexpr auto result = divisionIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { expIdentities(sym); }) {
    constexpr auto result = expIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { logIdentities(sym); }) {
    constexpr auto result = logIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { sinIdentities(sym); }) {
    constexpr auto result = sinIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else if constexpr (requires { cosIdentities(sym); }) {
    constexpr auto result = cosIdentities(sym);
    if constexpr (match(result, sym)) {
      return result;
    } else {
      return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
    }
  }

  else {
    return S{};
  }
}

template <typename Op, Symbolic... Args>
constexpr auto simplifyTerms(Expression<Op, Args...>) {
  return Expression{Op{}, simplify(Args{})...};
}

// Public API: Fully simplifies a symbolic expression.
// This function orchestrates the simplification process.
// It typically simplifies arguments first (bottom-up / inside-out),
// then applies rules to the expression with simplified arguments.
template <Symbolic S>
constexpr auto simplify(S sym) {
  if constexpr (requires { simplifyTerms(sym); }) {
    return simplifySymbol(simplifyTerms(sym));
  }

  else {
    return simplifySymbol(sym);
  }
}

}  // namespace tempura
