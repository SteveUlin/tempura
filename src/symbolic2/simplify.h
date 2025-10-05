#pragma once

#include "accessors.h"
#include "constants.h"
#include "core.h"
#include "evaluate.h"
#include "matching.h"
#include "operators.h"
#include "ordering.h"
#include "pattern_matching.h"

// Algebraic simplification using declarative pattern-based rewrite systems.
//
// Rules are organized by category (Identity, Ordering, Distribution, etc.)
// for maintainability. Each category can be tested and composed independently.
//
// Key design decisions:
// - Category ordering matters: Distribution must precede Associativity to avoid
//   rewriting distributed terms back into factored form
// - Subtraction/division normalized to addition/multiplication with negation
//   to reduce rule count and ensure consistent canonical forms
// - Predicate-based rules enable conditional rewrites (e.g., a+b → b+a iff b<a)
//   for establishing total orderings without infinite rewrite loops

namespace tempura {

// --- Simplification Rules ---

template <Symbolic S>
constexpr auto simplifySymbol(S sym);

// Fold expressions with only constant arguments into a single constant
template <typename Op, Symbolic... Args>
  requires((match(Args{}, 𝐜) && ...) && sizeof...(Args) > 0)
constexpr auto foldConstants(Expression<Op, Args...> expr) {
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

constexpr auto PowerRules =
    RewriteSystem{Rewrite{pow(x_, 0_c), 1_c}, Rewrite{pow(x_, 1_c), x_},
                  Rewrite{pow(1_c, x_), 1_c}, Rewrite{pow(0_c, x_), 0_c},
                  Rewrite{pow(pow(x_, a_), b_), pow(x_, a_* b_)}};

template <Symbolic S>
  requires(match(S{}, pow(𝐚𝐧𝐲, 𝐚𝐧𝐲)))
constexpr auto applyPowerRules(S expr) {
  return applyRules<PowerRules>(expr);
}

namespace AdditionRuleCategories {
constexpr auto Identity =
    RewriteSystem{Rewrite{0_c + x_, x_}, Rewrite{x_ + 0_c, x_}};

constexpr auto LikeTerms = RewriteSystem{Rewrite{x_ + x_, x_ * 2_c}};

constexpr auto Ordering =
    RewriteSystem{Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
                            return symbolicLessThan(get(ctx, y_), get(ctx, x_));
                          }}};

constexpr auto Factoring =
    RewriteSystem{Rewrite{x_ * a_ + x_, x_*(a_ + 1_c)},
                  Rewrite{x_ * a_ + x_ * b_, x_*(a_ + b_)}};

// Right-associative with conditional reordering for canonical form
constexpr auto Associativity = RewriteSystem{
    Rewrite{
        (a_ + c_) + b_, (a_ + b_) + c_,
        [](auto ctx) { return symbolicLessThan(get(ctx, b_), get(ctx, c_)); }},
    Rewrite{(a_ + b_) + c_, a_ + (b_ + c_)}};
}  // namespace AdditionRuleCategories

constexpr auto AdditionRules =
    compose(AdditionRuleCategories::Identity, AdditionRuleCategories::LikeTerms,
            AdditionRuleCategories::Ordering, AdditionRuleCategories::Factoring,
            AdditionRuleCategories::Associativity);

template <Symbolic S>
  requires(match(S{}, 𝐚𝐧𝐲 + 𝐚𝐧𝐲))
constexpr auto applyAdditionRules(S expr) {
  return applyRules<AdditionRules>(expr);
}

// Category ordering matters: Distribution before Associativity prevents
// un-factoring distributed terms
namespace MultiplicationRuleCategories {
constexpr auto Identity =
    RewriteSystem{Rewrite{0_c * x_, 0_c}, Rewrite{x_ * 0_c, 0_c},
                  Rewrite{1_c * x_, x_}, Rewrite{x_ * 1_c, x_}};

constexpr auto Distribution =
    RewriteSystem{Rewrite{(a_ + b_) * c_, (a_ * c_) + (b_ * c_)},
                  Rewrite{a_ * (b_ + c_), (a_ * b_) + (a_ * c_)}};

// Pattern matching ensures x_ binds consistently (x*x^a requires same base)
constexpr auto PowerCombining =
    RewriteSystem{Rewrite{x_ * pow(x_, a_), pow(x_, a_ + 1_c)},
                  Rewrite{pow(x_, a_) * x_, pow(x_, a_ + 1_c)},
                  Rewrite{pow(x_, a_) * pow(x_, b_), pow(x_, a_ + b_)}};

constexpr auto Ordering =
    RewriteSystem{Rewrite{x_ * y_, y_* x_, [](auto ctx) {
                            return symbolicLessThan(get(ctx, y_), get(ctx, x_));
                          }}};

constexpr auto Associativity = RewriteSystem{
    Rewrite{a_ * (b_ * c_), (a_ * b_) * c_},
    Rewrite{
        (a_ * c_) * b_, (a_ * b_) * c_,
        [](auto ctx) { return symbolicLessThan(get(ctx, b_), get(ctx, c_)); }},
    Rewrite{(a_ * b_) * c_, a_*(b_* c_)}};
}  // namespace MultiplicationRuleCategories

constexpr auto MultiplicationRules =
    compose(MultiplicationRuleCategories::Identity,
            MultiplicationRuleCategories::Distribution,
            MultiplicationRuleCategories::PowerCombining,
            MultiplicationRuleCategories::Ordering,
            MultiplicationRuleCategories::Associativity);

template <Symbolic S>
  requires(match(S{}, 𝐚𝐧𝐲* 𝐚𝐧𝐲))
constexpr auto applyMultiplicationRules(S expr) {
  return applyRules<MultiplicationRules>(expr);
}

// Normalize subtraction to addition: a - b → a + (-1·b)
template <Symbolic S>
  requires(match(S{}, 𝐚𝐧𝐲 - 𝐚𝐧𝐲))
constexpr auto normalizeSubtraction(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a + simplifySymbol(Constant<-1>{} * b));
}

// Normalize division to multiplication: a / b → a·b⁻¹
template <Symbolic S>
  requires(match(S{}, 𝐚𝐧𝐲 / 𝐚𝐧𝐲))
constexpr auto normalizeDivision(S expr) {
  constexpr auto a = left(expr);
  constexpr auto b = right(expr);
  return simplifySymbol(a * simplifySymbol(pow(b, Constant<-1>{})));
}

// Normalize exp to power form for consistent representation
constexpr auto ExpRules =
    RewriteSystem{Rewrite{exp(log(x_)), x_}, Rewrite{exp(x_), pow(e, x_)}};

template <Symbolic S>
  requires(match(S{}, exp(𝐚𝐧𝐲)))
constexpr auto applyExpRules(S expr) {
  return applyRules<ExpRules>(expr);
}

namespace LogRuleCategories {
constexpr auto Identity =
    RewriteSystem{Rewrite{log(1_c), 0_c}, Rewrite{log(e), 1_c}};

constexpr auto Inverse = RewriteSystem{Rewrite{log(exp(x_)), x_}};

constexpr auto Expansion =
    RewriteSystem{Rewrite{log(pow(x_, a_)), a_* log(x_)},
                  Rewrite{log(x_ * y_), log(x_) + log(y_)},
                  Rewrite{log(x_ / y_), log(x_) - log(y_)}};
}  // namespace LogRuleCategories

constexpr auto LogRules =
    compose(LogRuleCategories::Identity, LogRuleCategories::Inverse,
            LogRuleCategories::Expansion);

template <Symbolic S>
  requires(match(S{}, log(𝐚𝐧𝐲)))
constexpr auto applyLogRules(S expr) {
  return applyRules<LogRules>(expr);
}

// Trigonometric simplification rules
namespace SinRuleCategories {
constexpr auto Identity = RewriteSystem{Rewrite{sin(0_c), 0_c}};

constexpr auto SpecialAngles =
    RewriteSystem{Rewrite{sin(π * 0.5_c), 1_c}, Rewrite{sin(π), 0_c},
                  Rewrite{sin(π * 1.5_c), Constant<-1>{}}};

constexpr auto Symmetry =
    RewriteSystem{Rewrite{sin(-x_), -sin(x_)}};  // Odd function
}  // namespace SinRuleCategories

constexpr auto SinRules =
    compose(SinRuleCategories::Identity, SinRuleCategories::SpecialAngles,
            SinRuleCategories::Symmetry);

template <Symbolic S>
  requires(match(S{}, sin(𝐚𝐧𝐲)))
constexpr auto applySinRules(S expr) {
  return applyRules<SinRules>(expr);
}

namespace CosRuleCategories {
constexpr auto Identity = RewriteSystem{Rewrite{cos(0_c), 1_c}};

constexpr auto SpecialAngles =
    RewriteSystem{Rewrite{cos(π * 0.5_c), 0_c}, Rewrite{cos(π), Constant<-1>{}},
                  Rewrite{cos(π * 1.5_c), 0_c}};

constexpr auto Symmetry =
    RewriteSystem{Rewrite{cos(-x_), cos(x_)}};  // Even function
}  // namespace CosRuleCategories

constexpr auto CosRules =
    compose(CosRuleCategories::Identity, CosRuleCategories::SpecialAngles,
            CosRuleCategories::Symmetry);

template <Symbolic S>
  requires(match(S{}, cos(𝐚𝐧𝐲)))
constexpr auto applyCosRules(S expr) {
  return applyRules<CosRules>(expr);
}

namespace TanRuleCategories {
constexpr auto Identity = RewriteSystem{Rewrite{tan(0_c), 0_c}};

constexpr auto SpecialAngles = RewriteSystem{Rewrite{tan(π), 0_c}};

constexpr auto Symmetry =
    RewriteSystem{Rewrite{tan(-x_), -tan(x_)}};  // Odd function
}  // namespace TanRuleCategories

constexpr auto TanRules =
    compose(TanRuleCategories::Identity, TanRuleCategories::SpecialAngles,
            TanRuleCategories::Symmetry);

template <Symbolic S>
  requires(match(S{}, tan(𝐚𝐧𝐲)))
constexpr auto applyTanRules(S expr) {
  return applyRules<TanRules>(expr);
}

// Hyperbolic function simplification rules
namespace SinhRuleCategories {
constexpr auto Identity = RewriteSystem{Rewrite{sinh(0_c), 0_c}};

constexpr auto Symmetry =
    RewriteSystem{Rewrite{sinh(-x_), -sinh(x_)}};  // Odd function

constexpr auto Inverse =
    RewriteSystem{Rewrite{sinh(log(x_)), (x_ - pow(x_, Constant<-1>{})) / 2_c}};
}  // namespace SinhRuleCategories

constexpr auto SinhRules =
    compose(SinhRuleCategories::Identity, SinhRuleCategories::Symmetry,
            SinhRuleCategories::Inverse);

template <Symbolic S>
  requires(match(S{}, sinh(𝐚𝐧𝐲)))
constexpr auto applySinhRules(S expr) {
  return applyRules<SinhRules>(expr);
}

namespace CoshRuleCategories {
constexpr auto Identity = RewriteSystem{Rewrite{cosh(0_c), 1_c}};

constexpr auto Symmetry =
    RewriteSystem{Rewrite{cosh(-x_), cosh(x_)}};  // Even function

constexpr auto Inverse =
    RewriteSystem{Rewrite{cosh(log(x_)), (x_ + pow(x_, Constant<-1>{})) / 2_c}};
}  // namespace CoshRuleCategories

constexpr auto CoshRules =
    compose(CoshRuleCategories::Identity, CoshRuleCategories::Symmetry,
            CoshRuleCategories::Inverse);

template <Symbolic S>
  requires(match(S{}, cosh(𝐚𝐧𝐲)))
constexpr auto applyCoshRules(S expr) {
  return applyRules<CoshRules>(expr);
}

namespace TanhRuleCategories {
constexpr auto Identity = RewriteSystem{Rewrite{tanh(0_c), 0_c}};

constexpr auto Symmetry =
    RewriteSystem{Rewrite{tanh(-x_), -tanh(x_)}};  // Odd function
}  // namespace TanhRuleCategories

constexpr auto TanhRules =
    compose(TanhRuleCategories::Identity, TanhRuleCategories::Symmetry);

template <Symbolic S>
  requires(match(S{}, tanh(𝐚𝐧𝐲)))
constexpr auto applyTanhRules(S expr) {
  return applyRules<TanhRules>(expr);
}

template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym);

// Single-term simplification entry point
template <Symbolic S>
constexpr auto simplifySymbol(S sym) {
  return simplifySymbolWithDepth<S, 0>(sym);
}

// Apply rule and recurse if expression changed
template <Symbolic S, SizeT depth, auto SimplifyFunc>
constexpr auto trySimplify(S sym) {
  constexpr auto result = SimplifyFunc(sym);
  if constexpr (match(result, sym)) {
    return result;
  } else {
    return simplifySymbolWithDepth<decltype(result), depth + 1>(result);
  }
}

// Depth-limited simplification (prevents infinite recursion in pathological
// cases)
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
  if constexpr (depth >= 20) {
    return S{};
  } else if constexpr (requires { foldConstants(sym); }) {
    return foldConstants(sym);
  } else if constexpr (requires { applyPowerRules(sym); }) {
    return trySimplify<S, depth, applyPowerRules<S>>(sym);
  } else if constexpr (requires { applyAdditionRules(sym); }) {
    return trySimplify<S, depth, applyAdditionRules<S>>(sym);
  } else if constexpr (requires { normalizeSubtraction(sym); }) {
    return trySimplify<S, depth, normalizeSubtraction<S>>(sym);
  } else if constexpr (requires { applyMultiplicationRules(sym); }) {
    return trySimplify<S, depth, applyMultiplicationRules<S>>(sym);
  } else if constexpr (requires { normalizeDivision(sym); }) {
    return trySimplify<S, depth, normalizeDivision<S>>(sym);
  } else if constexpr (requires { applyExpRules(sym); }) {
    return trySimplify<S, depth, applyExpRules<S>>(sym);
  } else if constexpr (requires { applyLogRules(sym); }) {
    return trySimplify<S, depth, applyLogRules<S>>(sym);
  } else if constexpr (requires { applySinRules(sym); }) {
    return trySimplify<S, depth, applySinRules<S>>(sym);
  } else if constexpr (requires { applyCosRules(sym); }) {
    return trySimplify<S, depth, applyCosRules<S>>(sym);
  } else if constexpr (requires { applyTanRules(sym); }) {
    return trySimplify<S, depth, applyTanRules<S>>(sym);
  } else if constexpr (requires { applySinhRules(sym); }) {
    return trySimplify<S, depth, applySinhRules<S>>(sym);
  } else if constexpr (requires { applyCoshRules(sym); }) {
    return trySimplify<S, depth, applyCoshRules<S>>(sym);
  } else if constexpr (requires { applyTanhRules(sym); }) {
    return trySimplify<S, depth, applyTanhRules<S>>(sym);
  } else {
    return S{};
  }
}

template <typename Op, Symbolic... Args>
constexpr auto simplifyTerms(Expression<Op, Args...>) {
  return Expression{Op{}, simplify(Args{})...};
}

// Bottom-up: simplify children then apply rules to parent
template <Symbolic S>
constexpr auto simplify(S sym) {
  if constexpr (requires { simplifyTerms(sym); }) {
    return simplifySymbol(simplifyTerms(sym));
  } else {
    return simplifySymbol(sym);
  }
}

}  // namespace tempura
