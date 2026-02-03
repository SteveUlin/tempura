#pragma once

#include <cstddef>

// ============================================================================
// sampler.h - HMC configuration
// ============================================================================
//
// Configuration structs for MCMC samplers.
//
// Usage:
//   auto samples = posterior.sample(
//       HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 1000},
//       (alpha = 0.0, beta = 0.0, sigma = 1.0),
//       rng);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// HmcConfig - Configuration for Hamiltonian Monte Carlo
// ============================================================================

struct HmcConfig {
  double epsilon = 0.01;       // Leapfrog step size
  std::size_t steps = 20;      // Leapfrog steps per proposal
  std::size_t warmup = 500;    // Warmup iterations (discarded)
  std::size_t draws = 1000;    // Post-warmup samples to collect
};

}  // namespace tempura::symbolic4
