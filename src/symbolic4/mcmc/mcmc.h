#pragma once

// ============================================================================
// mcmc.h - MCMC integration for symbolic4
// ============================================================================
//
// Provides posterior wrapper for use with bayes/estimation MCMC samplers.
//
// Usage:
//   #include "symbolic4/mcmc/mcmc.h"
//   #include "bayes/estimation/hmc.h"
//
//   using namespace tempura::symbolic4;
//
//   // Define model
//   auto mu = normal(lit(0.0), lit(5.0));
//   auto sigma = halfNormal(lit(2.0));
//   auto y = normal(mu, sigma);
//
//   // Create posterior
//   auto posterior = makePosterior(logProb(mu, sigma, y), mu.sym(), sigma.sym())
//       .observe(y = 3.5);
//
//   // Use with HMC
//   auto log_target = [&](const std::array<double, 2>& state) {
//       return posterior.logProb(state[0], state[1]);
//   };
//   auto grad_log_target = [&](const std::array<double, 2>& state) {
//       return posterior.gradient(state[0], state[1]);
//   };
//
//   auto hmc = bayes::makeHMC<double, 2>(log_target, grad_log_target, 0.1, 10);
//
// ============================================================================

#include "symbolic4/mcmc/posterior.h"
