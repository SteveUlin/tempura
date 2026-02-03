#pragma once

#include "symbolic4/core.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/strategy/combinator.h"
#include "symbolic4/strategy/pattern.h"
#include "symbolic4/strategy/rule.h"

// ============================================================================
// RECURSIVE REWRITE RULES
// ============================================================================
//
// Enables defining recursive rules where the replacement can contain
// recursive calls back to the rule set.
//
// USAGE
// -----
//   auto diff = recursive(
//       rule(x_ + y_, rec(x_) + rec(y_))
//     | rule(x_ * y_, rec(x_) * y_ + x_ * rec(y_))
//     | rule(sin(x_), cos(x_) * rec(x_))
//     | rule(x_, 1_c)    // base case: variable
//   );
//
//   auto result = diff.apply(sin(x * x));
//   // -> cos(x * x) * (1 * x + x * 1)
//
// HOW IT WORKS
// ------------
// 1. `rec(x_)` creates a Rec<PatternVar> marker in the replacement
// 2. During substitution, when we see Rec<P>, we:
//    a. Look up what P matched in the bindings
//    b. Recursively apply the whole rule set to that subterm
// 3. `recursive(rules)` wraps the rules and provides the self-reference
//
// The recursion is explicit in the rule - you control exactly where
// recursive calls happen. This differs from traversal strategies like
// `bottomup` which automatically recurse into all children.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// REC MARKER
// ============================================================================

template <typename P>
struct Rec : SymbolicTag {
  [[no_unique_address]] P p;

  constexpr Rec() = default;
  constexpr explicit Rec(P pattern) : p{pattern} {}
};

template <typename P>
constexpr auto rec(P p) {
  return Rec<P>{p};
}

template <typename T>
struct IsRec : std::false_type {};

template <typename P>
struct IsRec<Rec<P>> : std::true_type {};

template <typename T>
constexpr bool is_rec_v = IsRec<T>::value;

// Detect if a type contains Rec<...> anywhere in its structure
template <typename T>
struct ContainsRec : std::false_type {};

template <typename P>
struct ContainsRec<Rec<P>> : std::true_type {};

template <typename Op, typename... Args>
struct ContainsRec<Expression<Op, Args...>>
    : std::bool_constant<(ContainsRec<Args>::value || ...)> {};

template <typename T>
constexpr bool contains_rec_v = ContainsRec<T>::value;

// ============================================================================
// RECURSIVE RULE TEMPLATE
// ============================================================================
//
// A rule that may contain rec(...) markers in its replacement.
// Not a strategy by itself - must be wrapped in recursive().
//
// ============================================================================

template <typename Pattern, typename Replacement>
struct RecursiveRule {
  [[no_unique_address]] Pattern pattern;
  [[no_unique_address]] Replacement replacement;

  constexpr RecursiveRule(Pattern p, Replacement r) : pattern{p}, replacement{r} {}
};

// ============================================================================
// rrule() - Explicit recursive rule factory
// ============================================================================
//
// Use rrule() when replacement contains rec(...) markers.
// Alternatively, rule() from rule.h works for non-recursive rules.
//
// Inside recursive(...), both work together via |
//
// ============================================================================

template <typename P, typename R>
constexpr auto rrule(P pattern, R replacement) {
  return RecursiveRule<P, R>{pattern, replacement};
}

// ============================================================================
// SUBSTITUTION WITH RECURSIVE CALLS
// ============================================================================

namespace detail {

// Forward declaration
template <typename T, typename Context, typename Self>
constexpr auto substituteRec(T t, Context const& ctx, Self const& self);

// Rec<P>: lookup binding for P, then recursively apply self
template <typename P, typename Context, typename Self>
constexpr auto substituteRecImpl(Rec<P>, Context const& ctx, Self const& self) {
  auto bound = ctx.template lookup<P::id>();
  return self.apply(bound);
}

// PatternVar: lookup and return bound value
template <typename Unique, typename Context, typename Self>
constexpr auto substituteRecImpl(PatternVar<Unique>, Context const& ctx, Self const&) {
  return ctx.template lookup<PatternVar<Unique>::id>();
}

// Terminal types: return as-is
template <auto V, typename Context, typename Self>
constexpr auto substituteRecImpl(Constant<V>, Context, Self const&) {
  return Constant<V>{};
}

template <long long N, long long D, typename Context, typename Self>
constexpr auto substituteRecImpl(Fraction<N, D>, Context, Self const&) {
  return Fraction<N, D>{};
}

template <typename Id, typename Effect, typename Context, typename Self>
constexpr auto substituteRecImpl(Atom<Id, Effect> atom, Context, Self const&) {
  return atom;
}

// Expression: variadic recursive substitution via index sequence
template <typename Op, typename... Args, typename Context, typename Self>
constexpr auto substituteRecImpl(Expression<Op, Args...> expr, Context const& ctx,
                                  Self const& self) {
  return substituteRecExpr<Op>(expr, ctx, self, MakeIndexSequence<sizeof...(Args)>{});
}

template <typename Op, typename Expr, typename Context, typename Self>
constexpr auto substituteRecExpr(Expr, Context, Self const&, IndexSequence<>) {
  return Expression<Op>{};
}

template <typename Op, typename Expr, typename Context, typename Self, SizeT... Is>
constexpr auto substituteRecExpr(Expr expr, Context const& ctx, Self const& self,
                                  IndexSequence<Is...>) {
  auto results = CompressedTuple{substituteRec(expr.template arg<Is>(), ctx, self)...};
  // decltype on function call gives value type; decltype on get<> would give reference
  if constexpr ((isSame<decltype(substituteRec(expr.template arg<Is>(), ctx, self)), Never> || ...)) {
    return Never{};
  } else {
    return Expression<Op, decltype(substituteRec(expr.template arg<Is>(), ctx, self))...>{
        get<Is>(results)...};
  }
}

// Dispatch
template <typename T, typename Context, typename Self>
constexpr auto substituteRec(T t, Context const& ctx, Self const& self) {
  return substituteRecImpl(t, ctx, self);
}

}  // namespace detail

// ============================================================================
// APPLY RECURSIVE RULE WITH SELF-REFERENCE
// ============================================================================

namespace detail {

// Apply a single RecursiveRule with self-reference
template <typename Pattern, typename Replacement, typename E, typename Self>
constexpr auto applyRecursiveRule(RecursiveRule<Pattern, Replacement> const& rule,
                                   E expr, Self const& self) {
  if constexpr (!match(Pattern{}, E{})) {
    return Never{};  // Structure mismatch
  } else {
    auto bindings = extractBindings(rule.pattern, expr);
    if constexpr (isBindingFailure<decltype(bindings)>()) {
      return Never{};  // Binding failed
    } else {
      return substituteRec(rule.replacement, bindings, self);
    }
  }
}

// Apply a regular Rewrite rule (no rec support, for mixing)
template <typename Pattern, typename Replacement, typename Pred, typename E, typename Self>
constexpr auto applyRecursiveRule(Rewrite<Pattern, Replacement, Pred> const& rule,
                                   E expr, Self const&) {
  auto result = rule.apply(expr);
  // Convert "unchanged" to Never for consistency with choice semantics
  if constexpr (isSame<decltype(result), E>) {
    return Never{};
  } else {
    return result;
  }
}

// Apply Choice<S1, S2> - try first, then second
template <typename S1, typename S2, typename E, typename Self>
constexpr auto applyRecursiveRule(Choice<S1, S2> const& choice, E expr, Self const& self) {
  auto r1 = applyRecursiveRule(choice.s1, expr, self);
  if constexpr (isSame<decltype(r1), Never>) {
    return applyRecursiveRule(choice.s2, expr, self);
  } else {
    return r1;
  }
}

// Apply Id - always returns Never (no match)
template <typename E, typename Self>
constexpr auto applyRecursiveRule(Id, E, Self const&) {
  return Never{};
}

// Apply Fail - always returns Never
template <typename E, typename Self>
constexpr auto applyRecursiveRule(Fail, E, Self const&) {
  return Never{};
}

}  // namespace detail

// ============================================================================
// RECURSIVE WRAPPER - Ties the knot
// ============================================================================

template <typename Rules>
struct Recursive {
  [[no_unique_address]] Rules rules;

  constexpr explicit Recursive(Rules r) : rules{r} {}

  template <Symbolic E>
  constexpr auto apply(E expr) const {
    // Generic handling for any ReduceOver: recurse into inner expression, rebuild
    if constexpr (is_reduce_over_v<E>) {
      auto inner_result = apply(expr.expr());
      return expr.rebuild(inner_result);
    } else {
      auto result = detail::applyRecursiveRule(rules, expr, *this);
      if constexpr (isSame<decltype(result), Never>) {
        return expr;  // No rule matched, return unchanged
      } else {
        return result;
      }
    }
  }
};

template <typename Rules>
constexpr auto recursive(Rules rules) {
  return Recursive<Rules>{rules};
}

}  // namespace tempura::symbolic4
