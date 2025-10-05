#pragma once

#include "symbolic2/pattern_matching.h"

namespace tempura {

// Rewrite rules with recursive function calls (for differentiation, etc.)
//
// Standard rewrites: pow(x_, 0_c) → 1_c
// Recursive rewrites: f_ + g_ → diff_(f_) + diff_(g_)  [requires recursive
// call]

// Rewrite rule supporting recursive calls in replacement expression
template <typename Pattern, typename Replacement,
          typename Predicate = NoPredicate>
struct RecursiveRewrite {
  Pattern pattern;
  Replacement replacement;
  [[no_unique_address]] Predicate predicate = {};

  template <Symbolic S>
  static constexpr bool matches(S expr) {
    if constexpr (!match(Pattern{}, expr)) {
      return false;
    } else {
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);
      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return false;
      } else {
        return Predicate{}(bindings_ctx);
      }
    }
  }

  template <Symbolic S, typename RecursiveFn, typename... Args>
  static constexpr auto apply([[maybe_unused]] S expr, RecursiveFn recursive_fn,
                              [[maybe_unused]] Args... args) {
    if constexpr (!match(Pattern{}, expr)) {
      return expr;
    } else {
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);

      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return expr;
      } else {
        if constexpr (!Predicate{}(bindings_ctx)) {
          return expr;
        } else {
          if constexpr (requires {
                          Replacement{}(bindings_ctx, recursive_fn, args...);
                        }) {
            return Replacement{}(bindings_ctx, recursive_fn, args...);
          } else {
            return substitute(Replacement{}, bindings_ctx);
          }
        }
      }
    }
  }
};

// Deduction guides
template <typename P, typename R>
RecursiveRewrite(P, R) -> RecursiveRewrite<P, R, NoPredicate>;

template <typename P, typename R, typename Pred>
RecursiveRewrite(P, R, Pred) -> RecursiveRewrite<P, R, Pred>;

// Sequential application of recursive rewrite rules (first match wins)
template <typename... Rules>
struct RecursiveRewriteSystem {
  constexpr RecursiveRewriteSystem(Rules...) {}

  template <std::size_t Index = 0, Symbolic S, typename RecursiveFn,
            typename... Args>
  static constexpr auto apply(S expr, RecursiveFn recursive_fn, Args... args) {
    if constexpr (Index >= sizeof...(Rules)) {
      return expr;
    } else {
      using CurrentRule = typename detail::GetNthType<Index, Rules...>::type;

      if constexpr (CurrentRule::matches(expr)) {
        return CurrentRule::apply(expr, recursive_fn, args...);
      } else {
        return apply<Index + 1>(expr, recursive_fn, args...);
      }
    }
  }
};

// Deduction guide
template <typename... Rules>
RecursiveRewriteSystem(Rules...) -> RecursiveRewriteSystem<Rules...>;

// =============================================================================
// USAGE EXAMPLE - Differentiation
// =============================================================================

/*

// Define differentiation rules using RecursiveRewrite:

// Base case: d/dx(x) = 1
constexpr auto DiffSelf = RecursiveRewrite{
  x_,  // Pattern
  1_c  // Replacement (no recursion needed)
};

// Sum rule: d/dx(f + g) = df/dx + dg/dx
constexpr auto DiffSum = RecursiveRewrite{
  f_ + g_,  // Pattern
  [](auto ctx, auto diff_fn, auto var) {
    // Extract matched subexpressions
    constexpr auto f = get(ctx, f_);
    constexpr auto g = get(ctx, g_);
    // Recursively differentiate
    return diff_fn(f, var) + diff_fn(g, var);
  }
};

// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
constexpr auto DiffProduct = RecursiveRewrite{
  f_ * g_,  // Pattern
  [](auto ctx, auto diff_fn, auto var) {
    constexpr auto f = get(ctx, f_);
    constexpr auto g = get(ctx, g_);
    return diff_fn(f, var) * g + f * diff_fn(g, var);
  }
};

// Create a rewrite system
constexpr auto DiffRules = RecursiveRewriteSystem{
  DiffSelf,
  DiffSum,
  DiffProduct
};

// Use it:
template <Symbolic Expr, Symbolic Var>
constexpr auto diff(Expr expr, Var var) {
  return DiffRules.apply(expr, diff, var);
}

*/

}  // namespace tempura
