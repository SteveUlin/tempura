#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "symbolic4/mcmc/transforms.h"

// ============================================================================
// transform_pack.h - Uniform transform layer for all parameter types
// ============================================================================
//
// TransformPack applies constraint transforms uniformly for ALL parameters,
// eliminating the scalar/indexed asymmetry in PlateTransformedPosterior.
//
// Before: scalar params transform at eval time (via Sample atom), indexed
// params transform at binding time (pre-applied by posterior). Two code paths.
//
// After: TransformPack transforms all params identically before evaluation.
// The evaluator sees only constrained values. One code path.
//
// Core operations:
//   transform(z)      — unconstrained -> constrained for all params
//   inverse(x)        — constrained -> unconstrained for all params
//   logJacobian(z)    — sum of log|det(Jacobian)| over all params
//   chainRuleGrad(g,z)— convert gradient from constrained to unconstrained space
//
// Usage:
//   auto pack = makeTransformPack(symbols, specs);
//   auto x = pack.transform(z);          // all params: z -> x
//   double lj = pack.logJacobian(z);     // total Jacobian correction
//   auto gz = pack.chainRuleGrad(gx, z); // gradient chain rule for all params
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// TransformPack — applies transforms uniformly for all params
// ============================================================================

template <typename SymbolsTuple, typename SpecsTuple>
class TransformPack {
  static_assert(std::tuple_size_v<SymbolsTuple> == std::tuple_size_v<SpecsTuple>,
                "Symbols and specs tuples must have the same size");

 public:
  static constexpr std::size_t NumParams = std::tuple_size_v<SymbolsTuple>;

  constexpr TransformPack(SymbolsTuple symbols, SpecsTuple specs)
      : symbols_{symbols}, specs_{specs} {}

  // -------------------------------------------------------------------------
  // Accessors
  // -------------------------------------------------------------------------

  constexpr auto symbols() const -> const SymbolsTuple& { return symbols_; }
  constexpr auto specs() const -> const SpecsTuple& { return specs_; }

  // Total state dimension (sum of all param sizes)
  auto stateDim() const -> std::size_t {
    return stateDimImpl(std::make_index_sequence<NumParams>{});
  }

  // Get the offset of a parameter in the flat state vector.
  // Looks up by matching the parameter's symbol type against the symbols tuple.
  template <typename P>
  auto offset(const P&) const -> std::size_t {
    constexpr std::size_t idx = findSymbolIndex<P>();
    static_assert(idx < NumParams, "Symbol not found in TransformPack");
    return computeOffset<idx>();
  }

  // Get the size of a parameter (1 for scalar, N for indexed).
  template <typename P>
  auto paramSize(const P&) const -> std::size_t {
    constexpr std::size_t idx = findSymbolIndex<P>();
    static_assert(idx < NumParams, "Symbol not found in TransformPack");
    return std::get<idx>(specs_).size();
  }

  // -------------------------------------------------------------------------
  // transform: unconstrained z -> constrained x for all params
  // -------------------------------------------------------------------------

  auto transform(std::span<const double> z) const -> std::vector<double> {
    assert(z.size() == stateDim());
    std::vector<double> x(z.size());
    std::size_t off = 0;
    transformImpl(z, x, off, std::make_index_sequence<NumParams>{});
    return x;
  }

  // -------------------------------------------------------------------------
  // inverse: constrained x -> unconstrained z for all params
  // -------------------------------------------------------------------------

  auto inverse(std::span<const double> x) const -> std::vector<double> {
    assert(x.size() == stateDim());
    std::vector<double> z(x.size());
    std::size_t off = 0;
    inverseImpl(x, z, off, std::make_index_sequence<NumParams>{});
    return z;
  }

  // -------------------------------------------------------------------------
  // logJacobian: sum of log|det(Jacobian)| over all params
  // -------------------------------------------------------------------------

  auto logJacobian(std::span<const double> z) const -> double {
    assert(z.size() == stateDim());
    double total = 0.0;
    std::size_t off = 0;
    logJacobianImpl(z, total, off, std::make_index_sequence<NumParams>{});
    return total;
  }

  // -------------------------------------------------------------------------
  // chainRuleGrad: convert gradient from constrained to unconstrained space
  //
  // Given grad_x = d(logp)/dx (gradient w.r.t. constrained values) and
  // unconstrained state z, returns:
  //   grad_z[i] = grad_x[i] * dx/dz + d(logJ)/dz
  //
  // This handles both scalar and indexed params uniformly via each spec's
  // chainRuleGrad (or the equivalent for scalar specs).
  // -------------------------------------------------------------------------

  auto chainRuleGrad(std::span<const double> grad_x,
                     std::span<const double> z) const -> std::vector<double> {
    assert(grad_x.size() == stateDim());
    assert(z.size() == stateDim());
    std::vector<double> grad_z(z.size());
    std::size_t off = 0;
    chainRuleGradImpl(grad_x, z, grad_z, off, std::make_index_sequence<NumParams>{});
    return grad_z;
  }

 private:
  SymbolsTuple symbols_;
  SpecsTuple specs_;

  // -------------------------------------------------------------------------
  // Symbol lookup: find index of a symbol type in the symbols tuple.
  // -------------------------------------------------------------------------

  template <typename P>
  static constexpr std::size_t findSymbolIndex() {
    if constexpr (requires { typename P::symbol_type; }) {
      return findByType<typename P::symbol_type>();
    } else {
      return findByType<P>();
    }
  }

  template <typename Sym, std::size_t I = 0>
  static constexpr std::size_t findByType() {
    if constexpr (I >= NumParams) {
      return NumParams;  // Not found
    } else if constexpr (std::is_same_v<Sym, std::tuple_element_t<I, SymbolsTuple>>) {
      return I;
    } else {
      return findByType<Sym, I + 1>();
    }
  }

  // -------------------------------------------------------------------------
  // Offset computation: sum of sizes for params [0, Idx)
  // -------------------------------------------------------------------------

  template <std::size_t Idx>
  auto computeOffset() const -> std::size_t {
    return computeOffsetImpl<Idx>(std::make_index_sequence<Idx>{});
  }

  template <std::size_t, std::size_t... Is>
  auto computeOffsetImpl(std::index_sequence<Is...>) const -> std::size_t {
    if constexpr (sizeof...(Is) == 0) {
      return 0;
    } else {
      return (std::get<Is>(specs_).size() + ...);
    }
  }

  // -------------------------------------------------------------------------
  // stateDim implementation
  // -------------------------------------------------------------------------

  template <std::size_t... Is>
  auto stateDimImpl(std::index_sequence<Is...>) const -> std::size_t {
    if constexpr (sizeof...(Is) == 0) {
      return 0;
    } else {
      return (std::get<Is>(specs_).size() + ...);
    }
  }

  // -------------------------------------------------------------------------
  // transform implementation: element-wise forward transform
  // -------------------------------------------------------------------------

  template <std::size_t... Is>
  void transformImpl(std::span<const double> z, std::vector<double>& x,
                     std::size_t& off, std::index_sequence<Is...>) const {
    (transformOneParam<Is>(z, x, off), ...);
  }

  template <std::size_t I>
  void transformOneParam(std::span<const double> z, std::vector<double>& x,
                         std::size_t& off) const {
    const auto& spec = std::get<I>(specs_);
    std::size_t n = spec.size();
    for (std::size_t i = 0; i < n; ++i) {
      x[off + i] = spec.transformValue(z[off + i]);
    }
    off += n;
  }

  // -------------------------------------------------------------------------
  // inverse implementation: element-wise inverse transform
  // -------------------------------------------------------------------------

  template <std::size_t... Is>
  void inverseImpl(std::span<const double> x, std::vector<double>& z,
                   std::size_t& off, std::index_sequence<Is...>) const {
    (inverseOneParam<Is>(x, z, off), ...);
  }

  template <std::size_t I>
  void inverseOneParam(std::span<const double> x, std::vector<double>& z,
                       std::size_t& off) const {
    const auto& spec = std::get<I>(specs_);
    std::size_t n = spec.size();
    for (std::size_t i = 0; i < n; ++i) {
      z[off + i] = spec.transform.inverse(x[off + i]);
    }
    off += n;
  }

  // -------------------------------------------------------------------------
  // logJacobian implementation: accumulate log|det(J)| for each element
  // -------------------------------------------------------------------------

  template <std::size_t... Is>
  void logJacobianImpl(std::span<const double> z, double& total,
                       std::size_t& off, std::index_sequence<Is...>) const {
    (logJacobianOneParam<Is>(z, total, off), ...);
  }

  template <std::size_t I>
  void logJacobianOneParam(std::span<const double> z, double& total,
                           std::size_t& off) const {
    const auto& spec = std::get<I>(specs_);
    std::size_t n = spec.size();
    for (std::size_t i = 0; i < n; ++i) {
      total += spec.logJacobian(z[off + i]);
    }
    off += n;
  }

  // -------------------------------------------------------------------------
  // chainRuleGrad implementation
  //
  // For each parameter element:
  //   grad_z = grad_x * dx/dz + d(logJ)/dz
  //
  // IndexedParamSpec has chainRuleGrad() directly. ScalarParamSpec uses the
  // underlying transform's chainRuleGrad() which computes the same formula.
  // -------------------------------------------------------------------------

  template <std::size_t... Is>
  void chainRuleGradImpl(std::span<const double> grad_x,
                         std::span<const double> z,
                         std::vector<double>& grad_z,
                         std::size_t& off,
                         std::index_sequence<Is...>) const {
    (chainRuleGradOneParam<Is>(grad_x, z, grad_z, off), ...);
  }

  template <std::size_t I>
  void chainRuleGradOneParam(std::span<const double> grad_x,
                             std::span<const double> z,
                             std::vector<double>& grad_z,
                             std::size_t& off) const {
    const auto& spec = std::get<I>(specs_);
    std::size_t n = spec.size();

    for (std::size_t i = 0; i < n; ++i) {
      if constexpr (requires { spec.chainRuleGrad(0.0, 0.0); }) {
        // Spec has chainRuleGrad (IndexedParamSpec, or ScalarParamSpec with
        // a transform that provides it)
        grad_z[off + i] = spec.chainRuleGrad(grad_x[off + i], z[off + i]);
      } else {
        // Fallback: manually compute grad_z = grad_x * dx/dz + d(logJ)/dz
        // using the transform's methods
        grad_z[off + i] = spec.transform.chainRuleGrad(grad_x[off + i], z[off + i]);
      }
    }
    off += n;
  }
};

// ============================================================================
// Factory function
// ============================================================================

template <typename SymbolsTuple, typename SpecsTuple>
auto makeTransformPack(SymbolsTuple symbols, SpecsTuple specs) {
  return TransformPack<SymbolsTuple, SpecsTuple>{symbols, specs};
}

}  // namespace tempura::symbolic4
