#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "matrix3/matrix.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/symbolic_state.h"

// ============================================================================
// samples.h - Unified MCMC samples container
// ============================================================================
//
// Samples<SymsTuple, SpecsTuple> stores MCMC draws in a flat DynamicDense matrix
// (draws × state_dim) with symbol-indexed access. Handles both scalar and indexed
// parameters uniformly — TransformPack treats size=1 and size=N the same way.
//
// Usage:
//   auto samples = posterior.sample(config, init, rng);
//
//   // Access all draws for one parameter
//   auto alpha_draws = samples[alpha];     // vector<double> (scalar)
//   auto theta_draws = samples[theta];     // DynamicDense (indexed: draws × groups)
//
//   // Evaluate derived expressions across draws
//   auto logit_p = samples[logistic(a + sigma * z)];
//
//   // Access single draw as SymbolicState
//   auto draw = samples[0];
//   double a_val = draw[alpha];
//
//   // Iteration
//   for (const auto& draw : samples) {
//     double a_val = draw[alpha];
//   }
//
// ============================================================================

namespace tempura::symbolic4 {

namespace samples_detail {

// Get the symbol type used in the samples tuple for a parameter
template <typename P>
using ParamSymbolType = typename P::symbol_type;

}  // namespace samples_detail

template <typename SymsTuple, typename SpecsTuple>
class Samples {
 public:
  using SymbolsTuple = SymsTuple;
  static constexpr std::size_t NumSlots = std::tuple_size_v<SymbolsTuple>;

  Samples(SymbolsTuple symbols, SpecsTuple specs, std::size_t state_dim)
      : symbols_{symbols}, specs_{specs}, state_dim_{state_dim}, data_{0, state_dim} {
    computeOffsets();
  }

  void push_back(std::span<const double> draw) {
    assert(draw.size() == state_dim_);
    data_.pushRow(draw);
  }

  void reserve(std::size_t num_draws) { data_.reserveRows(num_draws); }

  auto size() const -> std::size_t { return data_.rows(); }

  auto empty() const -> bool { return data_.rows() == 0; }

  // Access all values for a scalar parameter (returns vector<double>)
  template <typename P>
    requires(IsRandomVar<P> && !IsIndexedRandomVar<P>)
  auto operator[](const P& /*p*/) const -> std::vector<double> {
    using SymType = typename P::symbol_type;
    constexpr std::size_t idx = symbolic_state_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in Samples");

    std::size_t offset = offsets_[idx];
    std::vector<double> result;
    result.reserve(data_.rows());
    for (std::size_t i = 0; i < data_.rows(); ++i) {
      result.push_back(data_[i, offset]);
    }
    return result;
  }

  // Access all values for an indexed parameter (returns DynamicDense matrix)
  template <typename P>
    requires IsIndexedRandomVar<P>
  auto operator[](const P& /*p*/) const -> matrix3::DynamicDense<double> {
    using SymType = typename P::symbol_type;
    constexpr std::size_t idx = symbolic_state_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in Samples");

    std::size_t offset = offsets_[idx];
    std::size_t param_size = std::get<idx>(specs_).size();
    std::size_t num_draws = data_.rows();

    matrix3::DynamicDense<double> result(num_draws, param_size);
    for (std::size_t i = 0; i < num_draws; ++i) {
      for (std::size_t j = 0; j < param_size; ++j) {
        result[i, j] = data_[i, offset + j];
      }
    }
    return result;
  }

  // Evaluate a symbolic expression across all draws.
  // Binds all params to each draw's values, then evaluates with evaluateIndexed
  // to support expressions containing ReduceOver.
  template <Symbolic E>
  auto operator[](E expr) const -> std::vector<double> {
    std::vector<double> result;
    result.reserve(data_.rows());
    for (std::size_t i = 0; i < data_.rows(); ++i) {
      auto bindings = drawBindings(i, std::make_index_sequence<NumSlots>{});
      result.push_back(evaluateIndexed(expr, bindings));
    }
    return result;
  }

  // Access a single draw as a SymbolicState (queryable by symbol)
  using StateType = SymbolicState<SymbolsTuple, SpecsTuple>;

  auto operator[](std::size_t i) const -> StateType {
    assert(i < data_.rows());
    std::vector<double> row(state_dim_);
    for (std::size_t j = 0; j < state_dim_; ++j) {
      row[j] = data_[i, j];
    }
    return StateType{symbols_, specs_, std::move(row)};
  }

  // Raw access to underlying storage
  auto data() const -> const matrix3::DynamicDense<double>& { return data_; }
  auto data() -> matrix3::DynamicDense<double>& { return data_; }

  // Get state dimension
  auto stateDim() const -> std::size_t { return state_dim_; }

  // Get offset for a parameter
  template <typename P>
  auto offset(const P& /*p*/) const -> std::size_t {
    using SymType = samples_detail::ParamSymbolType<P>;
    constexpr std::size_t idx = symbolic_state_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in Samples");
    return offsets_[idx];
  }

  // Get size for a parameter
  template <typename P>
  auto paramSize(const P& /*p*/) const -> std::size_t {
    using SymType = samples_detail::ParamSymbolType<P>;
    constexpr std::size_t idx = symbolic_state_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in Samples");
    return std::get<idx>(specs_).size();
  }

  // -----------------------------------------------------------------------
  // Iterator support — yields SymbolicState per draw
  // -----------------------------------------------------------------------

  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = StateType;
    using pointer = void;
    using reference = StateType;  // Returns by value (proxy)

    Iterator() = default;
    Iterator(const Samples* samples, std::size_t row)
        : samples_{samples}, row_{row} {}

    auto operator*() const -> StateType { return (*samples_)[row_]; }

    auto operator++() -> Iterator& { ++row_; return *this; }
    auto operator++(int) -> Iterator { auto tmp = *this; ++row_; return tmp; }

    auto operator==(const Iterator& other) const -> bool { return row_ == other.row_; }
    auto operator!=(const Iterator& other) const -> bool { return row_ != other.row_; }

   private:
    const Samples* samples_ = nullptr;
    std::size_t row_ = 0;
  };

  auto begin() const -> Iterator { return Iterator{this, 0}; }
  auto end() const -> Iterator { return Iterator{this, data_.rows()}; }

 private:
  SymbolsTuple symbols_;
  SpecsTuple specs_;
  std::size_t state_dim_;
  matrix3::DynamicDense<double> data_;  // (num_draws × state_dim)
  std::array<std::size_t, NumSlots> offsets_;

  void computeOffsets() {
    computeOffsetsImpl(std::make_index_sequence<NumSlots>{});
  }

  template <std::size_t... Is>
  void computeOffsetsImpl(std::index_sequence<Is...>) {
    std::size_t running = 0;
    ((offsets_[Is] = running, running += std::get<Is>(specs_).size()), ...);
  }

  // Build bindings for draw i: scalar params get sym = val,
  // indexed params get IndexedBinding<Sym, 1>{span into row buffer}.
  template <std::size_t... Is>
  auto drawBindings(std::size_t i, std::index_sequence<Is...>) const {
    row_buf_.resize(state_dim_);
    for (std::size_t j = 0; j < state_dim_; ++j) {
      row_buf_[j] = data_[i, j];
    }
    auto binding_tuple = std::tuple_cat(bindSlot<Is>()...);
    return tupleToBinderPack(binding_tuple);
  }

  template <std::size_t I>
  auto bindSlot() const {
    using SpecType = std::decay_t<std::tuple_element_t<I, SpecsTuple>>;
    using SymType = std::decay_t<std::tuple_element_t<I, SymbolsTuple>>;
    if constexpr (!SpecType::is_indexed) {
      return std::make_tuple(SymType{} = row_buf_[offsets_[I]]);
    } else {
      std::size_t n = std::get<I>(specs_).size();
      return std::make_tuple(
          IndexedBinding<SymType, 1>{std::span<const double>(row_buf_.data() + offsets_[I], n)});
    }
  }

  mutable std::vector<double> row_buf_;
};

// Backward compatibility alias
template <typename SymsTuple, typename SpecsTuple>
using DynamicSamples = Samples<SymsTuple, SpecsTuple>;

// Factory function
template <typename SymbolsTuple, typename SpecsTuple>
auto makeSamples(SymbolsTuple symbols, SpecsTuple specs, std::size_t state_dim) {
  return Samples<SymbolsTuple, SpecsTuple>{symbols, specs, state_dim};
}

template <typename SymbolsTuple, typename SpecsTuple>
auto makeDynamicSamples(SymbolsTuple symbols, SpecsTuple specs, std::size_t state_dim) {
  return makeSamples(symbols, specs, state_dim);
}

// ============================================================================
// Statistics helpers - operate on vector<double> from samples[param]
// ============================================================================

inline auto mean(const std::vector<double>& values) -> double {
  if (values.empty()) return 0.0;
  return std::accumulate(values.begin(), values.end(), 0.0) /
         static_cast<double>(values.size());
}

inline auto sd(const std::vector<double>& values) -> double {
  if (values.size() < 2) return 0.0;
  double m = mean(values);
  double sum_sq = 0.0;
  for (double v : values) {
    sum_sq += (v - m) * (v - m);
  }
  return std::sqrt(sum_sq / static_cast<double>(values.size()));
}

inline auto variance(const std::vector<double>& values) -> double {
  if (values.size() < 2) return 0.0;
  double m = mean(values);
  double sum_sq = 0.0;
  for (double v : values) {
    sum_sq += (v - m) * (v - m);
  }
  return sum_sq / static_cast<double>(values.size());
}

// Quantile (0.0 to 1.0)
inline auto quantile(std::vector<double> values, double p) -> double {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  double idx = p * static_cast<double>(values.size() - 1);
  std::size_t lo = static_cast<std::size_t>(std::floor(idx));
  std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
  if (lo == hi) return values[lo];
  double frac = idx - static_cast<double>(lo);
  return values[lo] * (1.0 - frac) + values[hi] * frac;
}

inline auto median(const std::vector<double>& values) -> double {
  return quantile(values, 0.5);
}

}  // namespace tempura::symbolic4
