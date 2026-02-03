#pragma once

#include "symbolic4/core.h"
#include "symbolic4/strategy/pattern.h"
#include "symbolic4/strategy/combinator.h"

// ============================================================================
// REWRITE RULES
// ============================================================================
//
// A Rewrite rule is a strategy that matches a pattern and substitutes.
// A RewriteSystem is just Choice<Rule1, Choice<Rule2, ... Choice<RuleN, Id>>>
//
// This makes rules composable with all strategy combinators.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// REWRITE - A single rewrite rule (is itself a Strategy)
// ============================================================================

template <typename Pattern, typename Replacement, typename Predicate = NoPredicate>
struct Rewrite {
  [[no_unique_address]] Pattern pattern{};
  [[no_unique_address]] Replacement replacement{};
  [[no_unique_address]] Predicate predicate{};

  constexpr Rewrite() = default;
  constexpr Rewrite(Pattern p, Replacement r)
      : pattern{p}, replacement{r}, predicate{} {}
  constexpr Rewrite(Pattern p, Replacement r, Predicate pred)
      : pattern{p}, replacement{r}, predicate{pred} {}

  // Strategy interface: apply returns transformed expr or original if no match
  template <Symbolic S>
  constexpr auto apply(S expr) const {
    if constexpr (!match(Pattern{}, S{})) {
      return expr;  // Type-level structural mismatch
    } else {
      auto bindings = extractBindings(pattern, expr);
      if constexpr (detail::isBindingFailure<decltype(bindings)>()) {
        return expr;  // Binding failed
      } else {
        // Could check predicate here at runtime
        // For now, assume predicates are always true or type-level
        return substitute(replacement, bindings);
      }
    }
  }
};

// Deduction guides
template <typename P, typename R>
Rewrite(P, R) -> Rewrite<P, R, NoPredicate>;

template <typename P, typename R, typename Pred>
Rewrite(P, R, Pred) -> Rewrite<P, R, Pred>;

// ============================================================================
// REWRITE SYSTEM - Just a long Choice chain ending in Id
// ============================================================================
// RewriteSystem{r1, r2, r3} = Choice<r1, Choice<r2, Choice<r3, Id>>>

namespace detail {

// Build nested Choice from rules
template <typename... Rules>
struct MakeRuleSystem;

// Base case: single rule -> Choice<Rule, Id>
template <typename Rule>
struct MakeRuleSystem<Rule> {
  using type = Choice<Rule, Id>;

  static constexpr auto make(Rule r) {
    return Choice<Rule, Id>{r, Id{}};
  }
};

// Recursive case: Rule, Rest... -> Choice<Rule, MakeRuleSystem<Rest...>>
template <typename Rule, typename... Rest>
struct MakeRuleSystem<Rule, Rest...> {
  using Inner = typename MakeRuleSystem<Rest...>::type;
  using type = Choice<Rule, Inner>;

  static constexpr auto make(Rule r, Rest... rest) {
    return Choice<Rule, Inner>{r, MakeRuleSystem<Rest...>::make(rest...)};
  }
};

}  // namespace detail

// RewriteSystem is a type alias for the nested Choice
template <typename... Rules>
using RewriteSystem = typename detail::MakeRuleSystem<Rules...>::type;

// Factory function to create a RewriteSystem
template <typename... Rules>
constexpr auto rules(Rules... rs) {
  return detail::MakeRuleSystem<Rules...>::make(rs...);
}

// ============================================================================
// CONVENIENCE
// ============================================================================

template <typename P, typename R>
constexpr auto rule(P pattern, R replacement) {
  return Rewrite{pattern, replacement};
}

template <typename P, typename R, typename Pred>
constexpr auto rule_if(P pattern, R replacement, Pred predicate) {
  return Rewrite{pattern, replacement, predicate};
}

// Legacy helper - applies rule directly (for testing)
template <typename Pattern, typename Replacement, typename Predicate, Symbolic S>
constexpr auto applyRule(Rewrite<Pattern, Replacement, Predicate> const& r, S expr) {
  return r.apply(expr);
}

}  // namespace tempura::symbolic4
