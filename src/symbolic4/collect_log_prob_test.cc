// Tests for collect_log_prob.h - auto-discovery of log-probabilities
#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/interpreter/eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Basic Tests - Single RV (no parents)
  // =========================================================================

  "collectLogProbs with no parent RVs"_test = [] {
    // mu has literal parameters, so no parent RVs to discover
    auto mu = normal(0.0_c, 10.0_c);
    auto collected = collectLogProbs(mu);
    auto manual = mu.logProb();

    // Should be equivalent
    double val_collected = evaluate(collected, mu = 1.0);
    double val_manual = evaluate(manual, mu = 1.0);
    expectNear(val_manual, val_collected, 1e-10);
  };

  // =========================================================================
  // Parent Discovery Tests
  // =========================================================================

  "collectLogProbs discovers parent RV"_test = [] {
    // mu is a parent of y
    auto mu = normal(0.0_c, 10.0_c);
    auto y = normal(mu, 1.0_c);

    auto collected = collectLogProbs(y);
    auto manual = logProb(mu, y);

    // Evaluate both
    double val_collected = evaluate(collected, mu = 0.5, y = 1.0);
    double val_manual = evaluate(manual, mu = 0.5, y = 1.0);
    expectNear(val_manual, val_collected, 1e-10);
  };

  "collectLogProbs discovers multiple parent RVs"_test = [] {
    auto mu = normal(0.0_c, 10.0_c);
    auto sigma = halfNormal(5.0_c);
    auto y = normal(mu, sigma);

    auto collected = collectLogProbs(y);
    auto manual = logProb(mu, sigma, y);

    double val_collected = evaluate(collected, mu = 1.0, sigma = 2.0, y = 1.5);
    double val_manual = evaluate(manual, mu = 1.0, sigma = 2.0, y = 1.5);
    expectNear(val_manual, val_collected, 1e-10);
  };

  // =========================================================================
  // Deduplication Tests
  // =========================================================================

  "collectLogProbs does not double-count shared RVs"_test = [] {
    // theta appears in both alpha and beta parameters of the likelihood
    auto theta = beta(1.0_c, 1.0_c);
    auto y = bernoulli(theta);

    // If theta is counted once, joint = log P(theta) + log P(y|theta)
    auto collected = collectLogProbs(y);
    auto manual = logProb(theta, y);

    double val_collected = evaluate(collected, theta = 0.3, y = 1.0);
    double val_manual = evaluate(manual, theta = 0.3, y = 1.0);
    expectNear(val_manual, val_collected, 1e-10);
  };

  // =========================================================================
  // Comparison with Manual logProb
  // =========================================================================

  "collectLogProbs matches manual logProb for hierarchical model"_test = [] {
    // Hierarchical model: alpha, beta -> theta -> y
    // (Simplified - in practice alpha/beta would be priors on hyperparameters)
    auto alpha = gamma(2.0_c, 0.1_c);
    auto beta_param = gamma(2.0_c, 0.1_c);
    auto theta = beta(alpha, beta_param);

    auto collected = collectLogProbs(theta);

    // Manual: must list alpha, beta_param, theta
    auto manual = logProb(alpha, beta_param, theta);

    double val_collected = evaluate(collected, alpha = 2.0, beta_param = 5.0, theta = 0.3);
    double val_manual = evaluate(manual, alpha = 2.0, beta_param = 5.0, theta = 0.3);
    expectNear(val_manual, val_collected, 1e-10);
  };

  return TestRegistry::result();
}
