#pragma once

#include "symbolic4/indexed/dim.h"

// ============================================================================
// data.h - Indexed data placeholders for non-random indexed values
// ============================================================================
//
// IndexedData represents fixed data that varies over an index dimension but
// is NOT a random variable. This is used for:
//   - Sample sizes (n in Binomial)
//   - Predictors/covariates (x in regression)
//   - Any indexed constant in the model
//
// Usage:
//   struct Countries {};
//   auto n = data<Countries>();   // Total submissions per country
//
//   auto theta = plate<Countries>(beta(alpha, beta_param));
//   auto k = plate<Countries>(binomial(n, theta));  // n varies per country
//
//   // In posterior:
//   .bindData(n = indexed(n_values))
//
// Unlike RandomVar or IndexedRandomVar, IndexedData:
//   - Has no distribution (no logProb)
//   - Is not a parameter to be inferred
//   - Is bound to fixed data before evaluation
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// IndexedData - Placeholder for indexed constant data
// ============================================================================
//
// Extends SymbolicTag so it can be used directly in arithmetic expressions.
// When used in expressions, the operators work on IndexedData and create
// Expression nodes that contain the corresponding IndexedSymbol.

template <typename Id, typename... Dims>
struct IndexedData : SymbolicTag {
  using id_type = Id;
  using dims_list = TypeList<Dims...>;
  using symbol_type = IndexedSymbol<Id, Dims...>;
  static constexpr SizeT ndims = sizeof...(Dims);

  // Get the indexed symbol representing this data
  static constexpr auto sym() { return symbol_type{}; }

  // Implicit conversion to symbol (for use in expressions)
  constexpr operator symbol_type() const { return sym(); }

  // Binding syntax: n = indexed(values)
  auto operator=(IndexedValues iv) const { return IndexedBinding<symbol_type>{iv.values}; }

  // Multi-dim binding
  template <typename... ShapeDims>
  auto operator=(IndexedValuesND<ShapeDims...> iv) const {
    return IndexedBinding<symbol_type, sizeof...(ShapeDims)>{iv.data, iv.shape.sizes};
  }
};

// Type traits
template <typename T>
constexpr bool is_indexed_data_v = core_traits_detail::isSpecOf<T, IndexedData>();

// Concept
template <typename T>
concept IsIndexedDataType = is_indexed_data_v<T>;

// ============================================================================
// Factory: data<DimTag>() - Create indexed data placeholder
// ============================================================================

template <typename DimTag, typename Id = decltype([] {})>
constexpr auto data() {
  return IndexedData<Id, DimTag>{};
}

// For multiple dimensions
template <typename Dim1, typename Dim2, typename Id = decltype([] {})>
constexpr auto data() {
  return IndexedData<Id, Dim1, Dim2>{};
}

template <typename Dim1, typename Dim2, typename Dim3, typename Id = decltype([] {})>
constexpr auto data() {
  return IndexedData<Id, Dim1, Dim2, Dim3>{};
}

// ============================================================================
// toSymbolic overload for IndexedData
// ============================================================================

template <typename Id, typename... Dims>
constexpr auto toSymbolic(const IndexedData<Id, Dims...>& d) {
  return d.sym();
}

}  // namespace tempura::symbolic4
