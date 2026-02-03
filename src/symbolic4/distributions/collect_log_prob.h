#pragma once

#include <type_traits>
#include <utility>  // For std::pair

#include "symbolic4/core.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/sum_over.h"
#include "symbolic4/let.h"  // For IdSet

// ============================================================================
// collect_log_prob.h - Automatic log-probability collection via DAG traversal
// ============================================================================
//
// Problem: Manual listing of random variables is tedious and error-prone.
//
// Current approach (joint.h):
//   auto mu = normal(0.0, 10.0);
//   auto sigma = halfNormal(5.0);
//   auto y = normal(mu, sigma);
//   auto joint = logProb(mu, sigma, y);  // Must list all RVs!
//
// New approach (this file):
//   auto joint = collectLogProbs(y);     // Auto-discovers mu, sigma from y
//
// How it works:
//   1. Traverse the expression tree of y's log-probability
//   2. Detect RandomVarSymbol atoms (Atom<Id, Sample<Dist>>)
//   3. For each discovered RV, compute its log-probability
//   4. Sum all log-probabilities
//   5. Use IdSet to deduplicate (same RV symbol may appear multiple times)
//
// Key insight:
//   When y = normal(mu, sigma) is created, mu.sym() and sigma.sym() appear
//   in y's log-probability expression. These symbols carry their distributions
//   via the Sample<Dist> effect. We extract the distribution and compute
//   logProbFor(sym) to get each term.
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

// Collect from expression children (variadic), threading visited set
template <typename Visited, typename Op>
constexpr auto collectFromChildren(Visited visited, Op) {
  return std::pair{Constant<0>{}, visited};
}

template <typename Visited, typename Op, typename Child>
constexpr auto collectFromChildren(Visited visited, Op, Child child) {
  return collectFromExprImpl(visited, child);
}

template <typename Visited, typename Op, typename C1, typename C2>
constexpr auto collectFromChildren(Visited visited, Op, C1 c1, C2 c2) {
  auto [r1, v1] = collectFromExprImpl(visited, c1);
  auto [r2, v2] = collectFromExprImpl(v1, c2);
  return std::pair{addNonZero(r1, r2), v2};
}

template <typename Visited, typename Op, typename C1, typename C2, typename C3>
constexpr auto collectFromChildren(Visited visited, Op, C1 c1, C2 c2, C3 c3) {
  auto [r1, v1] = collectFromExprImpl(visited, c1);
  auto [r2, v2] = collectFromExprImpl(v1, c2);
  auto [r3, v3] = collectFromExprImpl(v2, c3);
  return std::pair{addNonZero(addNonZero(r1, r2), r3), v3};
}

// Dispatch expression children
template <typename Visited, typename Op, SizeT... Is, typename... Args>
constexpr auto collectFromExpr(Visited visited, Expression<Op, Args...> expr,
                               IndexSequence<Is...>) {
  return collectFromChildren(visited, Op{}, expr.template arg<Is>()...);
}

// Helper to compute constrained expression and Jacobian from distribution
template <typename Support, typename IdType>
constexpr auto constrainedExprFor() {
  auto z = Symbol<IdType>{};
  if constexpr (is_positive_support_v<Support>) {
    return exp(z);
  } else if constexpr (is_unit_support_v<Support>) {
    return 1_c / (1_c + exp(-z));
  } else {
    return z;
  }
}

template <typename Support, typename IdType>
constexpr auto jacobianFor() {
  auto z = Symbol<IdType>{};
  if constexpr (is_positive_support_v<Support>) {
    return z;  // log|d(exp(z))/dz| = z
  } else if constexpr (is_unit_support_v<Support>) {
    auto s = 1_c / (1_c + exp(-z));
    return log(s) + log(1_c - s);  // log(s(1-s))
  } else {
    return 0_c;
  }
}

// Main implementation: collect log-probs from any RandomVarSymbol atoms
// Returns pair{result, updated_visited_set}
template <typename Visited, Symbolic E>
constexpr auto collectFromExprImpl(Visited visited, E expr) {
  if constexpr (is_sum_over_v<E>) {
    // SumOver<DimTag, Body> - recurse into body expression
    // Must check before is_terminal_v since SumOver is not an Expression
    return collectFromExprImpl(visited, expr.expr_);
  } else if constexpr (is_indexed_random_var_atom_v<E>) {
    // Found an indexed random variable (plate)!
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      return std::pair{Constant<0>{}, visited};
    } else {
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = typename E::effect_type::dist_type;
      using DimsList = typename E::effect_type::dims_list;

      // Reconstruct IndexedRandomVar to get logProb() (which includes SumOver)
      // No constraint transform — indexed params handle constraints at bind-time
      auto rv = IndexedRandomVar<Dist, IdType, DimsList>{expr.effect_.dist_};
      auto rv_logprob = rv.logProb();

      // Recursively discover parents from the distribution's parameters
      auto [parent_logprobs, final_visited] =
          collectFromExprImpl(NewVisited{}, rv_logprob);

      return std::pair{addNonZero(rv_logprob, parent_logprobs), final_visited};
    }
  } else if constexpr (is_random_var_atom_v<E>) {
    // Found a scalar random variable!
    using IdType = get_id_t<E>;

    if constexpr (id_set_contains_v<IdType, Visited>) {
      // Already collected this RV's log-prob - skip to avoid double counting
      return std::pair{Constant<0>{}, visited};
    } else {
      // First time seeing this RV - add its log-prob and mark as visited
      using NewVisited = id_set_insert_t<IdType, Visited>;
      using Dist = std::decay_t<decltype(expr.effect_.dist_)>;
      using Support = typename Dist::support_type;

      // Compute log-prob using constrained expression + Jacobian
      // This matches what RandomVar::logProb() computes
      auto constrained = constrainedExprFor<Support, IdType>();
      auto jacobian = jacobianFor<Support, IdType>();
      auto logprob = expr.effect_.dist_.logProbFor(constrained) + jacobian;

      // Recursively discover parents from the distribution's logProb expression
      auto [parent_logprobs, final_visited] =
          collectFromExprImpl(NewVisited{}, logprob);

      return std::pair{addNonZero(logprob, parent_logprobs), final_visited};
    }
  } else if constexpr (is_terminal_v<E>) {
    // Non-RV terminal - return zero
    return std::pair{Constant<0>{}, visited};
  } else {
    // Expression - recurse into children
    return collectFromExpr(visited, expr, MakeIndexSequence<E::arity>{});
  }
}

}  // namespace collect_detail

// ============================================================================
// collectLogProbsFromExpr - Collect log-probs from an expression
// ============================================================================
//
// Traverses the given expression and collects log-probabilities from all
// RandomVarSymbol nodes found. Returns the sum of all log-probs.
//
// This is a lower-level function that works on arbitrary expressions.
// For user-facing API, see collectLogProbs below.

template <Symbolic E>
constexpr auto collectLogProbsFromExpr(E expr) {
  auto [result, visited] = collect_detail::collectFromExprImpl(IdSet<>{}, expr);
  return result;
}

// ============================================================================
// collectLogProbs - Auto-discover log-probs from a RandomVar
// ============================================================================
//
// Given a RandomVar (like y = normal(mu, sigma)), automatically discovers
// all parent RandomVars (mu, sigma) by traversing y's log-probability
// expression, and returns the joint log-probability.
//
// Usage:
//   auto mu = normal(0.0, 10.0);
//   auto sigma = halfNormal(5.0);
//   auto y = normal(mu, sigma);
//   auto joint = collectLogProbs(y);
//   // Returns: logProb(y) + logProb(mu) + logProb(sigma)

template <typename RV>
  requires IsRandomVar<RV> && (!IsIndexedRandomVar<RV>)
constexpr auto collectLogProbs(const RV& rv) {
  // Start with rv's own log-probability
  auto rv_logprob = rv.logProb();

  // Collect log-probs from any RandomVarSymbols appearing in rv's distribution
  auto parent_logprobs = collectLogProbsFromExpr(rv_logprob);

  // Sum rv's log-prob with all discovered parent log-probs
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
    // Already counted this RV - only collect from its expression (for parents)
    auto rv_logprob = rv.logProb();
    return collect_detail::collectFromExprImpl(visited, rv_logprob);
  } else {
    // First time seeing this RV - add it and its parents
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
