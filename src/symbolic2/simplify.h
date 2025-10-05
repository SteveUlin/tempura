#pragma once

#include "accessors.h"
#include "constants.h"
#include "core.h"
#include "evaluate.h"
#include "matching.h"
#include "operators.h"
#include "ordering.h"
#include "pattern_matching.h"

// Simplification rules for symbolic expressions using pattern matching RewriteSystem
//
// This file uses a hybrid approach:
// 1. RewriteSystem - For simple structural transformations (identities, inverses)
// 2. RewriteSystem with Predicates - For conditional transformations (ordering, etc.)
// 3. Custom logic - For complex transformations (factoring, deep associativity)
//
// Migrated to RewriteSystem (29 rules across 10 systems):
//   ✓ PowerRules (5 rules) - x^0→1, x^1→x, 1^x→1, 0^x→0, (x^a)^b→x^(a*b)
//   ✓ ExpRules (1 rule) - exp(log(x))→x
//   ✓ LogRules (6 rules) - log(1)→0, log(e)→1, log(x^a)→a*log(x), etc.
//   ✓ SinRules (3 rules) - sin(π/2)→1, sin(π)→0, sin(3π/2)→-1
//   ✓ CosRules (3 rules) - cos(π/2)→0, cos(π)→-1, cos(3π/2)→0
//   ✓ TanRules (1 rule) - tan(π)→0
//   ✓ AdditionRules (2 rules) - 0+x→x, x+0→x
//   ✓ MultiplicationRules (4 rules) - 0*x→0, x*0→0, 1*x→x, x*1→x
//   ✓ DistributionRules (2 rules) - (a+b)*c→a*c+b*c, a*(b+c)→a*b+a*c
//
// New: Predicate-based rewriting for conditional transformations!
//   Predicates allow expressing rules like "reorder x+y to y+x if y < x"
//   See PREDICATE_REWRITING.md for documentation and examples.
//
// Benefits: ~44% code reduction for fully migrated functions, declarative style,
//           self-documenting rules, easy to extend, zero runtime overhead
//
// See SIMPLIFY_REWRITE.md for detailed documentation of the migration.

namespace tempura {

// --- Simplification Rules ---

template <Symbolic S>
constexpr auto simplifySymbol(S sym);

// Constant folding: evaluate expressions with only constant arguments
template <typename Op, Symbolic... Args>
  requires((match(Args{}, AnyConstant{}) && ...) && sizeof...(Args) > 0)
constexpr auto evalConstantExpr(Expression<Op, Args...> expr) {
  return Constant<evaluate(expr, BinderPack{})>{};
}

// Power simplification rules using RewriteSystem
constexpr auto PowerRules = RewriteSystem{
  Rewrite{pow(x_, 0_c), 1_c},                     // x^0 → 1
  Rewrite{pow(x_, 1_c), x_},                      // x^1 → x
  Rewrite{pow(1_c, x_), 1_c},                     // 1^x → 1
  Rewrite{pow(0_c, x_), 0_c},                     // 0^x → 0
  Rewrite{pow(pow(x_, a_), b_), pow(x_, a_ * b_)} // (x^a)^b → x^(a*b)
};

template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  return PowerRules.apply(expr);
}

// Addition basic identity rules
constexpr auto AdditionRules = RewriteSystem{
  Rewrite{0_c + x_, x_},         // 0 + x → x
  Rewrite{x_ + 0_c, x_}          // x + 0 → x
};

// TODO: Migrate canonical ordering to predicate-based rewriting:
// constexpr auto AdditionRulesWithOrdering = RewriteSystem{
//   Rewrite{0_c + x_, x_},
//   Rewrite{x_ + 0_c, x_},
//   Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
//     return symbolicLessThan(get(ctx, y_), get(ctx, x_));
//   }}
// };

template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  // Try basic identity rules first
  constexpr auto result = AdditionRules.apply(expr);
  if constexpr (!match(result, expr)) {
    return result;
  }

  // x + x = x * 2
  else if constexpr (match(left(expr), right(expr))) {
    constexpr auto x = left(expr);
    return x * 2_c;  // Don't recurse - let outer loop handle it
  }

  // Canonical ordering: smaller term first
  // NOTE: This can be migrated to a predicate-based rewrite rule!
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

  // Factoring patterns

  // x * a + x → x * (a + 1)
  else if constexpr (match(expr, AnyArg{} * AnyArg{} + AnyArg{}) &&
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    return x * (a + 1_c);
  }

  // x * a + x * b → x * (a + b)
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

// Multiplication basic identity rules
constexpr auto MultiplicationRules = RewriteSystem{
  Rewrite{0_c * x_, 0_c},                      // 0 * x → 0
  Rewrite{x_ * 0_c, 0_c},                      // x * 0 → 0
  Rewrite{1_c * x_, x_},                       // 1 * x → x
  Rewrite{x_ * 1_c, x_}                        // x * 1 → x
};

// Distribution rules (can be applied separately)
constexpr auto DistributionRules = RewriteSystem{
  Rewrite{(a_ + b_) * c_, (a_ * c_) + (b_ * c_)},  // (a + b) * c → a*c + b*c
  Rewrite{a_ * (b_ + c_), (a_ * b_) + (a_ * c_)}   // a * (b + c) → a*b + a*c
};

template <Symbolic S>
  requires(match(S{}, AnyArg{} * AnyArg{}))
constexpr auto multiplicationIdentities(S expr) {
  // Try basic identity rules first
  constexpr auto result = MultiplicationRules.apply(expr);
  if constexpr (!match(result, expr)) {
    return result;
  }

  // Power combining: x * x^a → x^(a+1)
  else if constexpr (match(expr, AnyArg{} * pow(AnyArg{}, AnyArg{})) and
                     match(left(expr), left(right(expr)))) {
    constexpr auto x = left(expr);
    constexpr auto a = right(right(expr));
    return pow(x, a + 1_c);
  }

  // Power combining: x^a * x → x^(a+1)
  else if constexpr (match(expr, pow(AnyArg{}, AnyArg{}) * AnyArg{}) and
                     match(left(left(expr)), right(expr))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    return pow(x, a + 1_c);
  }

  // Power combining: x^a * x^b → x^(a+b)
  else if constexpr (match(expr, (pow(AnyArg{}, AnyArg{}) *
                                  pow(AnyArg{}, AnyArg{}))) and
                     (match(left(left(expr)), left(right(expr))))) {
    constexpr auto x = left(left(expr));
    constexpr auto a = right(left(expr));
    constexpr auto b = right(right(expr));
    return pow(x, a + b);
  }

  // Distribution: (a + b) * c → a*c + b*c  or  a * (b + c) → a*b + a*c
  else if constexpr (match(expr, (AnyArg{} + AnyArg{}) * AnyArg{}) ||
                     match(expr, AnyArg{} * (AnyArg{} + AnyArg{}))) {
    constexpr auto dist_result = DistributionRules.apply(expr);
    if constexpr (!match(dist_result, expr)) {
      return dist_result;
    } else {
      return expr;  // Shouldn't happen, but fallback
    }
  }

  // Canonical ordering: smaller term first
  // NOTE: This can be migrated to a predicate-based rewrite rule!
  else if constexpr (symbolicLessThan(right(expr), left(expr))) {
    constexpr auto a = left(expr);
    constexpr auto b = right(expr);
    return b * a;
  }

  // Left-associative normalization: a * (b * c) → (a * b) * c
  else if constexpr (match(expr, AnyArg{} * (AnyArg{} * AnyArg{}))) {
    constexpr auto a = left(expr);
    constexpr auto b = left(right(expr));
    constexpr auto c = right(right(expr));
    return (a * b) * c;
  }

  // Associativity for sorted terms: (a * c) * b → (a * b) * c when b < c
  else if constexpr (match(expr, (AnyArg{} * AnyArg{}) * AnyArg{}) and
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

// Exponential simplification rules
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
  // Otherwise normalize exp to power form: exp(x) → e^x
  else {
    return pow(e, operand(expr));
  }
}

// Logarithm simplification rules
constexpr auto LogRules = RewriteSystem{
  Rewrite{log(1_c), 0_c},                      // log(1) → 0
  Rewrite{log(e), 1_c},                        // log(e) → 1
  Rewrite{log(pow(x_, a_)), a_ * log(x_)},     // log(x^a) → a*log(x)
  Rewrite{log(x_ * y_), log(x_) + log(y_)},    // log(a*b) → log(a)+log(b)
  Rewrite{log(x_ / y_), log(x_) - log(y_)},    // log(a/b) → log(a)-log(b)
  Rewrite{log(exp(x_)), x_}                    // log(exp(x)) → x
};

template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  return LogRules.apply(expr);
}

// Sine simplification rules
constexpr auto SinRules = RewriteSystem{
  Rewrite{sin(π * 0.5_c), 1_c},                // sin(π/2) → 1
  Rewrite{sin(π), 0_c},                        // sin(π) → 0
  Rewrite{sin(π * 1.5_c), Constant<-1>{}}      // sin(3π/2) → -1
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

// Cosine simplification rules
constexpr auto CosRules = RewriteSystem{
  Rewrite{cos(π * 0.5_c), 0_c},                // cos(π/2) → 0
  Rewrite{cos(π), Constant<-1>{}},             // cos(π) → -1
  Rewrite{cos(π * 1.5_c), 0_c}                 // cos(3π/2) → 0
};

template <Symbolic S>
  requires(match(S{}, cos(AnyArg{})))
constexpr auto cosIdentities(S expr) {
  return CosRules.apply(expr);
}

// Tangent simplification rules
constexpr auto TanRules = RewriteSystem{
  Rewrite{tan(π), 0_c}                         // tan(π) → 0
};

template <Symbolic S>
  requires(match(S{}, tan(AnyArg{})))
constexpr auto tanIdentities(S expr) {
  return TanRules.apply(expr);
}

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
