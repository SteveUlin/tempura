#pragma once

#include "symbolic4/core.h"

// ============================================================================
// effects.h - Probabilistic effects for the symbolic expression system
// ============================================================================
//
// Domain-specific effects for Bayesian modeling. These extend the core
// Atom<Id, Effect> framework with probabilistic semantics:
//
//   Sample<D>              - Random variable drawn from distribution D
//   IndexedSample<D, Dims> - Indexed family of random variables (plate notation)
//
// The core expression algebra (Atom, Expression, Free, Compute) is
// domain-independent. These effects live here because they carry probabilistic
// semantics that the core doesn't need to know about.

namespace tempura::symbolic4 {

// Sample from distribution D (for Bayesian modeling)
template <typename D>
struct Sample {
  [[no_unique_address]] D dist_;
  constexpr Sample() = default;
  constexpr Sample(D d) : dist_{d} {}
};

// Indexed sample from distribution D over dimensions Dims... (for plate notation).
// Mirrors Sample<D> but carries dimension info, enabling auto-discovery of
// indexed latent parameters in expression trees.
template <typename D, typename... Dims>
struct IndexedSample {
  [[no_unique_address]] D dist_;
  using dist_type = D;
  using dims_list = TypeList<Dims...>;
  constexpr IndexedSample() = default;
  constexpr IndexedSample(D d) : dist_{d} {}
};

// ============================================================================
// Effect Classification Traits
// ============================================================================

template <typename E>
constexpr bool is_sample_effect_v = core_traits_detail::isSpecOf<E, Sample>();

template <typename E>
constexpr bool is_indexed_sample_effect_v = core_traits_detail::isSpecOf<E, IndexedSample>();

template <typename T>
constexpr bool is_random_var_atom_v = [] consteval {
    if constexpr (!is_atom_v<T>) return false;
    else return is_sample_effect_v<get_effect_t<T>>;
}();

template <typename T>
constexpr bool is_indexed_random_var_atom_v = [] consteval {
    if constexpr (!is_atom_v<T>) return false;
    else return is_indexed_sample_effect_v<get_effect_t<T>>;
}();

template <typename E>
  requires is_sample_effect_v<E>
using get_distribution_t = [:std::meta::template_arguments_of(^^E)[0]:];

template <typename T>
  requires is_random_var_atom_v<T>
using get_atom_distribution_t = get_distribution_t<get_effect_t<T>>;

}  // namespace tempura::symbolic4
