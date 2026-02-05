#pragma once

#include <array>
#include <cmath>

#include "symbolic4/core.h"
#include "symbolic4/operators.h"

// ============================================================================
// multivariate.h - Vector-valued random variables and distributions
// ============================================================================
//
// Provides support for multivariate distributions where a single RV draw
// produces a vector of values (e.g., MultivariateNormal).
//
// Key concept: event_shape vs batch_shape
//   - event_shape: dimensions within a single sample (vector components)
//   - batch_shape: dimensions over independent samples (plates)
//
// For MultivariateNormal(mu, Sigma):
//   - event_shape = [D] where D is the vector dimension
//   - log_prob sums over event dims internally -> returns scalar
//
// ============================================================================

namespace tempura::symbolic4 {

// Forward declaration
template <typename Id, SizeT D, SizeT I>
struct VectorComponent;

// ============================================================================
// VectorSymbol - A symbol representing a D-dimensional vector
// ============================================================================
//
// Unlike scalar Symbol<Id>, VectorSymbol<Id, D> represents D values that
// must be bound together.

template <typename Id, SizeT D>
struct VectorSymbol : SymbolicTag {
  using id_type = Id;
  static constexpr SizeT dim = D;

  // Access individual components symbolically
  template <SizeT I>
  static constexpr auto component() {
    static_assert(I < D, "Component index out of range");
    return VectorComponent<Id, D, I>{};
  }
};

// A single component of a VectorSymbol
template <typename Id, SizeT D, SizeT I>
struct VectorComponent : SymbolicTag {
  using id_type = Id;
  static constexpr SizeT dim = D;
  static constexpr SizeT index = I;
};

// Type traits — VectorSymbol/VectorComponent have SizeT NTTPs, use info-based isSpecOf
template <typename T>
constexpr bool is_vector_symbol_v = core_traits_detail::isSpecOf<T>(^^VectorSymbol);

template <typename T>
constexpr bool is_vector_component_v = core_traits_detail::isSpecOf<T>(^^VectorComponent);

// ============================================================================
// VectorBinding - Bind a VectorSymbol to actual values
// ============================================================================

template <typename Sym>
  requires is_vector_symbol_v<Sym>
struct VectorBinding {
  using symbol_type = Sym;
  static constexpr SizeT dim = Sym::dim;

  std::array<double, dim> values;

  VectorBinding(std::array<double, dim> v) : values{v} {}

  template <typename... Ts>
    requires (sizeof...(Ts) == dim)
  VectorBinding(Ts... args) : values{static_cast<double>(args)...} {}

  double at(SizeT i) const { return values[i]; }
  double operator[](SizeT i) const { return values[i]; }

  // Lookup by component
  template <SizeT I>
  double get() const {
    static_assert(I < dim, "Component index out of range");
    return values[I];
  }

  // Match operator[] for BinderPack lookup
  template <SizeT I>
  double operator[](VectorComponent<typename Sym::id_type, dim, I>) const {
    return values[I];
  }
};

template <typename T>
constexpr bool is_vector_binding_v = core_traits_detail::isSpecOf<T, VectorBinding>();

// ============================================================================
// MultivariateNormalDist - D-dimensional normal distribution (diagonal cov)
// ============================================================================
//
// Simplified version with diagonal covariance matrix (independent components).
// Full MultivariateNormal with arbitrary covariance would need matrix ops.
//
// For diagonal covariance:
//   log p(x | mu, sigma) = -0.5 * D * log(2π) - Σᵢ log(σᵢ)
//                         - 0.5 * Σᵢ ((xᵢ - μᵢ) / σᵢ)²
//
// Since we drop normalizing constants:
//   unnormalized: -0.5 * Σᵢ ((xᵢ - μᵢ) / σᵢ)²

template <SizeT D, typename MuParams, typename SigmaParams>
struct DiagonalMultivariateNormalDist {
  static constexpr SizeT dim = D;

  MuParams mu_params_;      // D-tuple of symbolic expressions for means
  SigmaParams sigma_params_; // D-tuple of symbolic expressions for std devs

  constexpr DiagonalMultivariateNormalDist(MuParams mu, SigmaParams sigma)
      : mu_params_{mu}, sigma_params_{sigma} {}

  // Unnormalized log-prob for vector symbol
  // Returns sum over components: -0.5 * Σᵢ ((xᵢ - μᵢ) / σᵢ)²
  template <typename VecSym>
    requires is_vector_symbol_v<VecSym> && (VecSym::dim == D)
  constexpr auto unnormalizedLogProbFor([[maybe_unused]] VecSym) const {
    return unnormalizedLogProbImpl<VecSym>(std::make_index_sequence<D>{});
  }

  // Full log-prob (same as unnormalized for symbolic differentiation)
  template <typename VecSym>
    requires is_vector_symbol_v<VecSym> && (VecSym::dim == D)
  constexpr auto logProbFor(VecSym sym) const {
    return unnormalizedLogProbFor(sym);
  }

private:
  template <typename VecSym, SizeT... Is>
  constexpr auto unnormalizedLogProbImpl([[maybe_unused]] std::index_sequence<Is...>) const {
    return (componentLogProb<VecSym, Is>() + ...);
  }

  template <typename VecSym, SizeT I>
  constexpr auto componentLogProb() const {
    auto x_i = VecSym::template component<I>();
    auto mu_i = std::get<I>(mu_params_);
    auto sigma_i = std::get<I>(sigma_params_);
    auto z = (x_i - mu_i) / sigma_i;
    return Fraction<-1, 2>{} * z * z;
  }
};

// Factory for diagonal MVN
template <typename... Mus, typename... Sigmas>
  requires (sizeof...(Mus) == sizeof...(Sigmas))
constexpr auto diagMvNormal(std::tuple<Mus...> mu, std::tuple<Sigmas...> sigma) {
  constexpr SizeT D = sizeof...(Mus);
  return DiagonalMultivariateNormalDist<D, std::tuple<Mus...>, std::tuple<Sigmas...>>{mu, sigma};
}

// Convenience: construct from variadic means and sigmas
template <Symbolic... Mus>
constexpr auto mvNormalMean(Mus... mus) {
  return std::tuple{mus...};
}

template <Symbolic... Sigmas>
constexpr auto mvNormalSigma(Sigmas... sigmas) {
  return std::tuple{sigmas...};
}

}  // namespace tempura::symbolic4
