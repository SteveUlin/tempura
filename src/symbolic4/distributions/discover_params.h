#pragma once

#include <tuple>
#include <type_traits>
#include <utility>

#include "symbolic4/core.h"
#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/let.h"

// ============================================================================
// discover_params.h - Automatic parameter discovery from expression trees
// ============================================================================
//
// Problem: Users must manually list all parameters in infer():
//   infer(alpha, beta, sigma, y).bind(...)  // Redundant!
//
// Solution: Discover parameters automatically from observed RVs:
//   infer(y).bind(...)  // y depends on alpha, beta, sigma
//
// How it works:
//   1. Traverse the observed RV's expression tree
//   2. Find all Atom<Id, Sample<Dist>> nodes (random variable atoms)
//   3. Reconstruct the actual RandomVar<Dist, Id> from each atom
//   4. Return a tuple of RandomVar objects (same types as originals!)
//
// Key insight: The atom contains everything needed to reconstruct the
// original RandomVar - the Id type and the distribution instance.
//
// ============================================================================

namespace tempura::symbolic4 {

namespace discover_detail {

// Helper: concatenate tuples
template <typename A, typename B>
constexpr auto tupleCat(A a, B b) {
  return std::tuple_cat(a, b);
}

// Forward declaration
template <typename Visited, typename Observed, Symbolic E>
constexpr auto discoverFromExprImpl(Visited visited, Observed observed, E expr);

// Process children of an expression, threading visited set
template <typename Visited, typename Observed, typename Op>
constexpr auto discoverFromChildren(Visited visited, Observed, Op) {
  return std::pair{std::tuple<>{}, visited};
}

template <typename Visited, typename Observed, typename Op, typename Child>
constexpr auto discoverFromChildren(Visited visited, Observed observed, Op, Child child) {
  return discoverFromExprImpl(visited, observed, child);
}

template <typename Visited, typename Observed, typename Op, typename C1, typename C2>
constexpr auto discoverFromChildren(Visited visited, Observed observed, Op, C1 c1, C2 c2) {
  auto [r1, v1] = discoverFromExprImpl(visited, observed, c1);
  auto [r2, v2] = discoverFromExprImpl(v1, observed, c2);
  return std::pair{tupleCat(r1, r2), v2};
}

template <typename Visited, typename Observed, typename Op, typename C1, typename C2, typename C3>
constexpr auto discoverFromChildren(Visited visited, Observed observed, Op, C1 c1, C2 c2, C3 c3) {
  auto [r1, v1] = discoverFromExprImpl(visited, observed, c1);
  auto [r2, v2] = discoverFromExprImpl(v1, observed, c2);
  auto [r3, v3] = discoverFromExprImpl(v2, observed, c3);
  return std::pair{tupleCat(tupleCat(r1, r2), r3), v3};
}

// Dispatch expression children
template <typename Visited, typename Observed, typename Op, SizeT... Is, typename... Args>
constexpr auto discoverFromExpr(Visited visited, Observed observed,
                                 Expression<Op, Args...> expr, IndexSequence<Is...>) {
  return discoverFromChildren(visited, observed, Op{}, expr.template arg<Is>()...);
}

// Check if an Id is in the observed set
template <typename Id, typename ObservedSet>
struct IsObservedId : std::false_type {};

template <typename Id, typename... ObsIds>
struct IsObservedId<Id, IdSet<ObsIds...>> {
  static constexpr bool value = (std::is_same_v<Id, ObsIds> || ...);
};

template <typename Id, typename ObservedSet>
constexpr bool is_observed_id_v = IsObservedId<Id, ObservedSet>::value;

// Main implementation: discover parameters from expression
// Returns pair{tuple_of_RandomVars, updated_visited_set}
template <typename Visited, typename Observed, Symbolic E>
constexpr auto discoverFromExprImpl(Visited visited, Observed observed, E expr) {
  if constexpr (is_sum_over_v<E>) {
    // SumOver<DimTag, Body> - recurse into body expression
    // Must check before is_terminal_v since SumOver is not an Expression
    return discoverFromExprImpl(visited, observed, expr.expr_);
  } else if constexpr (is_indexed_random_var_atom_v<E>) {
    // Found an indexed random variable atom: Atom<Id, IndexedSample<Dist, DimsList>>
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      return std::pair{std::tuple<>{}, visited};
    } else if constexpr (is_observed_id_v<IdType, Observed>) {
      // Observed indexed RV - still traverse its distribution to find latent parents
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = typename E::effect_type::dist_type;
      using DimsList = typename E::effect_type::dims_list;
      auto rv = IndexedRandomVar<Dist, IdType, DimsList>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();
      return discoverFromExprImpl(NewVisited{}, observed, rv_logprob);
    } else {
      // New latent indexed parameter - reconstruct IndexedRandomVar
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = typename E::effect_type::dist_type;
      using DimsList = typename E::effect_type::dims_list;

      auto rv = IndexedRandomVar<Dist, IdType, DimsList>{expr.effect_.dist_};

      // Also discover parents from distribution params (e.g., sigma in Normal(0, sigma))
      auto rv_logprob = rv.logProb();
      auto [parents, final_visited] = discoverFromExprImpl(NewVisited{}, observed, rv_logprob);

      return std::pair{std::tuple_cat(std::make_tuple(rv), parents), final_visited};
    }
  } else if constexpr (is_random_var_atom_v<E>) {
    // Found a scalar random variable atom: Atom<Id, Sample<Dist>>
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      // Already discovered - skip
      return std::pair{std::tuple<>{}, visited};
    } else if constexpr (is_observed_id_v<IdType, Observed>) {
      // This is an observed RV - don't include in params, but mark visited
      // Still traverse to find latent parents
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using DistType = std::decay_t<decltype(expr.effect_.dist_)>;
      auto rv = RandomVar<DistType, IdType>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();
      return discoverFromExprImpl(NewVisited{}, observed, rv_logprob);
    } else {
      // New latent parameter - reconstruct the actual RandomVar!
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using DistType = std::decay_t<decltype(expr.effect_.dist_)>;

      // Construct RandomVar<Dist, Id> - same type as original
      auto rv = RandomVar<DistType, IdType>{expr.effect_.dist_};

      // Also discover parents from this RV's distribution parameters
      auto rv_logprob = rv.logProb();
      auto [parents, final_visited] = discoverFromExprImpl(NewVisited{}, observed, rv_logprob);

      return std::pair{std::tuple_cat(std::make_tuple(rv), parents), final_visited};
    }
  } else if constexpr (is_terminal_v<E>) {
    // Non-RV terminal - nothing to discover
    return std::pair{std::tuple<>{}, visited};
  } else {
    // Expression - recurse into children
    return discoverFromExpr(visited, observed, expr, MakeIndexSequence<E::arity>{});
  }
}

}  // namespace discover_detail

// ============================================================================
// discoverParams - Find all latent parameters from observed RVs
// ============================================================================
//
// Given one or more observed random variables, traverses their expression
// trees to discover all latent parameters (random variables they depend on).
//
// Returns a tuple of RandomVar objects - the SAME types as the originals!
// This means samples[alpha] will work because the types match exactly.
//
// Usage:
//   auto alpha = normal(0.0, 10.0);
//   auto sigma = halfNormal(2.0);
//   auto y = plate<Obs>(normal(alpha, sigma));
//   auto params = discoverParams(y);
//   // Returns tuple<RandomVar<NormalDist<...>, AlphaId>,
//   //               RandomVar<HalfNormalDist<...>, SigmaId>>

template <HasLogProb RV>
constexpr auto discoverParams(const RV& observed) {
  // Mark the observed RV's ID so we don't include it in params
  using ObservedSet = IdSet<typename RV::id_type>;

  // Traverse the observed RV's log-prob expression
  auto expr = observed.logProb();
  auto [params, visited] = discover_detail::discoverFromExprImpl(
      IdSet<>{}, ObservedSet{}, expr);

  return params;
}

// Multiple observed RVs
template <HasLogProb... RVs>
  requires(sizeof...(RVs) > 1)
constexpr auto discoverParams(const RVs&... observed) {
  // Mark all observed RV IDs
  using ObservedSet = IdSet<typename RVs::id_type...>;

  // Collect log-probs and traverse
  auto joint = collectLogProbs(observed...);
  auto [params, visited] = discover_detail::discoverFromExprImpl(
      IdSet<>{}, ObservedSet{}, joint);

  return params;
}

}  // namespace tempura::symbolic4
