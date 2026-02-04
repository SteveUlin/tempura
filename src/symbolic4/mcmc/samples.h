#pragma once

#include <algorithm>
#include <array>
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
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/mcmc/state.h"

// ============================================================================
// samples.h - Samples container with symbol-based access
// ============================================================================
//
// Stores MCMC draws and provides type-safe access by parameter symbol.
//
// Usage:
//   Samples<decltype(alpha), decltype(beta)> samples;
//   samples.push_back({1.0, 2.0});
//   samples.push_back({1.1, 2.1});
//
//   // Access all draws for one parameter (for statistics)
//   auto alpha_draws = samples[alpha];  // vector<double>
//   double m = mean(alpha_draws);
//
//   // Access single draw
//   auto draw = samples[0];  // ParameterState
//   double a = draw[alpha];
//
//   // Iteration
//   for (const auto& draw : samples) {
//     std::cout << draw[alpha] << "\n";
//   }
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Samples - Container for MCMC draws with symbol-based access
// ============================================================================

template <typename... Params>
class Samples {
 public:
  static constexpr std::size_t N = sizeof...(Params);
  using StateType = ParameterState<Params...>;
  using ArrayType = std::array<double, N>;

  // Add a draw
  void push_back(const ArrayType& draw) { draws_.push_back(draw); }

  void reserve(std::size_t n) { draws_.reserve(n); }

  auto size() const -> std::size_t { return draws_.size(); }

  auto empty() const -> bool { return draws_.empty(); }

  // Access all values for one parameter (for statistics)
  template <typename P>
    requires IsRandomVar<P>
  auto operator[](const P&) const -> std::vector<double> {
    constexpr std::size_t idx = StateType::template indexOfParam<P>();
    static_assert(idx < N, "Parameter not found in Samples");

    std::vector<double> result;
    result.reserve(draws_.size());
    for (const auto& d : draws_) {
      result.push_back(d[idx]);
    }
    return result;
  }

  // Evaluate a symbolic expression across all draws
  // Usage: samples[alpha + beta] returns vector<double> of per-draw evaluations
  template <Symbolic E>
  auto operator[](E expr) const -> std::vector<double> {
    std::vector<double> result;
    result.reserve(draws_.size());
    for (std::size_t i = 0; i < draws_.size(); ++i) {
      auto bindings = drawBindings(i, std::index_sequence_for<Params...>{});
      result.push_back(evaluate(expr, bindings));
    }
    return result;
  }

  // Access single draw as ParameterState
  auto operator[](std::size_t i) const -> StateType {
    return StateType{draws_[i]};
  }

  // Raw access to underlying storage
  auto draws() const -> const std::vector<ArrayType>& { return draws_; }

  // -------------------------------------------------------------------------
  // Iterator support - iterates over draws as ParameterState
  // -------------------------------------------------------------------------

  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = StateType;
    using pointer = void;  // No pointer type for proxy iterator
    using reference = StateType;  // Returns by value (proxy)

    Iterator() = default;
    explicit Iterator(typename std::vector<ArrayType>::const_iterator it) : it_{it} {}

    auto operator*() const -> StateType { return StateType{*it_}; }

    auto operator++() -> Iterator& {
      ++it_;
      return *this;
    }

    auto operator++(int) -> Iterator {
      Iterator tmp = *this;
      ++it_;
      return tmp;
    }

    auto operator==(const Iterator& other) const -> bool { return it_ == other.it_; }
    auto operator!=(const Iterator& other) const -> bool { return it_ != other.it_; }

   private:
    typename std::vector<ArrayType>::const_iterator it_;
  };

  auto begin() const -> Iterator { return Iterator{draws_.begin()}; }
  auto end() const -> Iterator { return Iterator{draws_.end()}; }

 private:
  std::vector<ArrayType> draws_;

  // Build bindings for draw i: bind each param's Free symbol to the stored
  // constrained value. RandomVar expressions use plain Symbol<Id> atoms,
  // so binding constrained values directly is correct.
  template <std::size_t... Is>
  auto drawBindings(std::size_t i, std::index_sequence<Is...>) const {
    return BinderPack{(typename Params::symbol_type{} = draws_[i][Is])...};
  }
};

// ============================================================================
// DynamicSamples - Samples container for models with indexed latent params
// ============================================================================
//
// For models where parameter dimensions are determined at runtime (plate models),
// this container uses matrix3::DynamicDense for efficient storage.
//
// Usage:
//   DynamicSamples<SymbolsTuple, SpecsTuple> samples(symbols, specs, state_dim);
//   samples.push_back(draw_span);
//
//   // Scalar param: returns vector<double> of all draws
//   auto mu_draws = samples[mu];
//
//   // Indexed param: returns DynamicDense<double> (draws × groups)
//   auto theta_draws = samples[theta];  // matrix: num_draws × group_size
//
// ============================================================================

namespace samples_detail {

// Helper to find symbol index in tuple
template <typename Sym, typename SymsTuple, std::size_t I = 0>
struct SymbolIndex {
  static constexpr std::size_t value = [] {
    if constexpr (I >= std::tuple_size_v<SymsTuple>) {
      return std::size_t(-1);
    } else if constexpr (std::is_same_v<Sym, std::tuple_element_t<I, SymsTuple>>) {
      return I;
    } else {
      return SymbolIndex<Sym, SymsTuple, I + 1>::value;
    }
  }();
};

// Get the symbol type used in the samples tuple for a parameter
// Both scalar and indexed params use symbol_type directly.
template <typename P>
using ParamSymbolType = typename P::symbol_type;

}  // namespace samples_detail

template <typename SymsTuple, typename SpecsTuple>
class DynamicSamples {
 public:
  using SymbolsTuple = SymsTuple;  // Expose for type introspection
  static constexpr std::size_t NumSlots = std::tuple_size_v<SymbolsTuple>;

  DynamicSamples(SymbolsTuple symbols, SpecsTuple specs, std::size_t state_dim)
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
    constexpr std::size_t idx = samples_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in DynamicSamples");

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
    // IndexedRandomVar's symbol_type is IndexedSymbol<Id, Dims...>
    using SymType = typename P::symbol_type;
    constexpr std::size_t idx = samples_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in DynamicSamples");

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

  // Raw access to underlying storage
  auto data() const -> const matrix3::DynamicDense<double>& { return data_; }
  auto data() -> matrix3::DynamicDense<double>& { return data_; }

  // Get state dimension
  auto stateDim() const -> std::size_t { return state_dim_; }

  // Get offset for a parameter
  template <typename P>
  auto offset(const P& /*p*/) const -> std::size_t {
    using SymType = samples_detail::ParamSymbolType<P>;
    constexpr std::size_t idx = samples_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in DynamicSamples");
    return offsets_[idx];
  }

  // Get size for a parameter
  template <typename P>
  auto paramSize(const P& /*p*/) const -> std::size_t {
    using SymType = samples_detail::ParamSymbolType<P>;
    constexpr std::size_t idx = samples_detail::SymbolIndex<SymType, SymbolsTuple>::value;
    static_assert(idx < NumSlots, "Parameter not found in DynamicSamples");
    return std::get<idx>(specs_).size();
  }

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
};

// Factory function
template <typename SymbolsTuple, typename SpecsTuple>
auto makeDynamicSamples(SymbolsTuple symbols, SpecsTuple specs, std::size_t state_dim) {
  return DynamicSamples<SymbolsTuple, SpecsTuple>{symbols, specs, state_dim};
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
