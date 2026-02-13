#pragma once

#include <array>
#include <cstddef>
#include <experimental/meta>
#include <tuple>
#include <type_traits>
#include <utility>

#include "symbolic4/core.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/symbolic_state.h"

// ============================================================================
// state.h - ParameterState: the universal glue for binding-centric MCMC
// ============================================================================
//
// ParameterState is the central currency between all MCMC operations:
// - Constructed from bindings (type-safe, explicit)
// - Queryable by symbol (state[alpha])
// - Convertible to/from arrays for HMC internals
//
// Usage:
//   auto alpha = normal(0.0, 10.0);
//   auto beta = normal(0.0, 5.0);
//
//   // Construction from bindings
//   auto state = ParameterState<decltype(alpha), decltype(beta)>{
//       BinderPack{alpha = 1.0, beta = 2.0}};
//
//   // Symbol-based access
//   state[alpha] = 1.5;
//   double b = state[beta];
//
//   // Array access for HMC
//   auto& arr = state.array();
//
// ============================================================================

namespace tempura::symbolic4 {


// ============================================================================
// ParameterState - Type-safe parameter container with symbol-based access
// ============================================================================

template <typename... Params>
class ParameterState {
 public:
  static constexpr std::size_t N = sizeof...(Params);
  using ArrayType = std::array<double, N>;

  // Default construction (zero-initialized)
  constexpr ParameterState() : values_{} {}

  // Construction from array (internal use, e.g., from HMC)
  constexpr explicit ParameterState(ArrayType values) : values_{values} {}

  // Construction from bindings - the main API
  // Usage: ParameterState{BinderPack{alpha = 1.0, beta = 2.0}}
  template <typename... Binders>
  constexpr ParameterState(BinderPack<Binders...> bindings) : values_{} {
    initFromBindings(bindings, std::index_sequence_for<Params...>{});
  }

  // Symbol-based access (compile-time lookup)
  template <typename P>
    requires IsRandomVar<P>
  constexpr auto operator[](const P&) -> double& {
    constexpr std::size_t idx = indexOfParam<P>();
    static_assert(idx < N, "Parameter not found in ParameterState");
    return values_[idx];
  }

  template <typename P>
    requires IsRandomVar<P>
  constexpr auto operator[](const P&) const -> double {
    constexpr std::size_t idx = indexOfParam<P>();
    static_assert(idx < N, "Parameter not found in ParameterState");
    return values_[idx];
  }

  // Array access for HMC internals
  constexpr auto array() -> ArrayType& { return values_; }
  constexpr auto array() const -> const ArrayType& { return values_; }

  // Index-based access (for iteration)
  constexpr auto operator[](std::size_t i) -> double& { return values_[i]; }
  constexpr auto operator[](std::size_t i) const -> double { return values_[i]; }

  // Size
  static constexpr auto size() -> std::size_t { return N; }

  // Find index of a parameter type (public for Samples to use)
  template <typename P>
  static constexpr auto indexOfParam() -> std::size_t {
    using TargetSym = typename P::symbol_type;
    return symbolic_state_detail::SymbolIndex<TargetSym,
        std::tuple<typename Params::symbol_type...>>::value;
  }

 private:
  ArrayType values_;

  // Initialize from bindings by matching each param to its binder
  template <typename... Binders, std::size_t... Is>
  constexpr void initFromBindings(BinderPack<Binders...> bindings,
                                  std::index_sequence<Is...>) {
    // For each param position, find and extract the matching binding value
    ((values_[Is] = extractBindingForParam<Is>(bindings)), ...);
  }

  // Extract binding value for param at position I
  template <std::size_t I, typename... Binders>
  constexpr auto extractBindingForParam(BinderPack<Binders...> bindings) -> double {
    using ParamType = std::tuple_element_t<I, std::tuple<Params...>>;
    using SymType = typename ParamType::symbol_type;
    // bindings[sym] uses the inherited operator[] from the matching TypeValueBinder
    return static_cast<double>(bindings[SymType{}]);
  }
};

// Deduction guide: infer param types from RandomVars
template <typename... Params>
  requires(IsRandomVar<Params> && ...)
ParameterState(Params...) -> ParameterState<Params...>;

// Factory function to create ParameterState from bindings given param types
template <typename... Params, typename... Binders>
constexpr auto makeParameterState(BinderPack<Binders...> bindings)
    -> ParameterState<Params...> {
  return ParameterState<Params...>{bindings};
}

}  // namespace tempura::symbolic4
