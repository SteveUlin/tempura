#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "bayes/graph/core.h"
#include "symbolic3/core.h"

// DAG traversal for computing joint log-probability.
//
// jointLogProb(rv) traverses the entire DAG rooted at rv, visiting each
// node exactly once (deduplicating shared ancestors), and returns the
// sum of all nodeLogProb() contributions as a symbolic expression.
//
// The traversal is entirely compile-time via template metaprogramming.

namespace tempura::bayes::graph {

using namespace tempura::symbolic3;

// =============================================================================
// IdSet - Type-level set of visited node IDs
// =============================================================================

template <typename... Ids>
struct IdSet {
  template <typename Id>
  static constexpr bool contains = (std::is_same_v<Id, Ids> || ...);

  template <typename Id>
  using insert =
      std::conditional_t<contains<Id>, IdSet<Ids...>, IdSet<Id, Ids...>>;
};

// =============================================================================
// Traverse - DAG traversal accumulating log-prob
// =============================================================================

// Forward declaration
template <typename Node, typename Visited>
struct Traverse;

// Helper to fold over parents tuple
template <std::size_t I, typename Parents, typename V, typename Acc>
constexpr auto foldParents(const Parents& parents, V, Acc acc) {
  if constexpr (I >= std::tuple_size_v<Parents>) {
    return std::pair{acc, V{}};
  } else {
    const auto& parent = std::get<I>(parents);
    using Parent = std::remove_cvref_t<decltype(parent)>;
    auto [lp, new_visited] = Traverse<Parent, V>::apply(parent, V{});
    return foldParents<I + 1>(parents, new_visited, acc + lp);
  }
}

// Helper to traverse all parents
template <typename... Ps, typename V>
constexpr auto traverseParents(const std::tuple<Ps...>& parents, V visited) {
  return foldParents<0>(parents, visited, Literal{0.0});
}

// Empty parents case
template <typename V>
constexpr auto traverseParents(const std::tuple<>& /*parents*/, V /*visited*/) {
  return std::pair{Literal{0.0}, V{}};
}

// Main traversal logic
template <typename Node, typename Visited>
struct Traverse {
  static constexpr bool skip =
      Visited::template contains<typename Node::id_type>;
  using NewVisited =
      typename Visited::template insert<typename Node::id_type>;

  static constexpr auto apply(const Node& node, Visited /*v*/) {
    if constexpr (skip) {
      // Already visited this node
      return std::pair{Literal{0.0}, Visited{}};
    } else {
      // Traverse parents first, then add this node's contribution
      auto [parent_lp, after_parents] =
          traverseParents(node.parents(), NewVisited{});
      auto total = parent_lp + node.nodeLogProb();
      return std::pair{total, after_parents};
    }
  }
};

// =============================================================================
// jointLogProb - Public API
// =============================================================================

// Single root: traverse DAG from one node
template <typename Node>
constexpr auto jointLogProb(const Node& root) {
  return Traverse<Node, IdSet<>>::apply(root, IdSet<>{}).first;
}

// Multiple roots: traverse DAG from multiple nodes (e.g., multiple observations)
// Deduplicates shared ancestors across roots.

template <typename Visited, typename Acc>
constexpr auto jointLogProbMulti(Visited /*visited*/, Acc acc) {
  return acc;
}

template <typename Visited, typename Acc, typename Node, typename... Rest>
constexpr auto jointLogProbMulti(Visited visited, Acc acc, const Node& node,
                                 const Rest&... rest) {
  auto [lp, new_visited] = Traverse<Node, Visited>::apply(node, visited);
  return jointLogProbMulti(new_visited, acc + lp, rest...);
}

template <typename Node, typename... Nodes>
  requires(sizeof...(Nodes) > 0)
constexpr auto jointLogProb(const Node& first, const Nodes&... rest) {
  return jointLogProbMulti(IdSet<>{}, Literal{0.0}, first, rest...);
}

}  // namespace tempura::bayes::graph
