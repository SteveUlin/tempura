#pragma once

#include <array>
#include <span>
#include <vector>

#include "meta/tags.h"
#include "symbolic4/core.h"

// ============================================================================
// dim.h - Dimension tags and indexed symbols for plate notation
// ============================================================================
//
// Supports multi-dimensional indexing for hierarchical models:
//
//   struct Countries {};
//   struct Years {};
//
//   auto alpha = plate<Countries>(normal(mu, sigma));        // alpha[c]
//   auto y = plate<Years>(normal(alpha, 1.0_c));             // y[c,y]
//
//   // y.sym() is IndexedSymbol<Id, Countries, Years>
//   // Binding: y = indexed(data, shape<Countries, Years>{10, 20})
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Local type list helpers (in symbolic4 namespace)
// ============================================================================

namespace detail {

// At<I, TypeList<...>> — get type at index I via splice
template <SizeT I, typename List>
struct AtImpl;

template <SizeT I, typename... Ts>
struct AtImpl<I, TypeList<Ts...>> {
  using type = [:std::meta::template_arguments_of(^^TypeList<Ts...>)[I]:];
};

template <SizeT I, typename List>
using At = typename AtImpl<I, List>::type;

// IndexOf<T, TypeList<...>> — consteval loop
template <typename T, typename List>
consteval auto indexOfImpl() -> SizeT {
  auto args = std::meta::template_arguments_of(^^List);
  for (SizeT i = 0; i < args.size(); ++i) {
    if (args[i] == ^^T) return i;
  }
  return static_cast<SizeT>(-1);
}

template <typename T, typename List>
constexpr SizeT IndexOf = indexOfImpl<T, List>();

}  // namespace detail

// ============================================================================
// IndexedSymbol - A symbol that varies over one or more dimensions
// ============================================================================
//
// Type parameters:
//   Id      - Unique identifier for this indexed variable
//   Dims... - Dimension tags this symbol is indexed over (left to right)
//
// Examples:
//   IndexedSymbol<ThetaId, Countries>         // theta[c]
//   IndexedSymbol<YId, Countries, Years>      // y[c, y]

template <typename Id, typename... Dims>
struct IndexedSymbol : SymbolicTag {
  using id_type = Id;
  using dims_list = TypeList<Dims...>;
  static constexpr SizeT ndims = sizeof...(Dims);

  // Check if this symbol is indexed over a specific dimension
  template <typename DimTag>
  static constexpr bool has_dim = (std::is_same_v<DimTag, Dims> || ...);
};

// Type traits for IndexedSymbol
template <typename T>
constexpr bool is_indexed_symbol_v = core_traits_detail::isSpecOf<T, IndexedSymbol>();

// Get number of dimensions
template <typename T>
struct IndexedSymbolNDims;

template <typename Id, typename... Dims>
struct IndexedSymbolNDims<IndexedSymbol<Id, Dims...>> {
  static constexpr SizeT value = sizeof...(Dims);
};

template <typename T>
constexpr SizeT indexed_symbol_ndims_v = IndexedSymbolNDims<T>::value;

// ============================================================================
// Shape descriptor for multi-dimensional data
// ============================================================================

template <typename... Dims>
struct Shape {
  std::array<SizeT, sizeof...(Dims)> sizes;

  constexpr Shape(std::initializer_list<SizeT> init) {
    SizeT i = 0;
    for (auto s : init) sizes[i++] = s;
  }

  template <typename... Ss>
    requires(sizeof...(Ss) == sizeof...(Dims))
  constexpr Shape(Ss... ss) : sizes{static_cast<SizeT>(ss)...} {}

  constexpr SizeT operator[](SizeT i) const { return sizes[i]; }
  constexpr SizeT size() const { return sizeof...(Dims); }

  // Total number of elements (product of all dimensions)
  constexpr SizeT total() const {
    SizeT t = 1;
    for (auto s : sizes) t *= s;
    return t;
  }

  // Get size for a specific dimension by index
  template <SizeT I>
  constexpr SizeT get() const {
    static_assert(I < sizeof...(Dims));
    return sizes[I];
  }
};

// ============================================================================
// IndexedValues - Multi-dimensional data wrapper
// ============================================================================

// 1D indexed values (backward compatible)
struct IndexedValues {
  std::span<const double> values;

  IndexedValues(std::span<const double> v) : values{v} {}
  IndexedValues(const std::vector<double>& v) : values{v} {}

  auto size() const { return values.size(); }
  auto operator[](SizeT i) const { return values[i]; }
};

// Multi-dimensional indexed values
template <typename... Dims>
struct IndexedValuesND {
  std::span<const double> data;  // Flattened row-major
  Shape<Dims...> shape;

  IndexedValuesND(std::span<const double> d, Shape<Dims...> s) : data{d}, shape{s} {}
  IndexedValuesND(const std::vector<double>& v, Shape<Dims...> s) : data{v}, shape{s} {}

  // 1D access
  auto at(SizeT i) const requires(sizeof...(Dims) == 1) { return data[i]; }

  // 2D access (row-major)
  auto at(SizeT i, SizeT j) const requires(sizeof...(Dims) == 2) {
    return data[i * shape[1] + j];
  }

  // 3D access
  auto at(SizeT i, SizeT j, SizeT k) const requires(sizeof...(Dims) == 3) {
    return data[(i * shape[1] + j) * shape[2] + k];
  }

  // Generic N-D access via array of indices
  auto at(const std::array<SizeT, sizeof...(Dims)>& indices) const {
    SizeT flat = 0;
    SizeT stride = 1;
    for (int d = sizeof...(Dims) - 1; d >= 0; --d) {
      flat += indices[d] * stride;
      stride *= shape[d];
    }
    return data[flat];
  }

  auto total() const { return shape.total(); }
};

// Factory functions
inline auto indexed(std::span<const double> v) { return IndexedValues{v}; }
inline auto indexed(const std::vector<double>& v) { return IndexedValues{v}; }

template <typename... Dims>
auto indexed(std::span<const double> v, Shape<Dims...> s) {
  return IndexedValuesND<Dims...>{v, s};
}

template <typename... Dims>
auto indexed(const std::vector<double>& v, Shape<Dims...> s) {
  return IndexedValuesND<Dims...>{v, s};
}

// ============================================================================
// IndexedBinding - Binds an IndexedSymbol to data
// ============================================================================

// Forward declaration
template <typename Sym, SizeT NDims = Sym::ndims>
struct IndexedBinding;

// 1D binding (single dimension)
template <typename Sym>
struct IndexedBinding<Sym, 1> {
  using symbol_type = Sym;
  using dims_list = typename Sym::dims_list;
  static constexpr SizeT ndims = 1;

  std::span<const double> values;

  auto size() const { return values.size(); }
  auto at(SizeT i) const { return values[i]; }

  // For dimension size queries
  template <typename DimTag>
  auto dimSize() const requires(Sym::template has_dim<DimTag>) {
    return values.size();
  }

  auto operator[](Sym) const { return *this; }
};

// 2D binding
template <typename Sym>
struct IndexedBinding<Sym, 2> {
  using symbol_type = Sym;
  using dims_list = typename Sym::dims_list;
  static constexpr SizeT ndims = 2;

  std::span<const double> data;
  std::array<SizeT, 2> shape;

  auto at(SizeT i, SizeT j) const { return data[i * shape[1] + j]; }

  // Get size of a dimension by tag
  template <typename DimTag>
  auto dimSize() const requires(Sym::template has_dim<DimTag>) {
    constexpr SizeT idx = detail::IndexOf<DimTag, dims_list>;
    return shape[idx];
  }

  auto operator[](Sym) const { return *this; }
};

// 3D binding
template <typename Sym>
struct IndexedBinding<Sym, 3> {
  using symbol_type = Sym;
  using dims_list = typename Sym::dims_list;
  static constexpr SizeT ndims = 3;

  std::span<const double> data;
  std::array<SizeT, 3> shape;

  auto at(SizeT i, SizeT j, SizeT k) const {
    return data[(i * shape[1] + j) * shape[2] + k];
  }

  template <typename DimTag>
  auto dimSize() const requires(Sym::template has_dim<DimTag>) {
    constexpr SizeT idx = detail::IndexOf<DimTag, dims_list>;
    return shape[idx];
  }

  auto operator[](Sym) const { return *this; }
};

// Type traits
template <typename T>
constexpr bool is_indexed_binding_v = core_traits_detail::isSpecOf<T>(^^IndexedBinding);

// ============================================================================
// Helper to get dimension info from binding
// ============================================================================

template <typename T>
struct GetDimsFromBinding;

template <typename Sym, SizeT N>
struct GetDimsFromBinding<IndexedBinding<Sym, N>> {
  using type = typename Sym::dims_list;
};

template <typename T>
using get_dims_from_binding_t = typename GetDimsFromBinding<T>::type;

}  // namespace tempura::symbolic4
