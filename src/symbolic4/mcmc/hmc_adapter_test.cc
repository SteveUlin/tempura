#include <cmath>
#include <random>
#include <span>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/distributions/joint.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/infer.h"
#include "symbolic4/mcmc/plate_transforms.h"
#include "symbolic4/mcmc/samples.h"
#include "symbolic4/mcmc/sampler.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Dimension tag at namespace scope
namespace {
struct HmcTestCountries {};
}  // namespace

auto main() -> int {
  "HMC adapter basic functionality"_test = [] {
    // Model: mu ~ Normal(0, 5), sigma ~ HalfNormal(2), y ~ Normal(mu, sigma)
    // Observed: y = 3.5
    auto mu = normal(0.0_c, 5.0_c);
    auto sigma = halfNormal(2.0_c);
    auto y = normal(mu.sym(), sigma.sym());

    // makePlateTransformedPosterior auto-infers transforms from support
    auto posterior = makePlateTransformedPosterior(
        logProb(mu, sigma, y), mu, sigma
    ).observe(y = 3.5);

    std::size_t dim = posterior.stateDim();

    // Create dynamic HMC kernel wrapping the posterior
    auto hmc = bayes::makeHMCDynamic(
        [&](std::span<const double> s) { return posterior.logProb(s); },
        [&](std::span<const double> s) { return posterior.gradient(s); },
        0.1, 10, dim);

    // Test log-prob evaluation
    std::vector<double> z(dim, 0.0);  // mu=0, sigma=exp(0)=1
    double lp = hmc.logProb(z);
    expectTrue(std::isfinite(lp));

    // Test gradient evaluation
    auto grad = posterior.gradient(std::span<const double>(z));
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));

    // Test transform
    auto x = posterior.transform(std::span<const double>(z));
    expectNear(x[0], 0.0, 1e-10);  // mu unchanged
    expectNear(x[1], 1.0, 1e-10);  // sigma = exp(0) = 1
  };

  "HMC adapter sampling"_test = [] {
    // Simple model: x ~ Normal(2, 0.5)
    // Should recover mean near 2
    auto x = normal(2.0_c, 0.5_c);
    auto posterior = makePlateTransformedPosterior(
        logProb(x), x
    ).build();

    std::size_t dim = posterior.stateDim();

    auto hmc = bayes::makeHMCDynamic(
        [&](std::span<const double> s) { return posterior.logProb(s); },
        [&](std::span<const double> s) { return posterior.gradient(s); },
        0.05, 20, dim);

    std::mt19937_64 rng{42};
    std::vector<double> z(dim, 0.0);
    double lp = hmc.logProb(z);

    // Run some steps
    int accepted = 0;
    double sum = 0.0;
    for (int i = 0; i < 500; ++i) {
      auto [next_z, next_lp, was_accepted] = hmc.step(z, lp, rng);
      z = next_z;
      lp = next_lp;
      if (was_accepted) accepted++;
      if (i >= 100) {  // Skip warmup
        sum += z[0];
      }
    }

    double mean = sum / 400.0;
    // Mean should be near 2.0 (the prior mean)
    expectNear(mean, 2.0, 0.3);  // Allow some variance

    // Acceptance rate should be reasonable
    double accept_rate = static_cast<double>(accepted) / 500.0;
    expectTrue(accept_rate > 0.3);  // At least 30% acceptance
  };

  "HMC adapter with positive constraint"_test = [] {
    // Model: sigma ~ HalfNormal(2)
    // Should recover positive samples
    auto sigma = halfNormal(2.0_c);
    auto posterior = makePlateTransformedPosterior(
        logProb(sigma), sigma
    ).build();

    std::size_t dim = posterior.stateDim();

    auto hmc = bayes::makeHMCDynamic(
        [&](std::span<const double> s) { return posterior.logProb(s); },
        [&](std::span<const double> s) { return posterior.gradient(s); },
        0.1, 10, dim);

    std::mt19937_64 rng{123};
    std::vector<double> z(dim, 0.0);  // z=0 -> sigma=1
    double lp = hmc.logProb(z);

    // Run some steps and verify all transformed values are positive
    for (int i = 0; i < 100; ++i) {
      auto [next_z, next_lp, was_accepted] = hmc.step(z, lp, rng);
      z = next_z;
      lp = next_lp;

      auto x = posterior.transform(std::span<const double>(z));
      expectTrue(x[0] > 0.0);  // sigma always positive
    }
  };

  "HMC adapter inverse transform"_test = [] {
    auto mu = normal(0.0_c, 1.0_c);
    auto sigma = halfNormal(1.0_c);

    auto posterior = makePlateTransformedPosterior(
        logProb(mu, sigma), mu, sigma
    ).build();

    // Test round-trip: constrained -> unconstrained -> constrained
    std::vector<double> x_original{2.5, 1.5};
    auto z = posterior.inverse(std::span<const double>(x_original));
    auto x_recovered = posterior.transform(std::span<const double>(z));

    expectNear(x_recovered[0], x_original[0], 1e-10);
    expectNear(x_recovered[1], x_original[1], 1e-10);

    // Verify unconstrained values
    expectNear(z[0], 2.5, 1e-10);             // mu unchanged
    expectNear(z[1], std::log(1.5), 1e-10);   // log(sigma)
  };

  // =========================================================================
  // Mixed scalar + indexed params: samples[param] access
  // =========================================================================
  //
  // This is the key test case for the DynamicSamples symbol lookup bug:
  // When a model has both scalar params (a, sigma) and indexed params (z_b),
  // samples[a] should correctly find the scalar param in DynamicSamples.

  "sample() with mixed scalar+indexed params and symbol access"_test = [] {
    // Model similar to bmj_symbolic:
    // a ~ Normal(-2, 1)        (scalar)
    // sigma ~ Exponential(1)   (scalar)
    // z_b[i] ~ Normal(0, 1)    (indexed)
    auto a = normal(-2_c, 1_c);
    auto sigma = exponential(1_c);
    auto z_b = plate<HmcTestCountries>(normal(0.0_c, 1.0_c));

    // Joint log-prob (simplified - no likelihood for this test)
    auto joint_lp = collectLogProbs(a, sigma, z_b);

    auto posterior = makePlateTransformedPosterior(joint_lp, a, sigma, z_b)
                         .withDimension<HmcTestCountries>(3)
                         .build();

    // State dim: a (1) + sigma (1) + z_b (3) = 5
    expectEq(posterior.stateDim(), 5UL);

    // Run a few HMC steps to get samples
    std::mt19937_64 rng{42};
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.1, .steps = 10, .warmup = 10, .draws = 20},
        BinderPack{a = -2.0, sigma = 1.0, z_b = std::vector<double>{0.0, 0.0, 0.0}},
        rng);

    expectEq(samples.size(), 20UL);

    // KEY TEST: samples[a] should work for scalar params in mixed model
    auto a_draws = samples[a];
    expectEq(a_draws.size(), 20UL);
    expectTrue(std::isfinite(a_draws[0]));

    // samples[sigma] should also work
    auto sigma_draws = samples[sigma];
    expectEq(sigma_draws.size(), 20UL);
    expectTrue(sigma_draws[0] > 0.0);  // Positive constraint

    // samples[z_b] should return matrix (draws × groups)
    auto z_b_draws = samples[z_b];
    expectEq(z_b_draws.rows(), 20UL);
    expectEq(z_b_draws.cols(), 3UL);
    expectTrue(std::isfinite(z_b_draws[0, 0]));
  };

  return TestRegistry::result();
}
