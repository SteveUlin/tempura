#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/discover_params.h"
#include "symbolic4/distributions/indexed_node.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/mcmc/plate_transforms.h"
#include "symbolic4/mcmc/support.h"

// ============================================================================
// infer.h - Simplified inference API with automatic parameter discovery
// ============================================================================
//
// The infer() function takes OBSERVED random variables and automatically
// discovers all latent parameters they depend on.
//
// Usage:
//   auto alpha = normal(0.0, 10.0);
//   auto beta = normal(0.0, 5.0);
//   auto sigma = halfNormal(2.0);
//   auto x = data<Obs>();
//   auto y = plate<Obs>(normal(alpha + beta * x, sigma));
//
//   // Just pass the observed variable - params are discovered automatically!
//   auto posterior = infer(y).bind(x = x_data, y = y_data);
//
// How it works:
//   1. infer(y) traverses y's expression tree
//   2. Reconstructs actual RandomVar objects for discovered params
//   3. Types match exactly - samples[alpha] just works!
//
// ============================================================================

namespace tempura::symbolic4 {

namespace infer_detail {

// Check if any type in pack is an IndexedRandomVar
template <typename... Ts>
constexpr bool any_indexed_v = (IsIndexedRandomVar<Ts> || ...);

// Build posterior from discovered params (tuple) and observed RVs
template <typename LogProbExpr, typename DiscoveredTuple, typename... ObservedRVs>
auto buildPosteriorFromDiscovered(LogProbExpr joint_lp,
                                   DiscoveredTuple discovered,
                                   const ObservedRVs&... observed) {
  // Expand discovered tuple and combine with observed RVs
  return std::apply(
      [&joint_lp, &observed...](const auto&... disc_rvs) {
        // Pass discovered RVs first, then observed RVs
        // The observed RVs must be included so bind() can mark them as observed
        return makePlateTransformedPosterior(joint_lp, disc_rvs..., observed...);
      },
      discovered);
}

}  // namespace infer_detail

// ============================================================================
// infer(observed...) - Main API with automatic parameter discovery
// ============================================================================
//
// Pass only the OBSERVED random variables. Latent parameters are discovered
// automatically by traversing the expression tree.
//
// The discovered parameters are reconstructed as actual RandomVar objects,
// so samples[alpha] works because types match exactly.
//
// Usage:
//   auto y = plate<Obs>(normal(alpha + beta * x, sigma));
//   auto posterior = infer(y).bind(x = x_data, y = y_data);

template <HasLogProb... ObservedRVs>
auto infer(const ObservedRVs&... observed) {
  // Discover latent parameters - returns tuple of actual RandomVar objects!
  auto discovered = discoverParams(observed...);

  // Build joint log-prob including observed RVs and discovered params
  auto joint_lp = collectLogProbs(observed...);

  // Check if we have any discovered params
  if constexpr (std::tuple_size_v<decltype(discovered)> == 0) {
    // No latent params discovered - just use the observed RVs
    return makePlateTransformedPosterior(joint_lp, observed...);
  } else {
    // Build posterior from discovered params + observed RVs
    return infer_detail::buildPosteriorFromDiscovered(
        joint_lp, discovered, observed...);
  }
}

// ============================================================================
// inferExplicit(rv1, rv2, ...) - Legacy API with explicit parameter listing
// ============================================================================
//
// For cases where you want to explicitly list all parameters.
// All RVs are treated as parameters (no distinction between observed/latent).

template <typename... RVs>
  requires(infer_detail::any_indexed_v<RVs...>)
auto inferExplicit(const RVs&... rvs) {
  auto joint_lp = collectLogProbs(rvs...);
  return makePlateTransformedPosterior(joint_lp, rvs...);
}

template <typename... RVs>
  requires(IsRandomVar<RVs> && ...) && (!infer_detail::any_indexed_v<RVs...>)
constexpr auto inferExplicit(const RVs&... rvs) {
  auto joint_lp = collectLogProbs(rvs...);
  return makeAutoTransformedPosterior(joint_lp, rvs...);
}

// ============================================================================
// inferRaw(rv1, rv2, ...) - Create raw posterior without transforms
// ============================================================================
//
// For testing/debugging: operates in constrained space, no Jacobian.
// NOT suitable for HMC - use infer() instead.

template <typename LogProbExpr, typename ParamsTuple, typename GradsTuple>
class RawPosterior {
 public:
  static constexpr std::size_t NumParams = std::tuple_size_v<ParamsTuple>;

  constexpr RawPosterior(LogProbExpr lp, ParamsTuple params, GradsTuple grads)
      : lp_{lp}, params_{params}, grads_{grads} {}

  template <typename... Values>
    requires(sizeof...(Values) == NumParams)
  double logProb(Values... values) const {
    return logProbImpl(std::make_index_sequence<NumParams>{}, values...);
  }

  double logProb(std::span<const double> values) const {
    return logProbFromSpanImpl(values, std::make_index_sequence<NumParams>{});
  }

  template <std::size_t N>
    requires(N == NumParams)
  double logProb(const std::array<double, N>& values) const {
    return logProb(std::span<const double>{values});
  }

  template <typename... Values>
    requires(sizeof...(Values) == NumParams)
  std::array<double, NumParams> gradient(Values... values) const {
    return gradientImpl(std::make_index_sequence<NumParams>{}, values...);
  }

  std::array<double, NumParams> gradient(std::span<const double> values) const {
    return gradientFromSpanImpl(values, std::make_index_sequence<NumParams>{});
  }

  template <std::size_t N>
    requires(N == NumParams)
  std::array<double, NumParams> gradient(const std::array<double, N>& values) const {
    return gradient(std::span<const double>{values});
  }

  static constexpr std::size_t numParams() { return NumParams; }

 private:
  LogProbExpr lp_;
  ParamsTuple params_;
  GradsTuple grads_;

  template <std::size_t... Is, typename... Values>
  double logProbImpl(std::index_sequence<Is...>, Values... values) const {
    return evaluate(lp_, (std::get<Is>(params_) = values)...);
  }

  template <std::size_t... Is>
  double logProbFromSpanImpl(std::span<const double> values,
                             std::index_sequence<Is...>) const {
    return evaluate(lp_, (std::get<Is>(params_) = values[Is])...);
  }

  template <std::size_t... Is, typename... Values>
  std::array<double, NumParams> gradientImpl(std::index_sequence<Is...>,
                                             Values... values) const {
    return {evaluate(std::get<Is>(grads_), (std::get<Is>(params_) = values)...)...};
  }

  template <std::size_t... Is>
  std::array<double, NumParams> gradientFromSpanImpl(
      std::span<const double> values, std::index_sequence<Is...>) const {
    return {evaluate(std::get<Is>(grads_), (std::get<Is>(params_) = values[Is])...)...};
  }
};

template <IsRandomVar... RVs>
constexpr auto inferRaw(const RVs&... rvs) {
  auto joint_lp = collectLogProbs(rvs...);
  // Use RandomVars for params: their operator= applies inverse transform,
  // ensuring constrained-space values round-trip correctly through eval.
  auto params = std::make_tuple(rvs...);
  auto grads = std::make_tuple(simplify(diff(joint_lp, rvs.freeSym()))...);
  return RawPosterior{joint_lp, params, grads};
}

template <typename LogProbExpr, typename ParamsTuple, typename GradsTuple>
using SimplePosterior = RawPosterior<LogProbExpr, ParamsTuple, GradsTuple>;

}  // namespace tempura::symbolic4
