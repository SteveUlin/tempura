#pragma once

#include <random>

#include "bayes/beta.h"
#include "bayes/bernoulli.h"
#include "bayes/cauchy.h"
#include "bayes/exponential.h"
#include "bayes/gamma.h"
#include "bayes/normal.h"
#include "bayes/uniform.h"
#include "bayes2/distributions.h"
#include "bayes2/traversal.h"
#include "symbolic3/evaluate.h"

// Sampling from bayes2 distributions.
//
// Since distribution parameters are symbolic (may depend on other RandomVars),
// sampling requires bindings to evaluate parameters to concrete values first.
//
// Usage:
//   auto mu = normal(0.0, 10.0);
//   auto sigma = halfNormal(5.0);
//   auto y = normal(mu, sigma);
//
//   std::mt19937 rng{42};
//   auto bindings = BinderPack{mu = 2.0, sigma = 1.5};
//   double y_sample = sample(y, bindings, rng);

namespace tempura::bayes2 {

using namespace tempura::symbolic3;

// =============================================================================
// Sample from Distribution Wrappers
// =============================================================================
//
// Each overload evaluates symbolic parameters using bindings, creates a
// concrete distribution, and samples from it.

template <Symbolic Mu, Symbolic Sigma, typename... Binders, typename Generator>
auto sample(const NormalDist<Mu, Sigma>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double mu = evaluate(dist.mu_, bindings);
  double sigma = evaluate(dist.sigma_, bindings);
  return bayes::Normal<double>{mu, sigma}.sample(rng);
}

template <Symbolic Sigma, typename... Binders, typename Generator>
auto sample(const HalfNormalDist<Sigma>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double sigma = evaluate(dist.sigma_, bindings);
  // HalfNormal is |Normal(0, sigma)|
  return std::abs(bayes::Normal<double>{0.0, sigma}.sample(rng));
}

template <Symbolic Lambda, typename... Binders, typename Generator>
auto sample(const ExponentialDist<Lambda>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double lambda = evaluate(dist.lambda_, bindings);
  return bayes::Exponential<double>{lambda}.sample(rng);
}

template <Symbolic Alpha, Symbolic Beta, typename... Binders, typename Generator>
auto sample(const BetaDist<Alpha, Beta>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double alpha = evaluate(dist.alpha_, bindings);
  double beta = evaluate(dist.beta_, bindings);
  return bayes::Beta<double>{alpha, beta}.sample(rng);
}

template <Symbolic Shape, Symbolic Rate, typename... Binders, typename Generator>
auto sample(const GammaDist<Shape, Rate>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double shape = evaluate(dist.shape_, bindings);
  double rate = evaluate(dist.rate_, bindings);
  return bayes::Gamma<double>{shape, rate}.sample(rng);
}

template <Symbolic Lo, Symbolic Hi, typename... Binders, typename Generator>
auto sample(const UniformDist<Lo, Hi>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double lo = evaluate(dist.lo_, bindings);
  double hi = evaluate(dist.hi_, bindings);
  return bayes::Uniform<double>{lo, hi}.sample(rng);
}

template <Symbolic Location, Symbolic Scale, typename... Binders, typename Generator>
auto sample(const CauchyDist<Location, Scale>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> double {
  double location = evaluate(dist.location_, bindings);
  double scale = evaluate(dist.scale_, bindings);
  return bayes::Cauchy<double>{location, scale}.sample(rng);
}

template <Symbolic P, typename... Binders, typename Generator>
auto sample(const BernoulliDist<P>& dist, const BinderPack<Binders...>& bindings,
            Generator& rng) -> bool {
  double p = evaluate(dist.p_, bindings);
  return bayes::Bernoulli<double>{p}.sample(rng);
}

// =============================================================================
// Sample from RandomVar
// =============================================================================
//
// Sample from a RandomVar by delegating to its distribution's sample method.
// The bindings must provide values for all parent RandomVars.

template <typename Id, typename Dist, typename... Parents, typename... Binders,
          typename Generator>
auto sample(const RandomVar<Id, Dist, Parents...>& rv,
            const BinderPack<Binders...>& bindings, Generator& rng) {
  return sample(rv.dist_, bindings, rng);
}

// =============================================================================
// Prior Predictive Sampling
// =============================================================================
//
// Sample from the prior by first sampling all ancestors, then sampling this node.
// Returns the sampled value for this node.
//
// Strategy: recursively sample each parent (which handles its own ancestry),
// then create bindings just for direct parents and sample this node.

namespace detail {

// Sample parents and create bindings tuple using index sequence
template <typename Generator, std::size_t... Is, typename... Parents>
auto sampleParentsToBindings(const std::tuple<Parents...>& parents, Generator& rng,
                              std::index_sequence<Is...>) {
  // Sample each parent recursively (handles their own parent dependencies)
  // and create binders for direct parents only
  return BinderPack{(std::get<Is>(parents) = samplePrior(std::get<Is>(parents), rng))...};
}

}  // namespace detail

template <typename Id, typename Dist, typename Generator>
auto samplePrior(const RandomVar<Id, Dist>& rv, Generator& rng) -> double {
  // Root node (no parents) - sample with empty bindings
  return sample(rv.dist_, BinderPack{}, rng);
}

template <typename Id, typename Dist, typename... Parents, typename Generator>
  requires(sizeof...(Parents) > 0)
auto samplePrior(const RandomVar<Id, Dist, Parents...>& rv, Generator& rng) -> double {
  // Sample all parents and collect their bindings
  auto bindings = detail::sampleParentsToBindings(
      rv.parents(), rng, std::index_sequence_for<Parents...>{});
  return sample(rv.dist_, bindings, rng);
}

// =============================================================================
// Trace - A record of sampled values from the DAG
// =============================================================================
//
// Trace stores sampled values for all nodes in a model, queryable by node.
//
// Usage:
//   auto mu = normal(0.0, 10.0);
//   auto sigma = halfNormal(5.0);
//   auto y = normal(mu, sigma);
//
//   auto trace = sampleTrace(y, rng);
//
//   // Single value lookup
//   double mu_val = trace[mu];
//
//   // Multiple values as tuple (structured bindings)
//   auto [m, s, obs] = trace.get(mu, sigma, y);
//
//   // Use as bindings for evaluate
//   double lp = evaluate(jointLogProb(y), trace);

// Trace is a BinderPack holding sampled values, with node-based lookup.
// Empty specialization needed because BinderPack<> has no operator[] to inherit.
template <typename... Binders>
struct Trace;

// Empty trace - used during fold when no parents have been processed yet
template <>
struct Trace<> : BinderPack<> {
  using BinderPack<>::BinderPack;
};

// Non-empty trace - has operator[] for value lookup
template <typename First, typename... Rest>
struct Trace<First, Rest...> : BinderPack<First, Rest...> {
  using BinderPack<First, Rest...>::BinderPack;
  using BinderPack<First, Rest...>::operator[];

  // Allow lookup by RandomVar (forwards to symbol lookup)
  template <typename Id, typename Dist, typename... Parents>
  constexpr auto operator[](const RandomVar<Id, Dist, Parents...>& rv) const {
    return BinderPack<First, Rest...>::operator[](rv.sym());
  }

  // Multi-parameter query: trace.get(mu, sigma) returns std::tuple
  template <typename... Nodes>
  constexpr auto get(const Nodes&... nodes) const {
    return std::tuple{(*this)[nodes]...};
  }
};

template <typename... Binders>
Trace(Binders...) -> Trace<Binders...>;

// Merge two Traces into one (concatenate binders)
template <typename... Bs1, typename... Bs2>
constexpr auto mergeTraces(const Trace<Bs1...>& t1, const Trace<Bs2...>& t2) {
  return Trace<Bs1..., Bs2...>{static_cast<const Bs1&>(t1)...,
                               static_cast<const Bs2&>(t2)...};
}

// Base case: single trace
template <typename... Bs>
constexpr auto mergeAllTraces(const Trace<Bs...>& t) {
  return t;
}

// Recursive case: merge multiple traces
template <typename... Bs1, typename... Bs2, typename... Rest>
constexpr auto mergeAllTraces(const Trace<Bs1...>& t1, const Trace<Bs2...>& t2,
                              const Rest&... rest) {
  return mergeAllTraces(mergeTraces(t1, t2), rest...);
}

// =============================================================================
// sampleTrace Implementation with Deduplication
// =============================================================================
//
// Uses type-level IdSet (from traversal.h) to ensure each node is sampled
// exactly once, even with diamond dependencies where multiple children share
// a common ancestor. Without deduplication, shared ancestors would be sampled
// multiple times with different values, breaking model semantics.
//
// The implementation mirrors jointLogProb's traversal pattern: thread a
// visited set through recursive calls, skip already-visited nodes.

namespace detail {

// Forward declaration for SampleTraverse
template <typename Node, typename Visited, typename TraceAcc, typename Generator>
struct SampleTraverse;

// Fold over parent tuple, threading both visited set AND accumulated trace.
// The accumulated trace ensures child nodes can access values from ancestors
// that were sampled in sibling branches (diamond dependency case).
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

// Main traversal struct: handles both RandomVar and DeterministicVar.
// Threads accumulated trace to handle diamond dependencies correctly.
template <typename Node, typename Visited, typename TraceAcc, typename Generator>
struct SampleTraverse {
  using Id = typename Node::id_type;
  static constexpr bool skip = Visited::template contains<Id>;
  using NewVisited = typename Visited::template insert<Id>;

  static auto apply(const Node& node, Visited, TraceAcc trace, Generator& rng) {
    if constexpr (skip) {
      // Already sampled - value is in trace from first visit
      return std::pair{trace, Visited{}};
    } else {
      auto [parent_trace, after_parents] =
          foldSampleParents<0>(node.parents(), NewVisited{}, trace, rng);

      if constexpr (is_random_var<Node>) {
        // RandomVar: sample from distribution using accumulated trace
        auto value = sample(node.dist_, parent_trace, rng);
        return std::pair{mergeTraces(parent_trace, Trace{node.sym() = value}),
                         after_parents};
      } else {
        // DeterministicVar: compute deterministic value from expression
        auto value = evaluate(node.expr(), parent_trace);
        return std::pair{mergeTraces(parent_trace, Trace{node.sym() = value}),
                         after_parents};
      }
    }
  }
};

}  // namespace detail

// Public API: sample entire DAG rooted at node, returning Trace of all values
template <typename Node, typename Generator>
  requires GraphNode<Node>
auto sampleTrace(const Node& node, Generator& rng) {
  return detail::SampleTraverse<Node, IdSet<>, Trace<>, Generator>::apply(
             node, IdSet<>{}, Trace{}, rng)
      .first;
}

}  // namespace tempura::bayes2
