#include "symbolic4/mcmc/hmc_adapter.h"

#include <cmath>
#include <random>

#include "symbolic4/distributions/joint.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/mcmc/transforms.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "HMC adapter basic functionality"_test = [] {
    // Model: mu ~ Normal(0, 5), sigma ~ HalfNormal(2), y ~ Normal(mu, sigma)
    // Observed: y = 3.5
    auto mu = normal(lit(0.0), lit(5.0));
    auto sigma = halfNormal(lit(2.0));
    auto y = normal(mu.sym(), sigma.sym());

    // Create transformed posterior: mu unconstrained, sigma positive
    auto posterior = makeTransformedPosterior(
        logProb(mu, sigma, y),
        unconstrained(mu),
        positive(sigma)
    ).observe(y = 3.5);

    // Create HMC adapter
    auto adapter = makeHMCFromPosterior<double>(posterior, 0.1, 10);

    // Test log-prob evaluation
    std::array<double, 2> z{0.0, 0.0};  // mu=0, sigma=exp(0)=1
    double lp = adapter.logProb(z);
    expectTrue(std::isfinite(lp));

    // Test gradient evaluation
    auto grad = adapter.gradient(z);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));

    // Test transform
    auto x = adapter.transform(z);
    expectNear(x[0], 0.0, 1e-10);  // mu unchanged
    expectNear(x[1], 1.0, 1e-10);  // sigma = exp(0) = 1
  };

  "HMC adapter sampling"_test = [] {
    // Simple model: x ~ Normal(2, 0.5)
    // Should recover mean near 2
    auto x = normal(lit(2.0), lit(0.5));
    auto posterior = makeTransformedPosterior(
        logProb(x),
        unconstrained(x)
    ).build();

    auto adapter = makeHMCFromPosterior<double>(posterior, 0.05, 20);

    std::mt19937_64 rng{42};
    std::array<double, 1> z{0.0};
    double lp = adapter.logProb(z);

    // Run some steps
    int accepted = 0;
    double sum = 0.0;
    for (int i = 0; i < 500; ++i) {
      auto [next_z, next_lp, was_accepted] = adapter.step(z, lp, rng);
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
    auto sigma = halfNormal(lit(2.0));
    auto posterior = makeTransformedPosterior(
        logProb(sigma),
        positive(sigma)
    ).build();

    auto adapter = makeHMCFromPosterior<double>(posterior, 0.1, 10);

    std::mt19937_64 rng{123};
    std::array<double, 1> z{0.0};  // z=0 -> sigma=1
    double lp = adapter.logProb(z);

    // Run some steps and verify all transformed values are positive
    for (int i = 0; i < 100; ++i) {
      auto [next_z, next_lp, accepted] = adapter.step(z, lp, rng);
      z = next_z;
      lp = next_lp;

      auto x = adapter.transform(z);
      expectTrue(x[0] > 0.0);  // sigma always positive
    }
  };

  "HMC adapter inverse transform"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));
    auto sigma = halfNormal(lit(1.0));

    auto posterior = makeTransformedPosterior(
        logProb(mu, sigma),
        unconstrained(mu),
        positive(sigma)
    ).build();

    auto adapter = makeHMCFromPosterior<double>(posterior, 0.1, 10);

    // Test round-trip: constrained -> unconstrained -> constrained
    std::array<double, 2> x_original{2.5, 1.5};
    auto z = adapter.inverse(x_original);
    auto x_recovered = adapter.transform(z);

    expectNear(x_recovered[0], x_original[0], 1e-10);
    expectNear(x_recovered[1], x_original[1], 1e-10);

    // Verify unconstrained values
    expectNear(z[0], 2.5, 1e-10);             // mu unchanged
    expectNear(z[1], std::log(1.5), 1e-10);   // log(sigma)
  };

  return TestRegistry::result();
}
