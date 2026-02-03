#pragma once

#include <type_traits>
#include <utility>  // For std::pair

#include "symbolic4/constraints.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/let.h"  // For IdSet

// ============================================================================
// collect_log_prob.h - Automatic log-probability collection via DAG traversal
// ============================================================================
//
// Traverses expression trees to discover all RandomVarSymbol atoms and sum
// their log-probabilities. Uses direct recursion with variadic children.
//
// Usage:
//   auto mu = normal(0.0, 10.0);
//   auto sigma = halfNormal(5.0);
//   auto y = normal(mu, sigma);
//   auto joint = collectLogProbs(y);  // Auto-discovers mu, sigma from y
//
// ============================================================================

namespace tempura::symbolic4 {

namespace collect_detail {

// Type trait to check if T is Constant<0>
template <typename T>
struct IsZeroConstant : std::false_type {};

template <>
struct IsZeroConstant<Constant<0>> : std::true_type {};

template <typename T>
constexpr bool is_zero_constant_v = IsZeroConstant<T>::value;

// Helper: Add two results, treating Constant<0> as identity for addition
template <typename A, typename B>
constexpr auto addNonZero(A a, B b) {
  if constexpr (is_zero_constant_v<A>) {
    return b;
  } else if constexpr (is_zero_constant_v<B>) {
    return a;
  } else {
    return a + b;
  }
}

// Forward declaration for recursion
template <typename Visited, Symbolic E>
constexpr auto collectFromExprImpl(Visited visited, E expr);

// Variadic child processing: thread visited set through each child sequentially
// Base case: no children
template <typename Visited, typename Accum>
constexpr auto collectFromChildrenImpl(Visited v, Accum a) {
  return std::pair{a, v};
}

// Recursive case: process first child, then rest
template <typename Visited, typename Accum, typename C0, typename... Rest>
constexpr auto collectFromChildrenImpl(Visited v, Accum a, C0 c0, Rest... rest) {
  auto [r, v2] = collectFromExprImpl(v, c0);
  return collectFromChildrenImpl(v2, addNonZero(a, r), rest...);
}

// Dispatch expression children via index sequence
template <typename Visited, typename Op, SizeT... Is, typename... Args>
constexpr auto collectFromExpr(Visited visited, Expression<Op, Args...> expr,
                               IndexSequence<Is...>) {
  return collectFromChildrenImpl(visited, Constant<0>{}, expr.template arg<Is>()...);
}

// Main implementation: collect log-probs from any RandomVarSymbol atoms
template <typename Visited, Symbolic E>
constexpr auto collectFromExprImpl(Visited visited, E expr) {
  if constexpr (is_reduce_over_v<E>) {
    // ReduceOver<ROp, DimTag, Body> - recurse into body expression
    return collectFromExprImpl(visited, expr.expr());
  } else if constexpr (is_indexed_random_var_atom_v<E>) {
    // Found an indexed random variable (plate)!
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      return std::pair{Constant<0>{}, visited};
    } else {
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = typename E::effect_type::dist_type;
      using DimsList = typename E::effect_type::dims_list;

      auto rv = IndexedRandomVar<Dist, IdType, DimsList>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();

      auto [parent_logprobs, final_visited] =
          collectFromExprImpl(NewVisited{}, rv_logprob);

      return std::pair{addNonZero(rv_logprob, parent_logprobs), final_visited};
    }
  } else if constexpr (is_random_var_atom_v<E>) {
    // Found a scalar random variable!
    // Use the Sample atom directly — constrained-space formulation matching rv.logProb()
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      return std::pair{Constant<0>{}, visited};
    } else {
      using NewVisited = id_set_insert_t<IdType, Visited>;

      auto logprob = expr.effect_.dist_.logProbFor(E{expr.effect_});

      auto [parent_logprobs, final_visited] =
          collectFromExprImpl(NewVisited{}, logprob);

      return std::pair{addNonZero(logprob, parent_logprobs), final_visited};
    }
  } else if constexpr (is_terminal_v<E>) {
    return std::pair{Constant<0>{}, visited};
  } else {
    // Expression - recurse into children (variadic)
    return collectFromExpr(visited, expr, MakeIndexSequence<E::arity>{});
  }
}

}  // namespace collect_detail

// ============================================================================
// collectLogProbsFromExpr - Collect log-probs from an expression
// ============================================================================

template <Symbolic E>
constexpr auto collectLogProbsFromExpr(E expr) {
  auto [result, visited] = collect_detail::collectFromExprImpl(IdSet<>{}, expr);
  return result;
}

// ============================================================================
// collectLogProbs - Auto-discover log-probs from a RandomVar
// ============================================================================

template <typename RV>
  requires IsRandomVar<RV> && (!IsIndexedRandomVar<RV>)
constexpr auto collectLogProbs(const RV& rv) {
  auto rv_logprob = rv.logProb();
  auto parent_logprobs = collectLogProbsFromExpr(rv_logprob);
  return collect_detail::addNonZero(rv_logprob, parent_logprobs);
}

// ============================================================================
// Concept: something that has an id_type and logProb()
// ============================================================================

template <typename T>
concept HasLogProb = requires(T t) {
  typename T::id_type;
  { t.logProb() };
};

// ============================================================================
// Unified collectLogProbsWithVisited - handles both RandomVar and IndexedRandomVar
// ============================================================================

template <typename Visited, HasLogProb RV>
constexpr auto collectLogProbsWithVisited(Visited visited, const RV& rv) {
  using IdType = typename RV::id_type;

  if constexpr (id_set_contains_v<IdType, Visited>) {
    auto rv_logprob = rv.logProb();
    return collect_detail::collectFromExprImpl(visited, rv_logprob);
  } else {
    using NewVisited = id_set_insert_t<IdType, Visited>;
    auto rv_logprob = rv.logProb();
    auto [parent_logprobs, final_visited] =
        collect_detail::collectFromExprImpl(NewVisited{}, rv_logprob);
    return std::pair{collect_detail::addNonZero(rv_logprob, parent_logprobs), final_visited};
  }
}

// ============================================================================
// Unified collectLogProbsMulti - handles any mix of RV types
// ============================================================================

// Base case: no more RVs
template <typename Visited, typename Accum>
constexpr auto collectLogProbsMulti(Visited visited, Accum accum) {
  return std::pair{accum, visited};
}

// Recursive case: process one RV and continue
template <typename Visited, typename Accum, HasLogProb RV, HasLogProb... Rest>
constexpr auto collectLogProbsMulti(Visited visited, Accum accum, const RV& rv,
                                    const Rest&... rest) {
  auto [rv_result, new_visited] = collectLogProbsWithVisited(visited, rv);
  auto new_accum = collect_detail::addNonZero(accum, rv_result);
  return collectLogProbsMulti(new_visited, new_accum, rest...);
}

// ============================================================================
// collectLogProbs - public API
// ============================================================================

// Single IndexedRandomVar
template <IsIndexedRandomVar RV>
constexpr auto collectLogProbs(const RV& rv) {
  auto rv_logprob = rv.logProb();
  auto parent_logprobs = collectLogProbsFromExpr(rv_logprob);
  return collect_detail::addNonZero(rv_logprob, parent_logprobs);
}

// Multiple random variables (any mix of RandomVar and IndexedRandomVar)
template <HasLogProb... RVs>
  requires (sizeof...(RVs) > 1)
constexpr auto collectLogProbs(const RVs&... rvs) {
  auto [result, visited] = collectLogProbsMulti(IdSet<>{}, Constant<0>{}, rvs...);
  return result;
}

}  // namespace tempura::symbolic4
