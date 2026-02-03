#pragma once

// ============================================================================
// distributions.h - Probability distribution support for symbolic4
// ============================================================================
//
// Umbrella header providing:
//   - Symbolic log-probability functions (logNormal, logBeta, etc.)
//   - Distribution wrappers (NormalDist, BetaDist, etc.)
//   - Random variable nodes (StochasticNode, normal(), beta(), etc.)
//   - Joint log-probability computation (jointLogProb)
//   - DAG sampling (sampleTrace)
//
// Example:
//   using namespace tempura::symbolic4;
//
//   // Define hierarchical model
//   auto mu = normal(0.0, 10.0);        // mu ~ Normal(0, 10)
//   auto sigma = halfNormal(5.0);       // sigma ~ HalfNormal(5)
//   auto y = normal(mu, sigma);         // y ~ Normal(mu, sigma)
//
//   // Symbolic log-probability (differentiable)
//   auto lp = jointLogProb(y);
//   auto grad_mu = diff(lp, mu.sym());
//
//   // Evaluate at specific values
//   double val = evaluate(lp, mu = 1.0, sigma = 2.0, y = 1.5);
//
//   // Sample from prior
//   std::mt19937 rng{42};
//   auto trace = sampleTrace(y, rng);
//   double mu_sample = trace[mu];
//
// ============================================================================

#include "symbolic4/distributions/log_prob.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/distributions/joint.h"
#include "symbolic4/distributions/sample.h"
