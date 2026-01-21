#pragma once

#include <tuple>
#include <utility>

#include "symbolic3/core.h"
#include "symbolic3/derivative.h"

// Core types for bayes2 probabilistic programming.
//
// RandomVar: Stochastic node with distribution and parent dependencies
// DeterministicVar: Computed node (deterministic function of parents)
//
// The DAG structure is encoded entirely in types via parent tuples. Each node
// stores its parent nodes, enabling compile-time traversal to compute joint
// log-probability as a symbolic expression.
//
// CONSTEXPR DESIGN: All model construction is constexpr - the compiler builds
// the probability DAG and derives gradient expressions at compile time. Only
// evaluation (with concrete values) and sampling (requires RNG) happen at
// runtime. This enables:
//   - Zero-cost model specification (no runtime graph building)
//   - Symbolic autodiff for exact gradient expressions
//   - Aggressive inlining since expression trees are known at compile time

namespace tempura::bayes2 {

using namespace tempura::symbolic3;

// =============================================================================
// Type Traits and Concepts
// =============================================================================

template <typename T>
constexpr bool is_random_var = false;

template <typename T>
constexpr bool is_deterministic_var = false;

// GraphNode: any node in the probabilistic DAG (random or deterministic).
// Prescriptive concept requiring the actual interface, not just type membership.
// This catches mismatches at usage sites rather than deep in template instantiation.
template <typename T>
concept GraphNode = requires(const T& node) {
  typename T::id_type;                   // Unique type identity for deduplication
  typename T::symbol_type;               // Symbol type for symbolic expressions
  { T::sym() } -> Symbolic;              // Static symbol accessor
  { node.nodeLogProb() } -> Symbolic;    // Log-probability contribution
  { node.parents() };                    // Parent tuple for DAG traversal
  { node = std::declval<double>() };     // Binding syntax: node = value
};

// =============================================================================
// RandomVar - Stochastic node in the probabilistic graph
// =============================================================================
//
// Template parameters:
//   Id      - Unique type identity (stateless lambda from call site)
//   Dist    - Distribution type with logProbFor() method
//   Parents - Parent GraphNodes this variable depends on
//
// The Id type comes from decltype([]{}), which generates a unique lambda type
// at each call site. This gives every RandomVar a distinct type identity for
// DAG deduplication during traversal.

template <typename Id, typename Dist, typename... Parents>
struct RandomVar {
  using id_type = Id;
  using dist_type = Dist;
  using symbol_type = Symbol<Id>;

  // [[no_unique_address]]: Dist is often stateless (empty struct with just
  // logProbFor method), so this packs it into zero bytes.
  [[no_unique_address]] Dist dist_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  // Unique symbol for this variable's value in symbolic expressions
  static constexpr auto sym() { return symbol_type{}; }

  // Log-probability contribution: log p(this | parents)
  constexpr auto nodeLogProb() const { return dist_.logProbFor(sym()); }

  // Access parent nodes for DAG traversal
  constexpr const auto& parents() const { return parents_; }

  // Binding syntax for evaluation: mu = 1.0 creates Symbol<Id>=1.0 binder
  // Accepts any value type (double, Dual, etc.) for forward-mode autodiff
  constexpr auto operator=(auto value) const { return sym() = value; }
};

template <typename Id, typename Dist, typename... Parents>
constexpr bool is_random_var<RandomVar<Id, Dist, Parents...>> = true;

// =============================================================================
// DeterministicVar - Computed node (not sampled)
// =============================================================================
//
// Deterministic variables are defined by expressions over other variables.
// They contribute 0 to log-probability since they're derived, not sampled.
// Created automatically when arithmetic is performed on GraphNodes.

template <typename Id, typename Expr, typename... Parents>
struct DeterministicVar {
  using id_type = Id;
  using expr_type = Expr;
  using symbol_type = Symbol<Id>;

  [[no_unique_address]] Expr expr_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  static constexpr auto sym() { return symbol_type{}; }

  // Deterministic nodes contribute 0 to log-prob (they're derived quantities)
  constexpr auto nodeLogProb() const { return Literal{0.0}; }

  // The defining expression (for substitution into dependent nodes)
  constexpr auto expr() const { return expr_; }

  constexpr const auto& parents() const { return parents_; }

  constexpr auto operator=(auto value) const { return sym() = value; }
};

template <typename Id, typename Expr, typename... Parents>
constexpr bool is_deterministic_var<DeterministicVar<Id, Expr, Parents...>> =
    true;

// =============================================================================
// Argument Conversion Helpers
// =============================================================================
//
// These helpers convert distribution arguments to symbolic form and extract
// parent dependencies. This allows natural syntax like normal(mu, sigma)
// where mu/sigma can be RandomVars, scalars, or symbolic expressions.

// Convert to symbolic form for building compile-time expression DAGs.
// GraphNodes become their symbol; scalars become Literal.
template <typename T>
constexpr auto toSymbolic(const T& x) {
  if constexpr (GraphNode<T>) {
    return x.sym();
  } else if constexpr (Symbolic<T>) {
    return x;
  } else {
    return Literal{static_cast<double>(x)};
  }
}

// Extract GraphNode as parent tuple element for dependency tracking.
// Returns single-element tuple if T is a GraphNode, empty tuple otherwise.
// The asymmetry with toSymbolic() is intentional: toSymbolic converts values,
// while this filters types for parent collection.
template <typename T>
constexpr auto nodeAsParentTuple(const T& x) {
  if constexpr (GraphNode<T>) {
    return std::tuple<T>{x};
  } else {
    return std::tuple<>{};
  }
}

// Collect all GraphNode parents from distribution arguments.
// Used by factories to build the parent tuple for DAG edges.
template <typename... Args>
constexpr auto collectParents(const Args&... args) {
  return std::tuple_cat(nodeAsParentTuple(args)...);
}

// =============================================================================
// diff() - Differentiate w.r.t. GraphNode
// =============================================================================
//
// Dispatch to symbolic3::diff using the node's symbol. Works uniformly
// for both RandomVar and DeterministicVar.

template <Symbolic Expr, GraphNode Node>
constexpr auto diff(Expr expr, const Node& node) {
  return symbolic3::diff(expr, node.sym());
}

}  // namespace tempura::bayes2
