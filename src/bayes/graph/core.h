#pragma once

#include <tuple>

#include "symbolic3/core.h"
#include "symbolic3/derivative.h"

// Core types for expression-graph probabilistic programming.
//
// RandomVar: A stochastic node in the probabilistic DAG
// DeterministicVar: A computed node (deterministic function of parents)
//
// The key insight: each RandomVar carries its full ancestry in its type,
// enabling compile-time DAG traversal and symbolic log-prob computation.

namespace tempura::bayes::graph {

using namespace tempura::symbolic3;

// =============================================================================
// Type Traits
// =============================================================================

template <typename T>
constexpr bool is_random_var = false;

template <typename T>
constexpr bool is_deterministic_var = false;

template <typename T>
concept GraphNode = is_random_var<T> || is_deterministic_var<T>;

// =============================================================================
// RandomVar - A stochastic node in the probabilistic graph
// =============================================================================
//
// Template parameters:
//   Id       - Unique type identity (stateless lambda)
//   Dist     - Distribution type providing logProbFor()
//   Parents  - Parent RandomVars/DeterministicVars (dependency edges)

template <typename Id, typename Dist, typename... Parents>
struct RandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using symbol_type = Symbol<Id>;

  [[no_unique_address]] Dist dist_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  // The symbol representing this random variable's value
  static constexpr auto sym() { return symbol_type{}; }

  // Log-probability for THIS node only: log p(this | parents)
  constexpr auto nodeLogProb() const { return dist_.logProbFor(sym()); }

  // Access parents (for DAG traversal)
  constexpr const auto& parents() const { return parents_; }

  // Binding syntax: mu = 1.0 creates a TypeValueBinder
  // Enables: BinderPack{mu = 1.0, sigma = 2.0}
  constexpr auto operator=(double value) const { return sym() = value; }
};

template <typename Id, typename Dist, typename... Parents>
constexpr bool is_random_var<RandomVar<Id, Dist, Parents...>> = true;

// =============================================================================
// DeterministicVar - A computed node (not sampled)
// =============================================================================
//
// Deterministic variables are defined by expressions over other variables.
// They contribute 0 to log-probability (they're derived, not sampled).

template <typename Id, typename Expr, typename... Parents>
struct DeterministicVar {
  using id_type = Id;
  using expr_type = Expr;
  using symbol_type = Symbol<Id>;

  [[no_unique_address]] Expr expr_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  static constexpr auto sym() { return symbol_type{}; }

  // Deterministic: no log-prob contribution
  constexpr auto nodeLogProb() const { return Literal{0.0}; }

  // The defining expression (for substitution into children)
  constexpr auto expr() const { return expr_; }

  constexpr const auto& parents() const { return parents_; }

  // Binding syntax: y_hat = 5.0
  constexpr auto operator=(double value) const { return sym() = value; }
};

template <typename Id, typename Expr, typename... Parents>
constexpr bool is_deterministic_var<DeterministicVar<Id, Expr, Parents...>> =
    true;

// =============================================================================
// diff() overloads - Accept RandomVar/DeterministicVar directly
// =============================================================================

template <Symbolic Expr, typename Id, typename D, typename... Ps>
constexpr auto diff(Expr expr, const RandomVar<Id, D, Ps...>& rv) {
  return symbolic3::diff(expr, rv.sym());
}

template <Symbolic Expr, typename Id, typename E, typename... Ps>
constexpr auto diff(Expr expr, const DeterministicVar<Id, E, Ps...>& dv) {
  return symbolic3::diff(expr, dv.sym());
}

// =============================================================================
// Argument Handling - Convert distribution args to symbolic form
// =============================================================================

// Convert to symbolic: RandomVar/DeterministicVar -> sym(), arithmetic -> Literal
template <typename T>
constexpr auto toSymbolic(const T& x) {
  if constexpr (is_random_var<T>) {
    return x.sym();
  } else if constexpr (is_deterministic_var<T>) {
    return x.sym();
  } else if constexpr (Symbolic<T>) {
    return x;
  } else {
    return Literal{static_cast<double>(x)};
  }
}

// Extract a graph node as a parent tuple element
template <typename T>
constexpr auto asParentTuple(const T& x) {
  if constexpr (is_random_var<T> || is_deterministic_var<T>) {
    return std::tuple<T>{x};
  } else {
    return std::tuple<>{};
  }
}

// Collect all graph node parents from distribution arguments
template <typename... Args>
constexpr auto collectParents(const Args&... args) {
  return std::tuple_cat(asParentTuple(args)...);
}

}  // namespace tempura::bayes::graph
