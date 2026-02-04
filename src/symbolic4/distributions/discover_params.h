#pragma once

#include "meta/tuple.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/let.h"

// ============================================================================
// discover_params.h - Automatic parameter discovery from expression trees
// ============================================================================
//
// Discovers latent parameters automatically from observed RVs using direct
// recursion with variadic children. No cata/fold/scheme dependency.
//
// Usage:
//   auto alpha = normal(0.0, 10.0);
//   auto sigma = halfNormal(2.0);
//   auto y = plate<Obs>(normal(alpha, sigma));
//   auto params = discoverParams(y);
//   // Returns tuple<RandomVar<NormalDist<...>, AlphaId>,
//   //               RandomVar<HalfNormalDist<...>, SigmaId>>
//
// ============================================================================

namespace tempura::symbolic4 {

namespace discover_detail {

// Use meta::tupleCat directly - no wrapper needed

// Forward declaration
template <typename Visited, typename Observed, Symbolic E>
constexpr auto discoverFromExprImpl(Visited visited, Observed observed, E expr);

// Variadic child processing: thread visited set through each child sequentially
// Base case: no children
template <typename Visited, typename Observed, typename Accum>
constexpr auto discoverFromChildrenImpl(Visited v, Observed, Accum a) {
  return Tuple{a, v};
}

// Recursive case: process first child, then rest
template <typename Visited, typename Observed, typename Accum, typename C0, typename... Rest>
constexpr auto discoverFromChildrenImpl(Visited v, Observed observed, Accum a, C0 c0, Rest... rest) {
  auto [r, v2] = discoverFromExprImpl(v, observed, c0);
  return discoverFromChildrenImpl(v2, observed, tupleCat(a, r), rest...);
}

// Dispatch expression children via index sequence
template <typename Visited, typename Observed, typename Op, SizeT... Is, typename... Args>
constexpr auto discoverFromExpr(Visited visited, Observed observed,
                                 Expression<Op, Args...> expr, IndexSequence<Is...>) {
  return discoverFromChildrenImpl(visited, observed, Tuple<>{}, expr.template arg<Is>()...);
}

// Check if an Id is in the observed set
template <typename Id, typename ObservedSet>
struct IsObservedId : FalseType {};

template <typename Id, typename... ObsIds>
struct IsObservedId<Id, IdSet<ObsIds...>> {
  static constexpr bool value = (isSame<Id, ObsIds> || ...);
};

template <typename Id, typename ObservedSet>
constexpr bool is_observed_id_v = IsObservedId<Id, ObservedSet>::value;

// Main implementation: discover parameters from expression
template <typename Visited, typename Observed, Symbolic E>
constexpr auto discoverFromExprImpl(Visited visited, Observed observed, E expr) {
  if constexpr (is_reduce_over_v<E>) {
    // ReduceOver<ROp, DimTag, Body> - recurse into body expression
    return discoverFromExprImpl(visited, observed, expr.expr());
  } else if constexpr (is_indexed_random_var_atom_v<E>) {
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      return Tuple{Tuple<>{}, visited};
    } else if constexpr (is_observed_id_v<IdType, Observed>) {
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = typename E::effect_type::dist_type;
      using DimsList = typename E::effect_type::dims_list;
      auto rv = IndexedRandomVar<Dist, IdType, DimsList>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();
      return discoverFromExprImpl(NewVisited{}, observed, rv_logprob);
    } else {
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = typename E::effect_type::dist_type;
      using DimsList = typename E::effect_type::dims_list;

      auto rv = IndexedRandomVar<Dist, IdType, DimsList>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();
      auto [parents, final_visited] = discoverFromExprImpl(NewVisited{}, observed, rv_logprob);

      return Tuple{tupleCat(makeTuple(rv), parents), final_visited};
    }
  } else if constexpr (is_random_var_atom_v<E>) {
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      return Tuple{Tuple<>{}, visited};
    } else if constexpr (is_observed_id_v<IdType, Observed>) {
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using DistType = DecayT<decltype(expr.effect_.dist_)>;
      auto rv = RandomVar<DistType, IdType>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();
      return discoverFromExprImpl(NewVisited{}, observed, rv_logprob);
    } else {
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using DistType = DecayT<decltype(expr.effect_.dist_)>;

      auto rv = RandomVar<DistType, IdType>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();
      auto [parents, final_visited] = discoverFromExprImpl(NewVisited{}, observed, rv_logprob);

      return Tuple{tupleCat(makeTuple(rv), parents), final_visited};
    }
  } else if constexpr (is_terminal_v<E>) {
    return Tuple{Tuple<>{}, visited};
  } else {
    // Expression - recurse into children (variadic)
    return discoverFromExpr(visited, observed, expr, MakeIndexSequence<E::arity>{});
  }
}

}  // namespace discover_detail

// ============================================================================
// discoverParams - Find all latent parameters from observed RVs
// ============================================================================

template <HasLogProb RV>
constexpr auto discoverParams(const RV& observed) {
  using ObservedSet = IdSet<typename RV::id_type>;

  auto expr = observed.logProb();
  auto [params, visited] = discover_detail::discoverFromExprImpl(
      IdSet<>{}, ObservedSet{}, expr);

  return params;
}

// Multiple observed RVs
template <HasLogProb... RVs>
  requires(sizeof...(RVs) > 1)
constexpr auto discoverParams(const RVs&... observed) {
  using ObservedSet = IdSet<typename RVs::id_type...>;

  auto joint = collectLogProbs(observed...);
  auto [params, visited] = discover_detail::discoverFromExprImpl(
      IdSet<>{}, ObservedSet{}, joint);

  return params;
}

}  // namespace tempura::symbolic4
