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
// The core expression algebra (Atom, Expression, Free, Embedded, Compute) is
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

// Indexed sample from distribution D over dimensions DimsList (for plate notation).
// Mirrors Sample<D> but carries dimension info, enabling auto-discovery of
// indexed latent parameters in expression trees.
template <typename D, typename DimsList>
struct IndexedSample {
  [[no_unique_address]] D dist_;
  using dist_type = D;
  using dims_list = DimsList;
  constexpr IndexedSample() = default;
  constexpr IndexedSample(D d) : dist_{d} {}
};

// ============================================================================
// Effect Classification Traits
// ============================================================================

// Is this effect a Sample?
template <typename E>
struct IsSampleEffect : std::false_type {};

template <typename D>
struct IsSampleEffect<Sample<D>> : std::true_type {};

template <typename E>
constexpr bool is_sample_effect_v = IsSampleEffect<E>::value;

// Is this atom a random variable (has Sample effect)?
template <typename T>
struct IsRandomVarAtom : std::false_type {};

template <typename Id, typename D>
struct IsRandomVarAtom<Atom<Id, Sample<D>>> : std::true_type {};

template <typename T>
constexpr bool is_random_var_atom_v = IsRandomVarAtom<T>::value;

// Is this effect an IndexedSample?
template <typename E>
struct IsIndexedSampleEffect : std::false_type {};
template <typename D, typename DimsList>
struct IsIndexedSampleEffect<IndexedSample<D, DimsList>> : std::true_type {};
template <typename E>
constexpr bool is_indexed_sample_effect_v = IsIndexedSampleEffect<E>::value;

// Is this atom an indexed random variable? (Atom<Id, IndexedSample<D, DimsList>>)
template <typename T>
struct IsIndexedRandomVarAtom : std::false_type {};
template <typename Id, typename D, typename DimsList>
struct IsIndexedRandomVarAtom<Atom<Id, IndexedSample<D, DimsList>>> : std::true_type {};
template <typename T>
constexpr bool is_indexed_random_var_atom_v = IsIndexedRandomVarAtom<T>::value;

// Get the Distribution type from a Sample effect
template <typename E>
struct GetDistribution;

template <typename D>
struct GetDistribution<Sample<D>> {
  using type = D;
};

template <typename E>
using get_distribution_t = typename GetDistribution<E>::type;

// Get the Distribution type from an Atom (convenience wrapper)
template <typename T>
struct GetDistributionFromAtom;

template <typename Id, typename D>
struct GetDistributionFromAtom<Atom<Id, Sample<D>>> {
  using type = D;
};

template <typename T>
using get_atom_distribution_t = typename GetDistributionFromAtom<T>::type;

}  // namespace tempura::symbolic4
