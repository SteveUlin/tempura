#pragma once

#include "accessors.h"
#include "constants.h"
#include "core.h"
#include "evaluate.h"
#include "matching.h"
#include "operators.h"
#include "ordering.h"
#include "pattern_matching.h"

// Simplification rules for symbolic expressions using pattern matching
// RewriteSystem
//
// 🎉 FULLY MIGRATED to RewriteSystem with category-based organization!
//
// All simplification rules now use the declarative RewriteSystem, organized by
// category for better maintainability and testability:
//
//   ✓ PowerRules (5 rules) - x^0→1, x^1→x, 1^x→1, 0^x→0, (x^a)^b→x^(a*b)
//
//   ✓ AdditionRules (9 rules, 5 categories):
//     - Identity: 0 + x, x + 0
//     - LikeTerms: x + x → 2x
//     - Ordering: canonical ordering with predicates
//     - Factoring: x*a + x*b → x*(a+b)
//     - Associativity: structural normalization
//
//   ✓ MultiplicationRules (13 rules, 5 categories):
//     - Identity: 0 * x, x * 0, 1 * x, x * 1
//     - Distribution: (a+b)*c → a*c + b*c
//     - PowerCombining: x*x^a → x^(a+1), x^a*x^b → x^(a+b)
//     - Ordering: canonical ordering with predicates
//     - Associativity: structural normalization
//
//   ✓ ExpRules (2 rules) - exp(log(x))→x, exp(x)→e^x
//   ✓ LogRules (6 rules) - log(1)→0, log(e)→1, log(x^a)→a*log(x), etc.
//   ✓ SinRules (4 rules) - sin(π/2)→1, sin(π)→0, sin(3π/2)→-1, sin(-x)→-sin(x)
//   ✓ CosRules (3 rules) - cos(π/2)→0, cos(π)→-1, cos(3π/2)→0
//   ✓ TanRules (1 rule) - tan(π)→0
//   ✓ SubtractionRules - rewritten to addition (a-b → a+(-1*b))
//   ✓ DivisionRules - rewritten to power (a/b → a*b^(-1))
//
// Predicate-based rules enable conditional transformations:
//   - Canonical ordering: x+y → y+x if y < x
//   - Associative reordering: (a+c)+b → (a+b)+c if b < c
//   - Like term combining: x*a + x*b → x*(a+b) when bases match
//   - Power laws: x*x^a → x^(a+1), x^a*x^b → x^(a+b) when bases match
//
// NEW: Category-based organization with compose() helper:
//   - Rules organized by semantic purpose (Identity, Ordering, Factoring, etc.)
//   - Each category can be tested independently
//   - Easy to create custom rule combinations
//   - Self-documenting code structure
//
// See PREDICATE_REWRITING.md for documentation and examples.
//
// Benefits: Fully declarative, self-documenting rules, easy to extend,
//           zero runtime overhead, type-safe compile-time evaluation

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

// Helper: Apply a RewriteSystem to an expression
template <auto& Rules, Symbolic S>
constexpr auto applyRules(S expr) {
  return Rules.apply(expr);
}

// Helper: Compose multiple RewriteSystems into one
namespace detail {
// Base case: single system
template <typename... Rules>
constexpr auto composeRulesImpl(RewriteSystem<Rules...>) {
  return RewriteSystem<Rules...>{Rules{}...};
}

// Recursive case: combine two systems and continue
template <typename... Rules1, typename... Rules2, typename... Rest>
constexpr auto composeRulesImpl(RewriteSystem<Rules1...>,
                                RewriteSystem<Rules2...>, Rest... rest) {
  // Create combined system with all rules
  auto combined = RewriteSystem<Rules1..., Rules2...>{Rules1{}..., Rules2{}...};
  if constexpr (sizeof...(Rest) > 0) {
    return composeRulesImpl(combined, rest...);
  } else {
    return combined;
  }
}
}  // namespace detail

template <typename... Systems>
constexpr auto compose(Systems... systems) {
  return detail::composeRulesImpl(systems...);
}

// Power simplification rules using RewriteSystem
constexpr auto PowerRules = RewriteSystem{
    Rewrite{pow(x_, 0_c), 1_c},                     // x^0 → 1
    Rewrite{pow(x_, 1_c), x_},                      // x^1 → x
    Rewrite{pow(1_c, x_), 1_c},                     // 1^x → 1
    Rewrite{pow(0_c, x_), 0_c},                     // 0^x → 0
    Rewrite{pow(pow(x_, a_), b_), pow(x_, a_* b_)}  // (x^a)^b → x^(a*b)
};

template <Symbolic S>
  requires(match(S{}, pow(AnyArg{}, AnyArg{})))
constexpr auto powerIdentities(S expr) {
  return applyRules<PowerRules>(expr);
}

// Addition rules organized by category for better maintainability
namespace AdditionRuleCategories {
// Identity: operations with zero
constexpr auto Identity = RewriteSystem{
    Rewrite{0_c + x_, x_},  // 0 + x → x
    Rewrite{x_ + 0_c, x_}   // x + 0 → x
};

// Like terms: combining identical expressions
constexpr auto LikeTerms = RewriteSystem{
    Rewrite{x_ + x_, x_ * 2_c}  // x + x → 2x
};

// Ordering: canonical form for consistent representation
constexpr auto Ordering =
    RewriteSystem{// x + y → y + x if y < x (ensures canonical ordering)
                  Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
                            return symbolicLessThan(get(ctx, y_), get(ctx, x_));
                          }}};

// Factoring: extract common factors
constexpr auto Factoring = RewriteSystem{
    Rewrite{x_ * a_ + x_, x_*(a_ + 1_c)},     // x*a + x → x*(a+1)
    Rewrite{x_ * a_ + x_ * b_, x_*(a_ + b_)}  // x*a + x*b → x*(a+b)
};

// Associativity: restructure nested additions
constexpr auto Associativity = RewriteSystem{
    // (a + c) + b → (a + b) + c if b < c (reorder for canonical form)
    Rewrite{
        (a_ + c_) + b_, (a_ + b_) + c_,
        [](auto ctx) { return symbolicLessThan(get(ctx, b_), get(ctx, c_)); }},

    // (a + b) + c → a + (b + c) (right-associative normalization)
    Rewrite{(a_ + b_) + c_, a_ + (b_ + c_)}};
}  // namespace AdditionRuleCategories

// Complete addition rule set composed from categories
constexpr auto AdditionRules =
    compose(AdditionRuleCategories::Identity, AdditionRuleCategories::LikeTerms,
            AdditionRuleCategories::Ordering, AdditionRuleCategories::Factoring,
            AdditionRuleCategories::Associativity);

template <Symbolic S>
  requires(match(S{}, AnyArg{} + AnyArg{}))
constexpr auto additionIdentities(S expr) {
  return applyRules<AdditionRules>(expr);
}

// Multiplication rules organized by category
// NOTE: Category ordering matters! Distribution must come before associativity
// rewrites.
namespace MultiplicationRuleCategories {
// Identity and annihilator operations
constexpr auto Identity = RewriteSystem{
    Rewrite{0_c * x_, 0_c},  // 0 * x → 0 (annihilator)
    Rewrite{x_ * 0_c, 0_c},  // x * 0 → 0 (annihilator)
    Rewrite{1_c * x_, x_},   // 1 * x → x (multiplicative identity)
    Rewrite{x_ * 1_c, x_}    // x * 1 → x (multiplicative identity)
};

// Distribution: expand products over sums
constexpr auto Distribution = RewriteSystem{
    Rewrite{(a_ + b_) * c_, (a_ * c_) + (b_ * c_)},  // (a+b)*c → a*c + b*c
    Rewrite{a_ * (b_ + c_), (a_ * b_) + (a_ * c_)}   // a*(b+c) → a*b + a*c
};

// Power combining: x * x^a → x^(a+1)
// Note: Pattern matching ensures both x_ bind to the same value
constexpr auto PowerCombining = RewriteSystem{
    Rewrite{x_ * pow(x_, a_), pow(x_, a_ + 1_c)},         // x * x^a → x^(a+1)
    Rewrite{pow(x_, a_) * x_, pow(x_, a_ + 1_c)},         // x^a * x → x^(a+1)
    Rewrite{pow(x_, a_) * pow(x_, b_), pow(x_, a_ + b_)}  // x^a * x^b → x^(a+b)
};

// Ordering: canonical form for consistent representation
constexpr auto Ordering =
    RewriteSystem{// x * y → y * x if y < x (ensures canonical ordering)
                  Rewrite{x_ * y_, y_* x_, [](auto ctx) {
                            return symbolicLessThan(get(ctx, y_), get(ctx, x_));
                          }}};

// Associativity: restructure nested multiplications
constexpr auto Associativity = RewriteSystem{
    // a * (b * c) → (a * b) * c (left-associative normalization)
    Rewrite{a_ * (b_ * c_), (a_ * b_) * c_},

    // (a * c) * b → (a * b) * c if b < c (reorder for canonical form)
    Rewrite{
        (a_ * c_) * b_, (a_ * b_) * c_,
        [](auto ctx) { return symbolicLessThan(get(ctx, b_), get(ctx, c_)); }},

    // (a * b) * c → a * (b * c) (right-associative fallback)
    Rewrite{(a_ * b_) * c_, a_*(b_* c_)}};
}  // namespace MultiplicationRuleCategories

// Complete multiplication rule set composed from categories
// Order matters: Identity → Distribution → PowerCombining → Ordering →
// Associativity
constexpr auto MultiplicationRules =
    compose(MultiplicationRuleCategories::Identity,
            MultiplicationRuleCategories::Distribution,
            MultiplicationRuleCategories::PowerCombining,
            MultiplicationRuleCategories::Ordering,
            MultiplicationRuleCategories::Associativity);

template <Symbolic S>
  requires(match(S{}, AnyArg{} * AnyArg{}))
constexpr auto multiplicationIdentities(S expr) {
  return applyRules<MultiplicationRules>(expr);
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
    Rewrite{exp(log(x_)), x_},    // exp(log(x)) → x
    Rewrite{exp(x_), pow(e, x_)}  // exp(x) → e^x (normalize to power form)
};

template <Symbolic S>
  requires(match(S{}, exp(AnyArg{})))
constexpr auto expIdentities(S expr) {
  return applyRules<ExpRules>(expr);
}

// Logarithm rule categories
namespace LogRuleCategories {
// Identity: logarithm of special constants
constexpr auto Identity = RewriteSystem{
    Rewrite{log(1_c), 0_c},  // log(1) → 0
    Rewrite{log(e), 1_c}     // log(e) → 1
};

// Inverse: logarithm cancels with exponential
constexpr auto Inverse = RewriteSystem{
    Rewrite{log(exp(x_)), x_}  // log(exp(x)) → x
};

// Expansion: logarithm of products, quotients, and powers
constexpr auto Expansion = RewriteSystem{
    Rewrite{log(pow(x_, a_)), a_* log(x_)},    // log(x^a) → a*log(x)
    Rewrite{log(x_ * y_), log(x_) + log(y_)},  // log(a*b) → log(a)+log(b)
    Rewrite{log(x_ / y_), log(x_) - log(y_)}   // log(a/b) → log(a)-log(b)
};
}  // namespace LogRuleCategories

// Complete logarithm rule set composed from categories
// Order: Identity → Inverse → Expansion
constexpr auto LogRules =
    compose(LogRuleCategories::Identity, LogRuleCategories::Inverse,
            LogRuleCategories::Expansion);

template <Symbolic S>
  requires(match(S{}, log(AnyArg{})))
constexpr auto logIdentities(S expr) {
  return applyRules<LogRules>(expr);
}

// Sine simplification rules
constexpr auto SinRules = RewriteSystem{
    Rewrite{sin(π * 0.5_c), 1_c},             // sin(π/2) → 1
    Rewrite{sin(π), 0_c},                     // sin(π) → 0
    Rewrite{sin(π * 1.5_c), Constant<-1>{}},  // sin(3π/2) → -1
    Rewrite{sin(x_ * Constant<-1>{}),
            Constant<-1>{} * sin(x_)}  // sin(-x) → -sin(x) (odd function)
};

template <Symbolic S>
  requires(match(S{}, sin(AnyArg{})))
constexpr auto sinIdentities(S expr) {
  return applyRules<SinRules>(expr);
}

// Cosine simplification rules
constexpr auto CosRules = RewriteSystem{
    Rewrite{cos(π * 0.5_c), 0_c},     // cos(π/2) → 0
    Rewrite{cos(π), Constant<-1>{}},  // cos(π) → -1
    Rewrite{cos(π * 1.5_c), 0_c}      // cos(3π/2) → 0
};

template <Symbolic S>
  requires(match(S{}, cos(AnyArg{})))
constexpr auto cosIdentities(S expr) {
  return applyRules<CosRules>(expr);
}

// Tangent simplification rules
constexpr auto TanRules = RewriteSystem{
    Rewrite{tan(π), 0_c}  // tan(π) → 0
};

template <Symbolic S>
  requires(match(S{}, tan(AnyArg{})))
constexpr auto tanIdentities(S expr) {
  return applyRules<TanRules>(expr);
}

template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym);

// Single-term simplification entry point
template <Symbolic S>
constexpr auto simplifySymbol(S sym) {
  return simplifySymbolWithDepth<S, 0>(sym);
}

// Helper: Try applying a simplification rule and recurse if it changed
template <Symbolic S, SizeT depth, auto SimplifyFunc>
constexpr auto trySimplify(S sym) {
  constexpr auto result = SimplifyFunc(sym);
  if constexpr (match(result, sym)) {
    return result;  // No change, return as-is
  } else {
    return simplifySymbolWithDepth<decltype(result), depth + 1>(
        result);  // Recurse
  }
}

// Depth-limited simplification prevents infinite recursion
// Uses a sequence of simplification strategies
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
  if constexpr (depth >= 20) {
    return S{};  // Max depth reached
  } else if constexpr (requires { evalConstantExpr(sym); }) {
    return evalConstantExpr(sym);  // Constant folding
  } else if constexpr (requires { powerIdentities(sym); }) {
    return trySimplify<S, depth, powerIdentities<S>>(sym);
  } else if constexpr (requires { additionIdentities(sym); }) {
    return trySimplify<S, depth, additionIdentities<S>>(sym);
  } else if constexpr (requires { subtractionIdentities(sym); }) {
    return trySimplify<S, depth, subtractionIdentities<S>>(sym);
  } else if constexpr (requires { multiplicationIdentities(sym); }) {
    return trySimplify<S, depth, multiplicationIdentities<S>>(sym);
  } else if constexpr (requires { divisionIdentities(sym); }) {
    return trySimplify<S, depth, divisionIdentities<S>>(sym);
  } else if constexpr (requires { expIdentities(sym); }) {
    return trySimplify<S, depth, expIdentities<S>>(sym);
  } else if constexpr (requires { logIdentities(sym); }) {
    return trySimplify<S, depth, logIdentities<S>>(sym);
  } else if constexpr (requires { sinIdentities(sym); }) {
    return trySimplify<S, depth, sinIdentities<S>>(sym);
  } else if constexpr (requires { cosIdentities(sym); }) {
    return trySimplify<S, depth, cosIdentities<S>>(sym);
  } else {
    return S{};  // No applicable rules
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
