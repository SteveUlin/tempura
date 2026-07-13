#pragma once

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <utility>
#include <vector>

#include "weighted_dag.h"

namespace tempura::autodiff {

// v ← (I−L)⁻¹·v: forward substitution in node order; each node gathers its
// parents through its weights, on top of its seed.
template <typename T>
auto forwardSubstitute(const WeightedDag<T>& dag, std::vector<T> v)
    -> std::vector<T> {
  assert(v.size() == dag.nodeCount() && "vector must cover every node");
  for (std::size_t i = 0; i < dag.nodeCount(); ++i) {
    for (const auto& [dep, weight] : dag.edges(i)) {
      assert(dep < i && "edge must point to an earlier node");
      v[i] += weight * v[dep];
    }
  }
  return v;
}

// v ← (I−L)⁻ᵀ·v: back-substitution in reverse node order; each node scatters
// its value to its parents through its weights.
template <typename T>
auto backwardSubstitute(const WeightedDag<T>& dag, std::vector<T> v)
    -> std::vector<T> {
  assert(v.size() == dag.nodeCount() && "vector must cover every node");
  for (std::size_t i = dag.nodeCount(); i-- > 0;) {
    for (const auto& [dep, weight] : dag.edges(i)) {
      assert(dep < i && "edge must point to an earlier node");
      v[dep] += v[i] * weight;
    }
  }
  return v;
}

// Reverse-mode core (the VJP primitive): AD vocabulary over a public WeightedDag.
// Recording evaluates f once and stores its linearization — edge weight =
// ∂(node)/∂(parent) at the recorded point.
//
// T is generic: Tape<Dual<U>> records dual partials and sweeps them backward —
// forward-over-reverse second order with no second AD system.
template <typename T>
struct Tape {
  WeightedDag<T> dag;

  auto leaf() -> std::size_t { return dag.addNode({}); }

  auto unary(std::size_t a, const T& p) -> std::size_t { return dag.addNode({{a, p}}); }

  auto binary(std::size_t a, std::size_t b, const T& pa, const T& pb) -> std::size_t {
    return dag.addNode({{a, pa}, {b, pb}});
  }

  auto ternary(std::size_t a, std::size_t b, std::size_t c, const T& pa, const T& pb,
               const T& pc) -> std::size_t {
    return dag.addNode({{a, pa}, {b, pb}, {c, pc}});
  }

  // result[n] = ∂(seed·output)/∂n.
  auto backward(std::size_t output, const T& seed = T{1}) const -> std::vector<T> {
    auto adj = std::vector<T>(dag.nodeCount(), T{});
    adj[output] = seed;
    return backwardSubstitute(dag, std::move(adj));
  }

  // result[n] = n's tangent under the seeded leaf tangents: a JVP over the recording.
  auto forward(std::initializer_list<std::pair<std::size_t, T>> seeds) const
      -> std::vector<T> {
    auto tangents = std::vector<T>(dag.nodeCount(), T{});
    for (const auto& [node, tangent] : seeds) tangents[node] = tangent;
    return forwardSubstitute(dag, std::move(tangents));
  }
};

}  // namespace tempura::autodiff
