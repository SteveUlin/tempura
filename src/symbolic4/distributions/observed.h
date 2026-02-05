#pragma once

#include <span>

#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/indexed/indexed_eval.h"

// ============================================================================
// observed.h - Conditioning on observed data
// ============================================================================
//
// Allows conditioning random variables on observed data. This is essential for
// Bayesian inference where we compute P(params | data) ∝ P(data | params) P(params).
//
// Usage:
//   auto mu = normal(0_c, 10_c);
//   auto y = plate<Obs>(normal(mu, 1.0_c));
//
//   std::vector<double> y_data = {1.0, 2.0, 3.0};
//   auto y_obs = observe(y, y_data);
//
//   // Now logProb(y_obs) gives the log likelihood with y fixed to observed data
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Observed - Marks a random variable as having observed data
// ============================================================================

template <typename Node, typename Data>
struct Observed {
  using node_type = Node;
  using data_type = Data;

  Node node;
  Data data;

  constexpr Observed(Node n, Data d) : node{n}, data{d} {}
};

// Type traits
template <typename T>
constexpr bool is_observed_v = core_traits_detail::isSpecOf<T, Observed>();

// ============================================================================
// observe() - Factory functions
// ============================================================================

// Observe a scalar RandomVar (single data point)
template <IsRandomVar N>
auto observe(const N& node, double data) {
  return Observed<N, double>{node, data};
}

// Observe an IndexedRandomVar with 1D data
template <IsIndexedRandomVar N>
auto observe(const N& node, std::span<const double> data) {
  return Observed<N, IndexedValues>{node, IndexedValues{data}};
}

template <IsIndexedRandomVar N>
auto observe(const N& node, const std::vector<double>& data) {
  return Observed<N, IndexedValues>{node, IndexedValues{data}};
}

// Observe an IndexedRandomVar with multi-dim data
template <IsIndexedRandomVar N, typename... Dims>
auto observe(const N& node, IndexedValuesND<Dims...> data) {
  return Observed<N, IndexedValuesND<Dims...>>{node, data};
}

// ============================================================================
// observedLogProb - Compute log-prob expression for observed data
// ============================================================================

// For scalar observation
template <IsRandomVar N>
auto observedLogProb(const Observed<N, double>& obs) {
  return obs.node.logProb();
}

// For indexed observation
template <IsIndexedRandomVar N, typename Data>
auto observedLogProb(const Observed<N, Data>& obs) {
  return obs.node.logProb();
}

// ============================================================================
// ObservedBinding - Create binding from observed data
// ============================================================================

// For 1D indexed observations
template <IsIndexedRandomVar N>
auto makeObservedBinding(const Observed<N, IndexedValues>& obs) {
  using Sym = typename N::symbol_type;
  return IndexedBinding<Sym>{obs.data.values};
}

// For multi-dim indexed observations
template <IsIndexedRandomVar N, typename... Dims>
auto makeObservedBinding(const Observed<N, IndexedValuesND<Dims...>>& obs) {
  using Sym = typename N::symbol_type;
  return IndexedBinding<Sym, sizeof...(Dims)>{obs.data.data, obs.data.shape.sizes};
}

}  // namespace tempura::symbolic4
