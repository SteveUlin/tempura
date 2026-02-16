#pragma once

#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "symbolic4/core.h"

// ============================================================================
// mutable_state.h - Per-slot typed container for HMC state
// ============================================================================
//
// MutableState is the BinderPack pattern applied to mutable MCMC state.
// Each parameter occupies its own typed slot — scalar params store double,
// indexed params store vector<double>. No flat backing vector, no offset
// arithmetic. Symbol-dispatched operator[] resolves at compile time.
//
// Usage:
//   MutableState<ScalarSlot<AlphaSym>, IndexedSlot<ZSym>> state{...};
//   state[AlphaSym{}]  →  double&
//   state[ZSym{}]      →  std::span<double>
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Slot types — individual typed storage units
// ============================================================================

// Scalar slot — owns a double, dispatched by symbol type
template <typename SymType>
struct ScalarSlot {
  double value = 0.0;

  auto operator[](SymType) -> double& { return value; }
  auto operator[](SymType) const -> const double& { return value; }
};

// Indexed slot — owns a vector<double>, dispatched by symbol type
template <typename SymType>
struct IndexedSlot {
  std::vector<double> values;

  auto operator[](SymType) -> std::span<double> { return values; }
  auto operator[](SymType) const -> std::span<const double> { return values; }
};

// ============================================================================
// Slot size helpers (free functions for fold expressions)
// ============================================================================

template <typename Sym>
auto slotSize(const ScalarSlot<Sym>&) -> std::size_t { return 1; }

template <typename Sym>
auto slotSize(const IndexedSlot<Sym>& s) -> std::size_t { return s.values.size(); }

// ============================================================================
// MutableState — BinderPack pattern for mutable per-slot typed state
// ============================================================================
//
// Multiple inheritance from Slots... pulls every operator[] into the same
// overload set. state[AlphaSym{}] resolves to ScalarSlot<AlphaSym>::operator[]
// at compile time — zero runtime dispatch cost.

template <typename... Slots>
struct MutableState : Slots... {
  using Slots::operator[]...;

  // Unwrap types with symbol_type (RandomVar, IndexedRandomVar, etc.)
  template <typename T>
    requires (requires { typename T::symbol_type; } && !(Symbolic<T>))
  auto operator[](const T&) -> decltype(auto) {
    return (*this)[typename T::symbol_type{}];
  }

  template <typename T>
    requires (requires { typename T::symbol_type; } && !(Symbolic<T>))
  auto operator[](const T&) const -> decltype(auto) {
    return (*this)[typename T::symbol_type{}];
  }

  // Total number of scalar doubles across all slots
  auto size() const -> std::size_t {
    return (slotSize(static_cast<const Slots&>(*this)) + ...);
  }
};

// CTAD: deduce slot types from constructor arguments
template <typename... Slots>
MutableState(Slots...) -> MutableState<Slots...>;

// ============================================================================
// Construction helpers
// ============================================================================

namespace mutable_state_detail {

// Map (Symbol, Spec) → appropriate slot type, zero-initialized
template <typename Sym, typename Spec>
auto makeSlot(const Spec& spec) {
  if constexpr (!Spec::is_indexed) {
    return ScalarSlot<Sym>{0.0};
  } else {
    return IndexedSlot<Sym>{std::vector<double>(spec.size(), 0.0)};
  }
}

}  // namespace mutable_state_detail

// Construct a zero-initialized MutableState from symbols/specs tuples
template <typename SymsTuple, typename SpecsTuple, std::size_t... Is>
auto makeZeroState(SymsTuple, const SpecsTuple& specs, std::index_sequence<Is...>) {
  return MutableState{
      mutable_state_detail::makeSlot<std::tuple_element_t<Is, SymsTuple>>(
          std::get<Is>(specs))...};
}

}  // namespace tempura::symbolic4
