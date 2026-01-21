#pragma once

#include "symbolic4/core.h"
#include "symbolic4/let.h"

// fold_unique: catamorphism that tracks visited identities
// When traversing a DAG (where LetNodes share subexpressions), we want to
// compute each unique subexpression only once.

namespace tempura::symbolic4 {

// ============================================================================
// fold_unique - Catamorphism with identity tracking
// ============================================================================
// Like fold, but the context tracks which LetNode identities have been visited.
// When a LetNode is encountered:
//   - If its Id is NOT in the visited set: compute it, add Id to set, cache result
//   - If its Id IS in the visited set: retrieve cached result
//
// The interpreter must define:
//   - terminal(term, ctx) -> result
//   - combine(ctx, op, child_results...) -> result
//   - on_let(ctx, let_node, inner_result) -> result  (for LetNode handling)

template <typename I, Symbolic E, typename Ctx>
constexpr auto fold_unique(E expr, Ctx& ctx);

namespace detail {

template <typename I, typename Op, typename Ctx, SizeT... Is, typename... Args>
constexpr auto foldUniqueExpression(Expression<Op, Args...> expr, Ctx& ctx,
                                     IndexSequence<Is...>) {
  return I::combine(ctx, Op{},
                    fold_unique<I>(expr.template arg<Is>(), ctx)...);
}

}  // namespace detail

template <typename I, Symbolic E, typename Ctx>
constexpr auto fold_unique(E expr, Ctx& ctx) {
  if constexpr (is_let_node_v<E>) {
    // LetNode: check if already visited
    using Id = get_let_id_t<E>;

    if constexpr (id_set_contains_v<Id, typename Ctx::visited_set>) {
      // Already computed - return cached result
      return ctx.template get_cached<Id>();
    } else {
      // Not yet computed - evaluate and cache
      auto result = fold_unique<I>(expr.expr(), ctx);
      ctx.template cache<Id>(result);
      return I::on_let(ctx, expr, result);
    }
  } else if constexpr (is_terminal_v<E>) {
    return I::terminal(expr, ctx);
  } else {
    return detail::foldUniqueExpression<I>(expr, ctx,
                                           MakeIndexSequence<E::arity>{});
  }
}

// ============================================================================
// UniqueContext - Context that tracks visited identities
// ============================================================================
// Template over a cache type that stores computed values.
// The cache maps Id types to their computed results.

template <typename VisitedSet, typename Cache>
struct UniqueContext {
  using visited_set = VisitedSet;
  Cache cache;

  template <typename Id>
  constexpr auto get_cached() const {
    return cache.template get<Id>();
  }

  template <typename Id, typename T>
  constexpr void cache_result(T value) {
    cache.template set<Id>(value);
  }
};

// Simple compile-time cache for homogeneous values (all same type)
template <typename T>
struct SimpleCache {
  // For demonstration - in practice, you'd use a type-indexed map
  // This is a simplified version that works when all values are the same type
  T value{};

  template <typename Id>
  constexpr auto get() const {
    return value;
  }

  template <typename Id>
  constexpr void set(T v) {
    value = v;
  }
};

}  // namespace tempura::symbolic4
