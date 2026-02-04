#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================================
// symbolic_state.h - SymbolicSlot and SymbolicState
// ============================================================================
//
// A mutable, symbol-indexed state backed by a flat vector<double>.
// Each parameter occupies a typed "slot" in the state. Scalar params
// return double&, indexed params return span<double>. The flat vector
// is exposed for HMC, but users interact through typed symbol lookup.
//
// This is the Phase 7.0 foundation: all MCMC containers (state, gradient,
// samples) will eventually be SymbolicState instances with different
// slot configurations.
//
// Usage:
//   auto symbols = std::make_tuple(mu_sym, z_b_sym);
//   auto specs = std::make_tuple(scalar_spec, indexed_spec);
//   SymbolicState state{symbols, specs, dim};
//
//   state[mu_sym]   // → double&
//   state[z_b_sym]  // → std::span<double>
//   state.data()    // → std::vector<double>& (for HMC)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// SymbolicSlot — describes one parameter's position in the state vector
// ============================================================================
//
// Each slot knows its symbol type, spec type, offset into the flat vector,
// and size (1 for scalar, N for indexed). The spec's is_indexed flag drives
// compile-time dispatch between scalar (double&) and indexed (span) access.

template <typename SymType, typename SpecType>
struct SymbolicSlot {
  using symbol_type = SymType;
  using spec_type = SpecType;

  [[no_unique_address]] SymType sym_;
  SpecType spec_;
  std::size_t offset_ = 0;

  constexpr auto size() const -> std::size_t { return spec_.size(); }

  static constexpr bool isScalar() { return !SpecType::is_indexed; }
  static constexpr bool isIndexed() { return SpecType::is_indexed; }
};

// ============================================================================
// SymbolicState — mutable symbol-indexed state backed by a flat vector
// ============================================================================

namespace symbolic_state_detail {

// Build a SymbolicSlot from a (symbol, spec) pair at index I
template <typename SymbolsTuple, typename SpecsTuple, std::size_t I>
struct SlotType {
  using sym_type = std::decay_t<std::tuple_element_t<I, SymbolsTuple>>;
  using spec_type = std::decay_t<std::tuple_element_t<I, SpecsTuple>>;
  using type = SymbolicSlot<sym_type, spec_type>;
};

template <typename SymbolsTuple, typename SpecsTuple, std::size_t I>
using slot_type_t = typename SlotType<SymbolsTuple, SpecsTuple, I>::type;

// Find the index of a symbol in a symbols tuple
template <typename Sym, typename SymbolsTuple, std::size_t I = 0>
struct SymbolIndex {
  static constexpr std::size_t value = [] {
    if constexpr (I >= std::tuple_size_v<SymbolsTuple>) {
      return std::size_t(-1);
    } else if constexpr (std::is_same_v<std::decay_t<Sym>,
                                        std::decay_t<std::tuple_element_t<I, SymbolsTuple>>>) {
      return I;
    } else {
      return SymbolIndex<Sym, SymbolsTuple, I + 1>::value;
    }
  }();
};

}  // namespace symbolic_state_detail

template <typename SymbolsTuple, typename SpecsTuple>
class SymbolicState {
  static constexpr std::size_t N = std::tuple_size_v<SymbolsTuple>;
  static_assert(N == std::tuple_size_v<SpecsTuple>,
                "Symbols and specs tuples must have the same size");

 public:
  // Construct with given dimension, zero-initialized data
  SymbolicState(SymbolsTuple symbols, SpecsTuple specs, std::size_t dim)
      : symbols_{symbols}, specs_{specs}, data_(dim, 0.0) {
    computeOffsets();
    assert(totalSize() == dim);
  }

  // Construct from existing data vector (moved in)
  SymbolicState(SymbolsTuple symbols, SpecsTuple specs, std::vector<double> data)
      : symbols_{symbols}, specs_{specs}, data_{std::move(data)} {
    computeOffsets();
    assert(totalSize() == data_.size());
  }

  // -----------------------------------------------------------------------
  // Symbol-based access: scalar params → double&, indexed params → span
  // -----------------------------------------------------------------------

  template <typename Sym>
  auto operator[](const Sym& /*sym*/) -> decltype(auto) {
    constexpr std::size_t idx = findIndex<Sym>();
    static_assert(idx < N, "Symbol not found in SymbolicState");

    using SpecType = std::decay_t<std::tuple_element_t<idx, SpecsTuple>>;
    if constexpr (!SpecType::is_indexed) {
      return data_[offsets_[idx]];
    } else {
      return std::span<double>(data_.data() + offsets_[idx],
                               std::get<idx>(specs_).size());
    }
  }

  template <typename Sym>
  auto operator[](const Sym& /*sym*/) const -> decltype(auto) {
    constexpr std::size_t idx = findIndex<Sym>();
    static_assert(idx < N, "Symbol not found in SymbolicState");

    using SpecType = std::decay_t<std::tuple_element_t<idx, SpecsTuple>>;
    if constexpr (!SpecType::is_indexed) {
      return data_[offsets_[idx]];
    } else {
      return std::span<const double>(data_.data() + offsets_[idx],
                                     std::get<idx>(specs_).size());
    }
  }

  // -----------------------------------------------------------------------
  // Flat vector access for HMC
  // -----------------------------------------------------------------------

  auto data() -> std::vector<double>& { return data_; }
  auto data() const -> const std::vector<double>& { return data_; }

  auto size() const -> std::size_t { return data_.size(); }

  // -----------------------------------------------------------------------
  // Metadata access
  // -----------------------------------------------------------------------

  auto symbols() const -> const SymbolsTuple& { return symbols_; }
  auto specs() const -> const SpecsTuple& { return specs_; }

  // Get the offset and size for a specific symbol
  template <typename Sym>
  auto offset(const Sym& /*sym*/) const -> std::size_t {
    constexpr std::size_t idx = findIndex<Sym>();
    static_assert(idx < N, "Symbol not found in SymbolicState");
    return offsets_[idx];
  }

  template <typename Sym>
  auto paramSize(const Sym& /*sym*/) const -> std::size_t {
    constexpr std::size_t idx = findIndex<Sym>();
    static_assert(idx < N, "Symbol not found in SymbolicState");
    return std::get<idx>(specs_).size();
  }

  // Number of parameter slots (not total dimension)
  static constexpr auto numSlots() -> std::size_t { return N; }

 private:
  SymbolsTuple symbols_;
  SpecsTuple specs_;
  std::vector<double> data_;
  std::array<std::size_t, N> offsets_{};

  // Find the tuple index for a given symbol type
  template <typename Sym>
  static constexpr auto findIndex() -> std::size_t {
    return symbolic_state_detail::SymbolIndex<Sym, SymbolsTuple>::value;
  }

  // Compute cumulative offsets from spec sizes (same pattern as GradientResult)
  void computeOffsets() {
    computeOffsetsImpl(std::make_index_sequence<N>{});
  }

  template <std::size_t... Is>
  void computeOffsetsImpl(std::index_sequence<Is...>) {
    std::size_t running = 0;
    ((offsets_[Is] = running, running += std::get<Is>(specs_).size()), ...);
  }

  // Total size from specs (for validation)
  auto totalSize() const -> std::size_t {
    return totalSizeImpl(std::make_index_sequence<N>{});
  }

  template <std::size_t... Is>
  auto totalSizeImpl(std::index_sequence<Is...>) const -> std::size_t {
    return (std::get<Is>(specs_).size() + ...);
  }
};

// ============================================================================
// Factory function
// ============================================================================

template <typename SymbolsTuple, typename SpecsTuple>
auto makeSymbolicState(SymbolsTuple symbols, SpecsTuple specs, std::size_t dim) {
  return SymbolicState<SymbolsTuple, SpecsTuple>{symbols, specs, dim};
}

template <typename SymbolsTuple, typename SpecsTuple>
auto makeSymbolicState(SymbolsTuple symbols, SpecsTuple specs, std::vector<double> data) {
  return SymbolicState<SymbolsTuple, SpecsTuple>{symbols, specs, std::move(data)};
}

}  // namespace tempura::symbolic4
