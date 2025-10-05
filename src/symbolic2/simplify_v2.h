#pragma once

#include "accessors.h"
#include "constants.h"
#include "core.h"
#include "evaluate.h"
#include "matching.h"
#include "operators.h"
#include "ordering.h"
#include "pattern_matching.h"

// Simplified version of simplify.h using pattern matching RewriteSystem
//
// This version replaces manual if-else chains with declarative rewrite rules,
// making the code more concise and easier to understand.

namespace tempura {

// =============================================================================
// CONSTANT FOLDING
// =============================================================================

// Constant folding: evaluate expressions with only constant arguments
template <typename Op, Symbolic... Args>
  requires((match(Args{}, AnyConstant{}) && ...) && sizeof...(Args) > 0)
constexpr auto evalConstantExpr(Expression<Op, Args...> expr) {
  return Constant<evaluate(expr, BinderPack{})>{};
}

// =============================================================================
// POWER IDENTITIES - x^n simplification rules
// =============================================================================

constexpr auto PowerRules = RewriteSystem{
  Rewrite{pow(x_, 0_c), 1_c},                    // x^0 → 1
  Rewrite{pow(x_, 1_c), x_},                     // x^1 → x
  Rewrite{pow(1_c, x_), 1_c},                    // 1^x → 1
  Rewrite{pow(0_c, x_), 0_c},                    // 0^x → 0
  Rewrite{pow(pow(x_, a_), b_), pow(x_, a_ * b_)} // (x^a)^b → x^(a*b)
};

template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  return PowerRules.apply(expr);
}

// =============================================================================
// ADDITION IDENTITIES - x + y simplification rules
// =============================================================================

// Helper for canonical ordering in addition
template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  // Basic identities
  if constexpr (match(expr, 0_c + AnyArg{})) {
    return right(expr);  // 0 + x → x
  } else if constexpr (match(expr, AnyArg{} + 0_c)) {
    return left(expr);   // x + 0 → x
  }
  // x + x → 2*x
  else if constexpr (match(left(expr), right(expr))) {
    return left(expr) * 2_c;
  }
  // Canonical ordering: smaller term first
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    return right(expr) + left(expr);
  }
  // Associativity for sorted terms: (a + c) + b → (a + b) + c when b < c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) + AnyArg{}) &&
                     symbolicLessThan(right(expr), right(left(expr)))) {
    constexpr auto a = left(left(expr));
    constexpr auto c = right(left(expr));
    constexpr auto b = right(expr);
    return (a + b) + c;
  }
  // Factoring: x*a + x → x*(a+1)
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{}) &&
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    return x * (a + 1_c);
  }
  // Factoring: x*a + x*b → x*(a+b)
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{} * AnyArg{}) &&
                     match(left(left(expr)), left(right(expr)))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    return x * (a + b);
  }
  // Right-associative normalization: (a + b) + c → a + (b + c)
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) + AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    return a + (b + c);
  }
  else {
    return expr;
  }
}

// =============================================================================
// MULTIPLICATION IDENTITIES - x * y simplification rules
// =============================================================================

template <Symbolic S>
  requires(match(S{}, AnyArg{} * AnyArg{}))
constexpr auto multiplicationIdentities(S expr) {
  // Basic identities
  if constexpr (match(expr, 0_c * AnyArg{})) {
    return 0_c;  // 0 * x → 0
  } else if constexpr (match(expr, AnyArg{} * 0_c)) {
    return 0_c;  // x * 0 → 0
  } else if constexpr (match(expr, 1_c * AnyArg{})) {
    return right(expr);  // 1 * x → x
  } else if constexpr (match(expr, AnyArg{} * 1_c)) {
    return left(expr);   // x * 1 → x
  }
  // Power combining: x * x^a → x^(a+1)
  else if constexpr (match(expr, AnyArg{} * pow(AnyArg{}, AnyArg{})) &&
                     match(left(expr), left(right(expr)))) {
    constexpr auto x = left(expr);
    constexpr auto a = right(right(expr));
    return pow(x, a + 1_c);
  }
  // Power combining: x^a * x → x^(a+1)
  else if constexpr (match(expr, pow(AnyArg{}, AnyArg{}) * AnyArg{}) &&
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    return pow(x, a + 1_c);
  }
  // Power combining: x^a * x^b → x^(a+b)
  else if constexpr (match(expr, pow(AnyArg{}, AnyArg{}) * pow(AnyArg{}, AnyArg{})) &&
                     match(left(left(expr)), left(right(expr)))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    return pow(x, a + b);
  }
  // Distribution: (a + b) * c → a*c + b*c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) * AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    return (a * c) + (b * c);
  }
  // Distribution: a * (b + c) → a*b + a*c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} + AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    return (a * b) + (a * c);
  }
  // Canonical ordering: smaller term first
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    return right(expr) * left(expr);
  }
  // Left-associative normalization: a * (b * c) → (a * b) * c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} * AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    return (a * b) * c;
  }
  // Associativity for sorted terms: (a * c) * b → (a * b) * c when b < c
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{}) &&
                     symbolicLessThan(right(expr), right(left(expr)))) {
    constexpr auto a = left(left(expr));
    constexpr auto c = right(left(expr));
    constexpr auto b = right(expr);
    return (a * b) * c;
  }
  // Right-associative fallback: (a * b) * c → a * (b * c)
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{})) {
    constexpr auto a = left(left(expr));
    constexpr auto b = right(left(expr));
    constexpr auto c = right(expr);
    return a * (b * c);
  }
  else {
    return expr;
  }
}

// =============================================================================
// SUBTRACTION & DIVISION - Rewrite to addition/multiplication
// =============================================================================

// Forward declaration
template <Symbolic S>
constexpr auto simplifySymbol(S sym);

// Subtraction rewritten as addition: a - b → a + (-1 * b)
template <Symbolic S>
  requires(match(S{}, AnyArg{} - AnyArg{}))
constexpr auto subtractionIdentities(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a + simplifySymbol(Constant<-1>{} * b));
}

// Division rewritten as power: a / b → a * b^(-1)
template <Symbolic S>
  requires(match(S{}, AnyArg{} / AnyArg{}))
constexpr auto divisionIdentities(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a * simplifySymbol(pow(b, Constant<-1>{})));
}

// =============================================================================
// TRANSCENDENTAL FUNCTION IDENTITIES
// =============================================================================

constexpr auto ExpRules = RewriteSystem{
  Rewrite{exp(log(x_)), x_}  // exp(log(x)) → x
};

template <Symbolic S>
  requires(match(S{}, exp(AnyArg{})))
constexpr auto expIdentities(S expr) {
  // Try rewrite rules first
  constexpr auto result = ExpRules.apply(expr);
  if constexpr (!match(result, expr)) {
    return result;
  }
  // Otherwise normalize to power form: exp(x) → e^x
  else {
    return pow(e, operand(expr));
  }
}

constexpr auto LogRules = RewriteSystem{
  Rewrite{log(1_c), 0_c},              // log(1) → 0
  Rewrite{log(e), 1_c},                // log(e) → 1
  Rewrite{log(pow(x_, a_)), a_ * log(x_)},    // log(x^a) → a*log(x)
  Rewrite{log(x_ * y_), log(x_) + log(y_)},   // log(a*b) → log(a)+log(b)
  Rewrite{log(x_ / y_), log(x_) - log(y_)},   // log(a/b) → log(a)-log(b)
  Rewrite{log(exp(x_)), x_}            // log(exp(x)) → x
};

template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  return LogRules.apply(expr);
}

constexpr auto SinRules = RewriteSystem{
  Rewrite{sin(π * 0.5_c), 1_c},        // sin(π/2) → 1
  Rewrite{sin(π), 0_c},                // sin(π) → 0
  Rewrite{sin(π * 1.5_c), Constant<-1>{}}  // sin(3π/2) → -1
};

template <Symbolic S>
  requires(match(S{}, sin(AnyArg{})))
constexpr auto sinIdentities(S expr) {
  constexpr auto result = SinRules.apply(expr);
  if constexpr (!match(result, expr)) {
    return result;
  }
  // Odd function: sin(-x) → -sin(x)
  else if constexpr (match(expr, sin(AnyArg{} * Constant<-1>{}))) {
    constexpr auto x = left(operand(expr));
    return Constant<-1>{} * sin(x);
  }
  else {
    return expr;
  }
}

constexpr auto CosRules = RewriteSystem{
  Rewrite{cos(π * 0.5_c), 0_c},        // cos(π/2) → 0
  Rewrite{cos(π), Constant<-1>{}},     // cos(π) → -1
  Rewrite{cos(π * 1.5_c), 0_c}         // cos(3π/2) → 0
};

template <Symbolic S>
  requires(match(S{}, cos(AnyArg{})))
constexpr auto cosIdentities(S expr) {
  return CosRules.apply(expr);
}

constexpr auto TanRules = RewriteSystem{
  Rewrite{tan(π), 0_c}  // tan(π) → 0
};

template <Symbolic S>
  requires(match(S{}, tan(AnyArg{})))
constexpr auto tanIdentities(S expr) {
  return TanRules.apply(expr);
}

// =============================================================================
// SIMPLIFICATION ENGINE - Depth-limited iterative simplification
// =============================================================================

template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym);

// Single-term simplification entry point
template <Symbolic S>
constexpr auto simplifySymbol(S sym) {
  return simplifySymbolWithDepth<S, 0>(sym);
}

// Depth-limited simplification prevents infinite recursion
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
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

// Recursively simplify all subexpressions
template <typename Op, Symbolic... Args>
constexpr auto simplifyTerms(Expression<Op, Args...>) {
  return Expression{Op{}, simplify(Args{})...};
}

// Bottom-up simplification: simplify arguments first, then apply rules
template <Symbolic S>
constexpr auto simplify(S sym) {
  if constexpr (requires { simplifyTerms(sym); }) {
    return simplifySymbol(simplifyTerms(sym));
  } else {
    return simplifySymbol(sym);
  }
}

}  // namespace tempura
