#pragma once

#include "symbolic2/pattern_matching.h"

namespace tempura {

// =============================================================================
// RECURSIVE REWRITE - Support for recursive transformations
// =============================================================================
//
// The standard Rewrite system works great for simple pattern → replacement
// transformations, but differentiation requires recursive calls:
//
//   d/dx(f + g) = d/dx(f) + d/dx(g)
//                 ^^^^^^^^   ^^^^^^^^
//                 Recursive calls to diff()
//
// This file provides RecursiveRewrite which allows replacement expressions
// to be lambdas that can call back to the recursive function.

// A rewrite rule that supports recursive transformations
// The Replacement can be either:
//   1. A symbolic expression (like regular Rewrite)
//   2. A lambda that takes (context, recursive_fn) and returns an expression
template <typename Pattern, typename Replacement,
          typename Predicate = NoPredicate>
struct RecursiveRewrite {
  Pattern pattern;
  Replacement replacement;
  [[no_unique_address]] Predicate predicate = {};

  // Check if pattern matches expression and predicate holds
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

  // Apply with a recursive function
  // RecursiveFn should be a callable like: auto fn(expr, ...args)
  template <Symbolic S, typename RecursiveFn, typename... Args>
  static constexpr auto apply([[maybe_unused]] S expr, RecursiveFn recursive_fn,
                              [[maybe_unused]] Args... args) {
    if constexpr (!match(Pattern{}, expr)) {
      return expr;  // Pattern doesn't match
    } else {
      auto bindings_ctx = detail::extractBindings(Pattern{}, expr);

      if constexpr (detail::isBindingFailure<decltype(bindings_ctx)>()) {
        return expr;  // Binding failed
      } else {
        if constexpr (!Predicate{}(bindings_ctx)) {
          return expr;  // Predicate failed
        } else {
          // Check if Replacement is callable (lambda) or symbolic expression
          if constexpr (requires {
                          Replacement{}(bindings_ctx, recursive_fn, args...);
                        }) {
            // Lambda replacement: call it with context and recursive function
            return Replacement{}(bindings_ctx, recursive_fn, args...);
          } else {
            // Symbolic expression replacement: just substitute
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

// =============================================================================
// RECURSIVE REWRITE SYSTEM
// =============================================================================

// Apply a sequence of recursive rewrite rules
template <typename... Rules>
struct RecursiveRewriteSystem {
  constexpr RecursiveRewriteSystem(Rules...) {}

  template <std::size_t Index = 0, Symbolic S, typename RecursiveFn,
            typename... Args>
  static constexpr auto apply(S expr, RecursiveFn recursive_fn, Args... args) {
    if constexpr (Index >= sizeof...(Rules)) {
      return expr;  // No rules matched
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
