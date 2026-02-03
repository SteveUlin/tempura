#pragma once

#include <type_traits>
#include <utility>

#include "symbolic4/core.h"
#include "symbolic4/let.h"

// ============================================================================
// fold_unique.h - Catamorphism with identity tracking for DAG traversal
// ============================================================================
//
// What problem does this solve?
//
// Regular fold treats expressions as trees, visiting every node. But LetNode
// creates DAGs (Directed Acyclic Graphs) where subexpressions are shared:
//
//   TEMPURA_LET(t, x * x);       // t = x*x, computed once
//   auto expr = t.sym() + t.sym() + t.sym();  // References t three times
//
// A naive tree fold would evaluate x*x three times. fold_unique tracks which
// LetNode Ids have been visited and returns cached results for revisits.
//
// Interpreter Interface:
//
// fold_unique requires an interpreter I that provides:
//   - terminal(term, visited) -> result_type
//       Process a leaf node (Symbol, Constant, Literal, etc.)
//
//   - combine(visited, Op, child_results...) -> result_type
//       Combine results from an Expression's children
//
//   - identity(visited) -> result_type
//       What to return when revisiting an already-processed LetNode
//       For counting: return 0 (don't count again)
//
//   - on_let(visited, let_node, inner_result) -> result_type  [optional]
//       Called after processing a LetNode for the first time
//       Default: just returns inner_result
//
// Context (Visited Set):
//
// Uses IdSet<...> from let.h for compile-time tracking. The visited set is
// threaded through functionally - each call returns both the result AND the
// updated visited set as std::pair. This enables full constexpr evaluation.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Interpreter trait detection
// ============================================================================

namespace detail {

// Detect if interpreter has on_let method
template <typename I, typename Visited, typename Let, typename R,
          typename = void>
struct HasOnLet : std::false_type {};

template <typename I, typename Visited, typename Let, typename R>
struct HasOnLet<
    I, Visited, Let, R,
    std::void_t<decltype(I::on_let(kDeclVal<Visited>(), kDeclVal<Let>(),
                                   kDeclVal<R>()))>> : std::true_type {};

template <typename I, typename Visited, typename Let, typename R>
constexpr bool has_on_let_v = HasOnLet<I, Visited, Let, R>::value;

}  // namespace detail

// ============================================================================
// foldUnique - Main entry point (constexpr)
// ============================================================================
//
// Traverses expression DAG, tracking visited LetNode Ids at compile time.
// Returns std::pair{result, updated_visited_set}.

template <typename I, Symbolic E, typename Visited = IdSet<>>
constexpr auto foldUnique(E expr, Visited visited = {});

// Forward declare for recursion
namespace detail {

// Fold children left-to-right, threading visited set through.
//
// Why explicit arity overloads instead of variadic fold?
// Variadic pack expansion can't easily thread state between elements in constexpr
// context. A fold expression like `(foldUnique<I>(args, visited), ...)` would use
// the same `visited` for all children. We need sequential evaluation where each
// child's result updates the visited set for the next child.

template <typename I, typename Op, typename Visited>
constexpr auto foldChildren(Visited visited) {
  // Nullary operator
  return std::pair{I::combine(visited, Op{}), visited};
}

template <typename I, typename Op, typename Visited, typename Child>
constexpr auto foldChildren(Visited visited, Child child) {
  auto [r, v] = foldUnique<I>(child, visited);
  return std::pair{I::combine(v, Op{}, r), v};
}

template <typename I, typename Op, typename Visited, typename C1, typename C2>
constexpr auto foldChildren(Visited visited, C1 c1, C2 c2) {
  auto [r1, v1] = foldUnique<I>(c1, visited);
  auto [r2, v2] = foldUnique<I>(c2, v1);
  return std::pair{I::combine(v2, Op{}, r1, r2), v2};
}

template <typename I, typename Op, typename Visited, typename C1, typename C2,
          typename C3>
constexpr auto foldChildren(Visited visited, C1 c1, C2 c2, C3 c3) {
  auto [r1, v1] = foldUnique<I>(c1, visited);
  auto [r2, v2] = foldUnique<I>(c2, v1);
  auto [r3, v3] = foldUnique<I>(c3, v2);
  return std::pair{I::combine(v3, Op{}, r1, r2, r3), v3};
}

// Dispatch expression based on arity
template <typename I, typename Op, typename Visited, SizeT... Is,
          typename... Args>
constexpr auto foldExpression(Expression<Op, Args...> expr, Visited visited,
                              IndexSequence<Is...>) {
  return foldChildren<I, Op>(visited, expr.template arg<Is>()...);
}

}  // namespace detail

template <typename I, Symbolic E, typename Visited>
constexpr auto foldUnique(E expr, Visited visited) {
  using ResultType = typename I::result_type;

  if constexpr (is_let_node_v<E>) {
    using Id = get_let_id_t<E>;

    if constexpr (id_set_contains_v<Id, Visited>) {
      // Already visited - return identity, visited unchanged
      return std::pair{I::identity(visited), visited};
    } else {
      // First visit - mark as visited, then process inner
      using NewVisited = id_set_insert_t<Id, Visited>;
      auto [inner_result, final_visited] =
          foldUnique<I>(expr.expr(), NewVisited{});

      if constexpr (detail::has_on_let_v<I, decltype(final_visited), E,
                                         ResultType>) {
        return std::pair{I::on_let(final_visited, expr, inner_result),
                         final_visited};
      } else {
        return std::pair{inner_result, final_visited};
      }
    }
  } else if constexpr (is_terminal_v<E>) {
    return std::pair{I::terminal(expr, visited), visited};
  } else {
    return detail::foldExpression<I>(expr, visited,
                                     MakeIndexSequence<E::arity>{});
  }
}

// Convenience: just return the result value
template <typename I, Symbolic E>
constexpr auto foldUniqueValue(E expr) -> typename I::result_type {
  return foldUnique<I>(expr, IdSet<>{}).first;
}

}  // namespace tempura::symbolic4
