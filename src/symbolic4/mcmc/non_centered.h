#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>

// ============================================================================
// non_centered.h - Non-centered parameterization helper for hierarchical models
// ============================================================================
//
// Non-centered parameterization reduces funnel geometry in hierarchical models:
//   param[j] = location + exp(log_scale) * z[j]
//   where z[j] ~ Normal(0, 1)
//
// This is mathematically equivalent to:
//   param[j] ~ Normal(location, scale)
//
// But samples z (standard normal) instead of param directly, which works better
// when scale is small (centered parameterization creates "funnels" that HMC
// struggles to navigate).
//
// The gradient through param[j] = location + scale * z[j]:
//   d(param[j])/d(location)  = 1
//   d(param[j])/d(log_scale) = scale * z[j]  (chain rule through exp)
//   d(param[j])/d(z[j])      = scale
//
// Usage (from bangladesh_contraception.cpp):
//   // State layout: [..., a_bar, log_sigma_a, z_a[0..N-1], ...]
//   auto ncp_a = nonCenteredParam<N>(location_idx, log_scale_idx, z_offset);
//
//   // In likelihood loop:
//   double a_j = ncp_a.param(state, j);  // = a_bar + sigma_a * z_a[j]
//
//   // In gradient computation:
//   ncp_a.addParamGrad(grad, j, d_eta, state);  // chain rule from d(loss)/d(a_j)
//   ncp_a.addZPriorGrad(grad, j, state);        // z_j ~ Normal(0,1) prior
//
// ============================================================================

namespace tempura::symbolic4 {

// Helper for non-centered hierarchical parameterization.
// N is the number of groups (e.g., districts in bangladesh_contraception.cpp).
template <std::size_t N>
struct NonCenteredParam {
  std::size_t location_idx;    // Index of location parameter in state
  std::size_t log_scale_idx;   // Index of log(scale) parameter in state
  std::size_t z_offset;        // Starting index of z[0..N-1] in state

  // Get scale = exp(log_scale) from state
  auto scale(std::span<const double> state) const -> double {
    assert(log_scale_idx < state.size());
    return std::exp(state[log_scale_idx]);
  }

  // Get param[j] = location + scale * z[j]
  auto param(std::span<const double> state, std::size_t j) const -> double {
    assert(j < N);
    assert(location_idx < state.size());
    assert(z_offset + j < state.size());
    return state[location_idx] + scale(state) * state[z_offset + j];
  }

  // Get z[j] from state
  auto z(std::span<const double> state, std::size_t j) const -> double {
    assert(j < N);
    assert(z_offset + j < state.size());
    return state[z_offset + j];
  }

  // Log-prior for z[j]: -0.5 * z[j]^2 (Standard Normal, unnormalized)
  auto zLogPrior(std::span<const double> state, std::size_t j) const -> double {
    double z_j = z(state, j);
    return -0.5 * z_j * z_j;
  }

  // Add gradient contribution from df/d(param[j]) to grad vector.
  // Updates grad[location_idx], grad[log_scale_idx], grad[z_offset + j].
  //
  // Given: param[j] = location + exp(log_scale) * z[j]
  //   d(param)/d(location)  = 1
  //   d(param)/d(log_scale) = exp(log_scale) * z[j] = scale * z[j]
  //   d(param)/d(z[j])      = scale
  void addParamGrad(std::span<double> grad, std::size_t j,
                    double df_dparam, std::span<const double> state) const {
    assert(j < N);
    assert(location_idx < grad.size());
    assert(log_scale_idx < grad.size());
    assert(z_offset + j < grad.size());

    double s = scale(state);
    double z_j = z(state, j);

    grad[location_idx] += df_dparam;                // d/d(location)
    grad[log_scale_idx] += df_dparam * s * z_j;     // d/d(log_scale)
    grad[z_offset + j] += df_dparam * s;            // d/d(z[j])
  }

  // Add z prior gradient: grad[z_offset + j] += -z[j]
  // From d/dz[-0.5 * z^2] = -z
  void addZPriorGrad(std::span<double> grad, std::size_t j,
                     std::span<const double> state) const {
    assert(j < N);
    assert(z_offset + j < grad.size());
    grad[z_offset + j] += -z(state, j);
  }
};

// Factory function
template <std::size_t N>
constexpr auto nonCenteredParam(std::size_t location_idx,
                                 std::size_t log_scale_idx,
                                 std::size_t z_offset) -> NonCenteredParam<N> {
  return NonCenteredParam<N>{location_idx, log_scale_idx, z_offset};
}

}  // namespace tempura::symbolic4
