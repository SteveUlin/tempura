#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

#include "bayes2/core.h"
#include "symbolic3/core.h"

// DAG traversal for computing joint log-probability.
//
// jointLogProb(node) traverses the entire DAG rooted at node, visiting each
// node exactly once (deduplicating shared ancestors), and returns the
// sum of all nodeLogProb() contributions as a symbolic expression.
//
// Deduplication uses a type-level IdSet that accumulates visited node IDs.
// The set is threaded through all recursive calls so shared ancestors
// (diamond dependencies) are only counted once.
//
// The traversal happens entirely at compile time via template metaprogramming.

namespace tempura::bayes2 {

using namespace tempura::symbolic3;

// =============================================================================
// IdSet - Type-level set of visited node IDs for deduplication
// =============================================================================
//
// Each node has a unique id_type (from decltype([]{})), stored in this set
// to track which nodes have been visited. Checking membership and insertion
// are O(N) in the number of types, but N is small for typical models.

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

template <typename Node, typename Visited>
struct Traverse;

// Fold over parents tuple recursively, threading visited set through each step.
// Returns pair of (accumulated log-prob, updated visited set).
template <std::size_t I, typename Parents, typename V, typename Acc>
constexpr auto foldParents(const Parents& parents, V visited, Acc acc) {
  if constexpr (I >= std::tuple_size_v<Parents>) {
    return std::pair{acc, visited};
  } else {
    const auto& parent = std::get<I>(parents);
    using Parent = std::remove_cvref_t<decltype(parent)>;
    auto [lp, new_visited] = Traverse<Parent, V>::apply(parent, visited);
    return foldParents<I + 1>(parents, new_visited, acc + lp);
  }
}

// Traverse all parents (non-empty case)
template <typename... Ps, typename V>
constexpr auto traverseParents(const std::tuple<Ps...>& parents, V visited) {
  return foldParents<0>(parents, visited, Literal{0.0});
}

// Traverse all parents (empty case) - preserve visited set
template <typename V>
constexpr auto traverseParents(const std::tuple<>& /*parents*/, V visited) {
  return std::pair{Literal{0.0}, visited};
}

// Main traversal logic for a single node.
// If already visited (in Visited set), returns 0 contribution.
// Otherwise, traverses parents first, then adds this node's log-prob.
template <typename Node, typename Visited>
struct Traverse {
  static constexpr bool skip =
      Visited::template contains<typename Node::id_type>;
  using NewVisited =
      typename Visited::template insert<typename Node::id_type>;

  static constexpr auto apply(const Node& node, Visited visited) {
    if constexpr (skip) {
      // Already visited - return 0 contribution, preserve visited set
      return std::pair{Literal{0.0}, visited};
    } else {
      // First visit: traverse parents with this node marked as visited,
      // then add this node's contribution
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
// Deduplicates shared ancestors across all roots by threading visited set.

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

}  // namespace tempura::bayes2
