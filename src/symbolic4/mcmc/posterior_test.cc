// Tests for symbolic4/mcmc/posterior.h
#include "symbolic4/mcmc/mcmc.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Basic Posterior construction
  // ===========================================================================

  "Posterior with single parameter"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));
    auto posterior = makePosterior(logProb(mu), mu).build();

    double lp = posterior.logProb(0.5);
    // log N(0.5 | 0, 1) = -0.5 * 0.25 - log(1) - 0.9189385332046727
    double expected = -0.5 * 0.25 - 0.9189385332046727;
    expectNear(expected, lp, 1e-10);
  };

  "Posterior gradient with single parameter"_test = [] {
    auto mu = normal(lit(0.0), lit(1.0));
    auto posterior = makePosterior(logProb(mu), mu).build();

    auto grad = posterior.gradient(0.5);
    // d/d_mu [-0.5 * mu^2] = -mu = -0.5
    expectNear(-0.5, grad[0], 1e-10);
  };

  "Posterior with two parameters"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto sigma = halfNormal(lit(5.0));

    auto lp_expr = logProb(mu, sigma);
    auto posterior = makePosterior(lp_expr, mu, sigma).build();

    double lp = posterior.logProb(1.0, 2.0);
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradient(1.0, 2.0);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));
  };

  // ===========================================================================
  // Posterior with observations
  // ===========================================================================

  "Posterior with observed data"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto y = normal(mu, lit(1.0));

    // Observe y = 3.0
    auto posterior = makePosterior(logProb(mu, y), mu).observe(y = 3.0);

    double lp = posterior.logProb(2.0);

    // log N(2 | 0, 10) + log N(3 | 2, 1)
    double lp_mu = -0.5 * (2.0 / 10.0) * (2.0 / 10.0) - std::log(10.0) - 0.9189385332046727;
    double lp_y = -0.5 * 1.0 * 1.0 - std::log(1.0) - 0.9189385332046727;
    double expected = lp_mu + lp_y;

    expectNear(expected, lp, 1e-10);
  };

  "Posterior gradient with observed data"_test = [] {
    auto mu = normal(lit(0.0), lit(10.0));
    auto y = normal(mu, lit(1.0));

    auto posterior = makePosterior(logProb(mu, y), mu).observe(y = 3.0);

    auto grad = posterior.gradient(2.0);

    // d/d_mu [log N(mu|0,10) + log N(y|mu,1)]
    // = -mu/100 + (y - mu)
    // = -2/100 + (3 - 2) = -0.02 + 1 = 0.98
    double expected = -2.0 / 100.0 + (3.0 - 2.0);
    expectNear(expected, grad[0], 1e-10);
  };

  // ===========================================================================
  // Two-parameter posterior with observation
  // ===========================================================================

  "Two-parameter posterior with observation"_test = [] {
    auto mu = normal(lit(0.0), lit(5.0));
    auto sigma = halfNormal(lit(2.0));
    auto y = normal(mu, sigma);

    auto posterior = makePosterior(logProb(mu, sigma, y), mu, sigma).observe(y = 3.5);

    double lp = posterior.logProb(2.0, 1.5);
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradient(2.0, 1.5);
    expectTrue(std::isfinite(grad[0]));
    expectTrue(std::isfinite(grad[1]));
    expectEq(grad.size(), 2UL);
  };

  // ===========================================================================
  // Verify gradient correctness via finite differences
  // ===========================================================================

  "Gradient matches finite differences"_test = [] {
    auto mu = normal(lit(0.0), lit(5.0));
    auto sigma = halfNormal(lit(2.0));
    auto y = normal(mu, sigma);

    auto posterior = makePosterior(logProb(mu, sigma, y), mu, sigma).observe(y = 3.5);

    constexpr double h = 1e-5;
    double mu_val = 2.0;
    double sigma_val = 1.5;

    // Analytic gradient
    auto grad = posterior.gradient(mu_val, sigma_val);

    // Finite difference for mu
    double lp_plus = posterior.logProb(mu_val + h, sigma_val);
    double lp_minus = posterior.logProb(mu_val - h, sigma_val);
    double fd_mu = (lp_plus - lp_minus) / (2.0 * h);

    // Finite difference for sigma
    lp_plus = posterior.logProb(mu_val, sigma_val + h);
    lp_minus = posterior.logProb(mu_val, sigma_val - h);
    double fd_sigma = (lp_plus - lp_minus) / (2.0 * h);

    expectNear(fd_mu, grad[0], 1e-5);
    expectNear(fd_sigma, grad[1], 1e-5);
  };

  return TestRegistry::result();
}
