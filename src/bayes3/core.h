#pragma once

#include <concepts>
#include <cstddef>
#include <random>
#include <tuple>
#include <type_traits>
#include <utility>

#include "prob/beta.h"
#include "prob/exponential.h"
#include "prob/gamma.h"
#include "prob/half_normal.h"
#include "prob/normal.h"
#include "prob/student_t.h"
#include "prob/uniform.h"
#include "symbolic3/core.h"
#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/operators.h"

// bayes3: Symbolic probabilistic programming DSL
//
// Distributions are expression-like types: Dist<DistTemplate, Param1, Param2>
// Parameters are always symbolic (scalars auto-convert to Literal).
// DAG structure is encoded in types via parent tracking.
//
// Supports both:
//   - Symbolic mode: jointUnnormalizedLogProb() returns symbolic expression for autodiff
//   - Numeric mode: unnormalizedLogProb() evaluates with trace to double
//
// Usage:
//   auto mu = normal(0.0, 1.0);
//   auto y = normal(mu, 2.0);
//
//   // Symbolic: get differentiable expression
//   auto lp = jointUnnormalizedLogProb(y);
//   auto grad = diff(lp, mu);
//
//   // Numeric: sample and evaluate
//   auto trace = sampleTrace(y, rng);
//   double lp_val = unnormalizedLogProb(y, trace);

namespace tempura::bayes3 {

using namespace tempura::symbolic3;

// =============================================================================
// Type Traits
// =============================================================================

template <typename T>
constexpr bool is_random_var = false;

template <typename T>
constexpr bool is_deterministic_var = false;

// =============================================================================
// GraphNode Concept - any node in the probabilistic DAG
// =============================================================================

template <typename T>
concept GraphNode = requires(const T& node) {
  typename T::id_type;
  typename T::symbol_type;
  { T::sym() } -> Symbolic;
  { node.nodeUnnormalizedLogProb() } -> Symbolic;
  { node.parents() };
  { node = std::declval<double>() };
};

// Legacy alias for compatibility
template <typename T>
concept Distribution = GraphNode<T>;

// =============================================================================
// toSymbolic: Convert value to symbolic form
// =============================================================================

template <typename T>
constexpr auto toSymbolic(const T& x) {
  if constexpr (GraphNode<T>) {
    return T::sym();
  } else if constexpr (Symbolic<T>) {
    return x;
  } else {
    return Literal{static_cast<double>(x)};
  }
}

// =============================================================================
// Parent Collection Helpers
// =============================================================================

template <typename T>
constexpr auto nodeAsParentTuple(const T& x) {
  if constexpr (GraphNode<T>) {
    return std::tuple<T>{x};
  } else {
    return std::tuple<>{};
  }
}

template <typename... Args>
constexpr auto collectParents(const Args&... args) {
  return std::tuple_cat(nodeAsParentTuple(args)...);
}

// =============================================================================
// LogProbFormula - Traits providing symbolic log-prob formulas
// =============================================================================
//
// Each specialization defines the unnormalized log-probability formula
// matching prob::*::unnormalizedLogProb(). Symbolic constants use Literal.

template <template <typename...> class D>
struct LogProbFormula;

template <>
struct LogProbFormula<prob::Normal> {
  // -½z² where z = (x-μ)/σ
  template <Symbolic X, Symbolic Mu, Symbolic Sigma>
  static constexpr auto apply(X x, Mu μ, Sigma σ) {
    auto z = (x - μ) / σ;
    return Literal{-0.5} * z * z;
  }
};

template <>
struct LogProbFormula<prob::HalfNormal> {
  // -½z² where z = x/σ
  template <Symbolic X, Symbolic Sigma>
  static constexpr auto apply(X x, Sigma σ) {
    auto z = x / σ;
    return Literal{-0.5} * z * z;
  }
};

template <>
struct LogProbFormula<prob::Exponential> {
  // -λx
  template <Symbolic X, Symbolic Lambda>
  static constexpr auto apply(X x, Lambda λ) {
    return -λ * x;
  }
};

template <>
struct LogProbFormula<prob::Gamma> {
  // (α-1)log(x) - βx
  template <Symbolic X, Symbolic Alpha, Symbolic Beta>
  static constexpr auto apply(X x, Alpha α, Beta β) {
    return (α - Literal{1.0}) * log(x) - β * x;
  }
};

template <>
struct LogProbFormula<prob::Beta> {
  // (α-1)log(x) + (β-1)log(1-x)
  template <Symbolic X, Symbolic Alpha, Symbolic Beta>
  static constexpr auto apply(X x, Alpha α, Beta β) {
    return (α - Literal{1.0}) * log(x) + (β - Literal{1.0}) * log(Literal{1.0} - x);
  }
};

template <>
struct LogProbFormula<prob::Uniform> {
  // 0 (constant density)
  template <Symbolic X, Symbolic Lower, Symbolic Upper>
  static constexpr auto apply(X /*x*/, Lower /*a*/, Upper /*b*/) {
    return Literal{0.0};
  }
};

template <>
struct LogProbFormula<prob::StudentT> {
  // -((ν+1)/2)·log(1 + x²/ν)
  template <Symbolic X, Symbolic Nu>
  static constexpr auto apply(X x, Nu ν) {
    return -(ν + Literal{1.0}) / Literal{2.0} * log(Literal{1.0} + x * x / ν);
  }
};

// =============================================================================
// Dist - Generic distribution wrapper with symbolic parameters
// =============================================================================

template <template <typename...> class D, Symbolic... Params>
struct Dist {
  [[no_unique_address]] std::tuple<Params...> params_;

  constexpr Dist(Params... ps) : params_{ps...} {}
  constexpr const auto& params() const { return params_; }

  template <Symbolic X>
  constexpr auto unnormalizedLogProb(X x) const {
    return std::apply(
        [&](auto... ps) { return LogProbFormula<D>::apply(x, ps...); }, params_);
  }
};

// =============================================================================
// RandomVar - Stochastic node with distribution and parent dependencies
// =============================================================================

template <typename Id, typename DistT, typename... Parents>
struct RandomVar {
  using id_type = Id;
  using dist_type = DistT;
  using symbol_type = Symbol<Id>;

  [[no_unique_address]] DistT dist_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  static constexpr auto sym() { return symbol_type{}; }
  constexpr const auto& parents() const { return parents_; }
  constexpr const auto& params() const { return dist_.params(); }

  // Symbolic log-probability contribution: log p(this | parents)
  constexpr auto nodeUnnormalizedLogProb() const { return dist_.unnormalizedLogProb(sym()); }

  // Binding syntax: mu = 1.0 creates Symbol<Id>=1.0 binder
  constexpr auto operator=(auto value) const { return sym() = value; }
};

template <typename Id, typename DistT, typename... Parents>
constexpr bool is_random_var<RandomVar<Id, DistT, Parents...>> = true;

// =============================================================================
// DeterministicVar - Computed node (not sampled)
// =============================================================================
//
// Created by arithmetic on GraphNodes. Contributes 0 to log-probability.

template <typename Id, typename Expr, typename... Parents>
struct DeterministicVar {
  using id_type = Id;
  using expr_type = Expr;
  using symbol_type = Symbol<Id>;

  [[no_unique_address]] Expr expr_;
  [[no_unique_address]] std::tuple<Parents...> parents_;

  static constexpr auto sym() { return symbol_type{}; }
  constexpr const auto& parents() const { return parents_; }
  constexpr auto expr() const { return expr_; }

  // Deterministic nodes contribute 0 to log-prob (they're derived quantities)
  constexpr auto nodeUnnormalizedLogProb() const { return Literal{0.0}; }

  constexpr auto operator=(auto value) const { return sym() = value; }
};

template <typename Id, typename Expr, typename... Parents>
constexpr bool is_deterministic_var<DeterministicVar<Id, Expr, Parents...>> =
    true;

// =============================================================================
// Factory Functions - Create RandomVars with unique identities per call site
// =============================================================================

template <typename Id, typename Dist, typename... Args>
constexpr auto makeRandomVar(Dist dist, const Args&... args) {
  return std::apply(
      [&]<typename... Ps>(const Ps&... ps) {
        return RandomVar<Id, Dist, Ps...>{dist, std::tuple{ps...}};
      },
      collectParents(args...));
}

template <typename Id = decltype([] {}), typename Mu, typename Sigma>
constexpr auto normal(const Mu& mu, const Sigma& sigma) {
  using DistType =
      Dist<prob::Normal, decltype(toSymbolic(mu)), decltype(toSymbolic(sigma))>;
  return makeRandomVar<Id>(DistType{toSymbolic(mu), toSymbolic(sigma)}, mu,
                           sigma);
}

template <typename Id = decltype([] {}), typename Sigma>
constexpr auto halfNormal(const Sigma& sigma) {
  using DistType = Dist<prob::HalfNormal, decltype(toSymbolic(sigma))>;
  return makeRandomVar<Id>(DistType{toSymbolic(sigma)}, sigma);
}

template <typename Id = decltype([] {}), typename Lambda>
constexpr auto exponential(const Lambda& lambda) {
  using DistType = Dist<prob::Exponential, decltype(toSymbolic(lambda))>;
  return makeRandomVar<Id>(DistType{toSymbolic(lambda)}, lambda);
}

template <typename Id = decltype([] {}), typename Shape, typename Rate>
constexpr auto gamma(const Shape& shape, const Rate& rate) {
  using DistType =
      Dist<prob::Gamma, decltype(toSymbolic(shape)), decltype(toSymbolic(rate))>;
  return makeRandomVar<Id>(DistType{toSymbolic(shape), toSymbolic(rate)}, shape,
                           rate);
}

template <typename Id = decltype([] {}), typename Alpha, typename Beta>
constexpr auto beta(const Alpha& alpha, const Beta& b) {
  using DistType =
      Dist<prob::Beta, decltype(toSymbolic(alpha)), decltype(toSymbolic(b))>;
  return makeRandomVar<Id>(DistType{toSymbolic(alpha), toSymbolic(b)}, alpha,
                           b);
}

template <typename Id = decltype([] {}), typename Lower, typename Upper>
constexpr auto uniform(const Lower& lower, const Upper& upper) {
  using DistType = Dist<prob::Uniform, decltype(toSymbolic(lower)),
                        decltype(toSymbolic(upper))>;
  return makeRandomVar<Id>(DistType{toSymbolic(lower), toSymbolic(upper)},
                           lower, upper);
}

template <typename Id = decltype([] {}), typename Nu>
constexpr auto studentT(const Nu& nu) {
  using DistType = Dist<prob::StudentT, decltype(toSymbolic(nu))>;
  return makeRandomVar<Id>(DistType{toSymbolic(nu)}, nu);
}

// =============================================================================
// DeterministicVar Factory
// =============================================================================

template <typename Expr, typename... Parents>
constexpr auto makeDeterministicVar(Expr expr, std::tuple<Parents...> parents) {
  constexpr auto id = [] {};
  using Id = decltype(id);
  return DeterministicVar<Id, Expr, Parents...>{expr, parents};
}

// =============================================================================
// Arithmetic Operators for GraphNodes
// =============================================================================
//
// Arithmetic on graph nodes produces DeterministicVar with symbolic expression.

// Binary operators: GraphNode OP GraphNode
template <GraphNode L, GraphNode R>
constexpr auto operator+(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() + rhs.sym(), std::tuple{lhs, rhs});
}

template <GraphNode L, GraphNode R>
constexpr auto operator-(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() - rhs.sym(), std::tuple{lhs, rhs});
}

template <GraphNode L, GraphNode R>
constexpr auto operator*(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() * rhs.sym(), std::tuple{lhs, rhs});
}

template <GraphNode L, GraphNode R>
constexpr auto operator/(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() / rhs.sym(), std::tuple{lhs, rhs});
}

// Binary operators: GraphNode OP scalar
template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() + Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} + node.sym(),
                              std::tuple{node});
}

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() - Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} - node.sym(),
                              std::tuple{node});
}

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() * Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} * node.sym(),
                              std::tuple{node});
}

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() / Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} / node.sym(),
                              std::tuple{node});
}

// Unary operators
template <GraphNode G>
constexpr auto operator-(const G& node) {
  return makeDeterministicVar(Literal{0.0} - node.sym(), std::tuple{node});
}

template <GraphNode G>
constexpr auto operator+(const G& node) {
  return makeDeterministicVar(node.sym(), std::tuple{node});
}

// =============================================================================
// IdSet - Type-level set for deduplication during traversal
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
// Trace - Holds sampled values, queryable by node
// =============================================================================

template <typename... Binders>
struct Trace;

template <>
struct Trace<> : BinderPack<> {
  using BinderPack<>::BinderPack;
};

template <typename First, typename... Rest>
struct Trace<First, Rest...> : BinderPack<First, Rest...> {
  using BinderPack<First, Rest...>::BinderPack;
  using BinderPack<First, Rest...>::operator[];

  template <typename Id, typename Dist, typename... Parents>
  constexpr auto operator[](const RandomVar<Id, Dist, Parents...>& rv) const {
    return BinderPack<First, Rest...>::operator[](rv.sym());
  }

  template <typename... Nodes>
  constexpr auto get(const Nodes&... nodes) const {
    return std::tuple{(*this)[nodes]...};
  }
};

template <typename... Binders>
Trace(Binders...) -> Trace<Binders...>;

template <typename... Bs1, typename... Bs2>
constexpr auto mergeTraces(const Trace<Bs1...>& t1, const Trace<Bs2...>& t2) {
  return Trace<Bs1..., Bs2...>{static_cast<const Bs1&>(t1)...,
                               static_cast<const Bs2&>(t2)...};
}

// =============================================================================
// Sampling from distributions with bindings (generic for all Dist types)
// =============================================================================

template <template <typename...> class D, Symbolic... Params, typename... Binders,
          typename Generator>
auto sampleDist(const Dist<D, Params...>& dist,
                const BinderPack<Binders...>& bindings, Generator& rng) {
  return std::apply(
      [&](const auto&... params) {
        return D{evaluate(params, bindings)...}.sample(rng);
      },
      dist.params());
}

// =============================================================================
// Log prob from distributions with bindings (generic for all Dist types)
// =============================================================================

template <template <typename...> class D, Symbolic... Params, typename... Binders>
auto logProbDist(const Dist<D, Params...>& dist, double value,
                 const BinderPack<Binders...>& bindings) {
  return std::apply(
      [&](const auto&... params) {
        return D{evaluate(params, bindings)...}.unnormalizedLogProb(value);
      },
      dist.params());
}

// =============================================================================
// sampleTrace - DAG traversal for sampling
// =============================================================================

namespace detail {

template <typename Node, typename Visited, typename TraceAcc, typename Generator>
struct SampleTraverse;

template <std::size_t I, typename Parents, typename V, typename TraceAcc,
          typename Generator>
auto foldSampleParents(const Parents& parents, V visited, TraceAcc trace,
                       Generator& rng) {
  if constexpr (I >= std::tuple_size_v<Parents>) {
    return std::pair{trace, visited};
  } else {
    const auto& parent = std::get<I>(parents);
    using Parent = std::remove_cvref_t<decltype(parent)>;
    auto [trace1, visited1] =
        SampleTraverse<Parent, V, TraceAcc, Generator>::apply(parent, visited,
                                                              trace, rng);
    return foldSampleParents<I + 1>(parents, visited1, trace1, rng);
  }
}

template <typename Node, typename Visited, typename TraceAcc, typename Generator>
struct SampleTraverse {
  using Id = typename Node::id_type;
  static constexpr bool skip = Visited::template contains<Id>;
  using NewVisited = typename Visited::template insert<Id>;

  static auto apply(const Node& node, Visited, TraceAcc trace, Generator& rng) {
    if constexpr (skip) {
      return std::pair{trace, Visited{}};
    } else {
      auto [parent_trace, after_parents] =
          foldSampleParents<0>(node.parents(), NewVisited{}, trace, rng);

      auto value = sampleDist(node.dist_, parent_trace, rng);
      return std::pair{mergeTraces(parent_trace, Trace{node.sym() = value}),
                       after_parents};
    }
  }
};

}  // namespace detail

template <typename Node, typename Generator>
  requires Distribution<Node>
auto sampleTrace(const Node& node, Generator& rng) {
  return detail::SampleTraverse<Node, IdSet<>, Trace<>, Generator>::apply(
             node, IdSet<>{}, Trace{}, rng)
      .first;
}

// =============================================================================
// jointUnnormalizedLogProb - Symbolic DAG traversal returning expression
// =============================================================================
//
// Traverses the DAG and returns a symbolic expression representing the joint
// log-probability. This can be differentiated symbolically for autodiff.

namespace detail {

template <typename Node, typename Visited>
struct SymbolicTraverse;

template <std::size_t I, typename Parents, typename V, typename Acc>
constexpr auto foldSymbolicParents(const Parents& parents, V visited, Acc acc) {
  if constexpr (I >= std::tuple_size_v<Parents>) {
    return std::pair{acc, visited};
  } else {
    const auto& parent = std::get<I>(parents);
    using Parent = std::remove_cvref_t<decltype(parent)>;
    auto [lp, new_visited] = SymbolicTraverse<Parent, V>::apply(parent, visited);
    return foldSymbolicParents<I + 1>(parents, new_visited, acc + lp);
  }
}

template <typename... Ps, typename V>
constexpr auto traverseSymbolicParents(const std::tuple<Ps...>& parents,
                                       V visited) {
  return foldSymbolicParents<0>(parents, visited, Literal{0.0});
}

template <typename V>
constexpr auto traverseSymbolicParents(const std::tuple<>& /*parents*/,
                                       V visited) {
  return std::pair{Literal{0.0}, visited};
}

template <typename Node, typename Visited>
struct SymbolicTraverse {
  static constexpr bool skip =
      Visited::template contains<typename Node::id_type>;
  using NewVisited = typename Visited::template insert<typename Node::id_type>;

  static constexpr auto apply(const Node& node, Visited visited) {
    if constexpr (skip) {
      return std::pair{Literal{0.0}, visited};
    } else {
      auto [parent_lp, after_parents] =
          traverseSymbolicParents(node.parents(), NewVisited{});
      auto total = parent_lp + node.nodeUnnormalizedLogProb();
      return std::pair{total, after_parents};
    }
  }
};

}  // namespace detail

// Single root: traverse DAG from one node
template <GraphNode Node>
constexpr auto jointUnnormalizedLogProb(const Node& root) {
  return detail::SymbolicTraverse<Node, IdSet<>>::apply(root, IdSet<>{}).first;
}

// Multiple roots: traverse DAG from multiple nodes (e.g., multiple observations)
template <typename Visited, typename Acc>
constexpr auto jointUnnormalizedLogProbMulti(Visited /*visited*/, Acc acc) {
  return acc;
}

template <typename Visited, typename Acc, GraphNode Node, typename... Rest>
constexpr auto jointUnnormalizedLogProbMulti(Visited visited, Acc acc, const Node& node,
                                 const Rest&... rest) {
  auto [lp, new_visited] =
      detail::SymbolicTraverse<Node, Visited>::apply(node, visited);
  return jointUnnormalizedLogProbMulti(new_visited, acc + lp, rest...);
}

template <GraphNode Node, GraphNode... Nodes>
  requires(sizeof...(Nodes) > 0)
constexpr auto jointUnnormalizedLogProb(const Node& first, const Nodes&... rest) {
  return jointUnnormalizedLogProbMulti(IdSet<>{}, Literal{0.0}, first, rest...);
}

// =============================================================================
// diff - Differentiate expression w.r.t. GraphNode
// =============================================================================

template <Symbolic Expr, GraphNode Node>
constexpr auto diff(Expr expr, const Node& node) {
  return symbolic3::diff(expr, node.sym());
}

// =============================================================================
// unnormalizedLogProb - Compute joint log prob from trace (numeric)
// =============================================================================

namespace detail {

template <typename Node, typename Visited, typename TraceT>
struct LogProbTraverse;

template <std::size_t I, typename Parents, typename V, typename TraceT>
auto foldLogProbParents(const Parents& parents, V visited, const TraceT& trace,
                        double acc) {
  if constexpr (I >= std::tuple_size_v<Parents>) {
    return std::pair{acc, visited};
  } else {
    const auto& parent = std::get<I>(parents);
    using Parent = std::remove_cvref_t<decltype(parent)>;
    auto [lp, new_visited] =
        LogProbTraverse<Parent, V, TraceT>::apply(parent, visited, trace);
    return foldLogProbParents<I + 1>(parents, new_visited, trace, acc + lp);
  }
}

template <typename Node, typename Visited, typename TraceT>
struct LogProbTraverse {
  using Id = typename Node::id_type;
  static constexpr bool skip = Visited::template contains<Id>;
  using NewVisited = typename Visited::template insert<Id>;

  static auto apply(const Node& node, Visited, const TraceT& trace) {
    if constexpr (skip) {
      return std::pair{0.0, Visited{}};
    } else {
      auto [parent_lp, after_parents] =
          foldLogProbParents<0>(node.parents(), NewVisited{}, trace, 0.0);

      double value = trace[node.sym()];
      double node_lp = logProbDist(node.dist_, value, trace);
      return std::pair{parent_lp + node_lp, after_parents};
    }
  }
};

}  // namespace detail

template <typename Node, typename... Binders>
  requires Distribution<Node>
auto unnormalizedLogProb(const Node& node, const Trace<Binders...>& trace) {
  return detail::LogProbTraverse<Node, IdSet<>, Trace<Binders...>>::apply(
             node, IdSet<>{}, trace)
      .first;
}

}  // namespace tempura::bayes3
