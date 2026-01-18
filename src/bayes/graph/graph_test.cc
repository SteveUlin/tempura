#include "bayes/graph/graph.h"

#include <cmath>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes::graph;
using namespace tempura::symbolic3;

auto main() -> int {
  // ===========================================================================
  // Basic RandomVar Tests
  // ===========================================================================

  "Normal creates RandomVar with correct structure"_test = [] {
    auto mu = Normal(0.0, 10.0);

    // Should be a RandomVar
    static_assert(is_random_var<decltype(mu)>);

    // Should have no parents (root prior)
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(mu.parents())>> == 0);
  };

  "HalfNormal creates RandomVar"_test = [] {
    auto sigma = HalfNormal(5.0);

    static_assert(is_random_var<decltype(sigma)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(sigma.parents())>> == 0);
  };

  "RandomVar with RandomVar parent captures dependency"_test = [] {
    auto mu = Normal(0.0, 10.0);
    auto y = Normal(mu, 1.0);

    static_assert(is_random_var<decltype(y)>);
    // y has mu as parent
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(y.parents())>> == 1);
  };

  "RandomVar with two RandomVar parents captures both"_test = [] {
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    static_assert(is_random_var<decltype(y)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(y.parents())>> == 2);
  };

  // ===========================================================================
  // jointLogProb Tests
  // ===========================================================================

  "jointLogProb single root returns nodeLogProb"_test = [] {
    auto mu = Normal(0.0, 1.0);
    auto lp = jointLogProb(mu);

    // Evaluate at mu = 0 (mode of standard normal)
    double val = evaluate(lp, BinderPack{mu = 0.0});

    // logNormal(0, 0, 1) = -0.5 * 0 - log(1) - log(sqrt(2pi))
    //                    = -log(sqrt(2pi)) ≈ -0.9189
    expectNear(-0.9189, val, 0.001);
  };

  "jointLogProb traverses to parent"_test = [] {
    auto mu = Normal(0.0, 1.0);
    auto y = Normal(mu, 1.0);

    auto lp = jointLogProb(y);

    // Should include both mu's prior and y's likelihood
    double val = evaluate(lp, BinderPack{mu = 0.0, y = 0.0});

    // Two standard normals at their modes: 2 * (-log(sqrt(2pi)))
    expectNear(-1.8379, val, 0.001);
  };

  "jointLogProb deduplicates shared parents"_test = [] {
    auto mu = Normal(0.0, 1.0);
    // Each call site now gets a unique type automatically
    auto y1 = Normal(mu, 1.0);
    auto y2 = Normal(mu, 1.0);

    auto lp = jointLogProb(y1, y2);

    // mu should be counted once (deduplication), y1 and y2 each once = 3 terms
    double val = evaluate(lp, BinderPack{mu = 0.0, y1 = 0.0, y2 = 0.0});

    // Three standard normals at their modes: 3 * (-log(sqrt(2pi)))
    expectNear(-2.7568, val, 0.001);
  };

  "jointLogProb with diamond dependency deduplicates correctly"_test = [] {
    auto mu = Normal(0.0, 1.0);
    auto sigma = HalfNormal(1.0);
    // Both y1 and y2 depend on mu and sigma - now works with same params!
    auto y1 = Normal(mu, sigma);
    auto y2 = Normal(mu, sigma);

    auto lp = jointLogProb(y1, y2);

    // mu, sigma, y1, y2 = 4 nodes, each counted once
    double val =
        evaluate(lp, BinderPack{mu = 0.0, sigma = 1.0, y1 = 0.0, y2 = 0.0});

    // mu: logNormal(0, 0, 1) ≈ -0.9189
    // sigma: logHalfNormal(1, 1) = log(2) - log(sqrt(2pi)) - log(1) - 0.5*1
    // y1, y2: each logNormal(0, 0, 1) ≈ -0.9189
    double log_half_normal = std::log(2) - 0.9189 - 0.5;  // ≈ -0.7256
    double expected = -0.9189 + log_half_normal + (-0.9189) + (-0.9189);
    expectNear(expected, val, 0.01);
  };

  // ===========================================================================
  // Gradient Tests
  // ===========================================================================

  "diff computes gradient correctly"_test = [] {
    auto mu = Normal(0.0, 10.0);
    auto y = Normal(mu, 1.0);

    auto lp = jointLogProb(y);
    auto dlp_dmu = diff(lp, mu);

    // At mu=0, y=1:
    // lp = logNormal(mu, 0, 10) + logNormal(y, mu, 1)
    // d/dmu logNormal(mu, 0, 10) = -mu/100 = 0
    // d/dmu logNormal(y, mu, 1) = (y - mu) = 1
    // Total: 1
    double grad = evaluate(dlp_dmu, BinderPack{mu = 0.0, y = 1.0});
    expectNear(1.0, grad, 0.001);
  };

  "diff with respect to sigma"_test = [] {
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    auto lp = jointLogProb(y);
    auto dlp_dsigma = diff(lp, sigma);

    // Gradient exists and is finite
    double grad =
        evaluate(dlp_dsigma, BinderPack{mu = 0.0, sigma = 1.0, y = 0.0});
    expectTrue(std::isfinite(grad));
  };

  // ===========================================================================
  // DeterministicVar Tests
  // ===========================================================================

  "RandomVar arithmetic creates DeterministicVar"_test = [] {
    // Each call site now gets a unique type automatically
    auto alpha = Normal(0.0, 10.0);
    auto beta = Normal(0.0, 10.0);

    auto y_hat = alpha + beta;

    static_assert(is_deterministic_var<decltype(y_hat)>);
    // y_hat depends on both alpha and beta
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(y_hat.parents())>> == 2);
  };

  "DeterministicVar with scalar"_test = [] {
    auto alpha = Normal(0.0, 10.0);

    auto y_hat = alpha * 2.0 + 1.0;

    static_assert(is_deterministic_var<decltype(y_hat)>);
  };

  "DeterministicVar in distribution"_test = [] {
    // Each call site now gets a unique type automatically
    auto alpha = Normal(0.0, 10.0);
    auto beta = Normal(0.0, 10.0);
    auto sigma = HalfNormal(1.0);

    auto y_hat = alpha + beta * 2.0;
    auto y = Normal(y_hat, sigma);

    static_assert(is_random_var<decltype(y)>);

    auto lp = jointLogProb(y);

    // Evaluate - y_hat is a symbol that needs binding
    double val = evaluate(
        lp,
        BinderPack{alpha = 1.0, beta = 2.0, sigma = 1.0, y_hat = 5.0, y = 5.0});
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Binding Syntax Tests
  // ===========================================================================

  "RandomVar operator= creates binder"_test = [] {
    auto mu = Normal(0.0, 1.0);
    auto lp = jointLogProb(mu);

    // mu = 0.0 should work in BinderPack
    double val = evaluate(lp, BinderPack{mu = 0.0});
    expectTrue(std::isfinite(val));
  };

  "Multiple bindings work"_test = [] {
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    auto lp = jointLogProb(y);

    // All three bindings
    double val = evaluate(lp, BinderPack{mu = 2.0, sigma = 1.5, y = 2.5});
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Various Distributions
  // ===========================================================================

  "Beta distribution"_test = [] {
    auto p = Beta(2.0, 5.0);
    auto lp = jointLogProb(p);

    double val = evaluate(lp, BinderPack{p = 0.3});
    expectTrue(std::isfinite(val));
  };

  "Exponential distribution"_test = [] {
    auto x = Exponential(2.0);
    auto lp = jointLogProb(x);

    double val = evaluate(lp, BinderPack{x = 1.0});
    // logExponential(1, 2) = log(2) - 2*1 = 0.693 - 2 = -1.307
    expectNear(-1.307, val, 0.01);
  };

  "Gamma distribution"_test = [] {
    auto x = Gamma(2.0, 1.0);
    auto lp = jointLogProb(x);

    double val = evaluate(lp, BinderPack{x = 1.0});
    expectTrue(std::isfinite(val));
  };

  "Bernoulli distribution"_test = [] {
    auto p_prior = Beta(1.0, 1.0);
    auto x = Bernoulli(p_prior);

    auto lp = jointLogProb(x);

    double val = evaluate(lp, BinderPack{p_prior = 0.7, x = 1.0});
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Integration Example: Simple Bayesian Model
  // ===========================================================================

  "Complete Bayesian model workflow"_test = [] {
    // Define model
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    // Get joint log-prob
    auto lp = jointLogProb(y);

    // Compute gradients
    auto dlp_dmu = diff(lp, mu);
    auto dlp_dsigma = diff(lp, sigma);

    // Evaluate at a point
    auto bindings = BinderPack{mu = 2.0, sigma = 1.5, y = 2.5};

    double log_prob = evaluate(lp, bindings);
    double grad_mu = evaluate(dlp_dmu, bindings);
    double grad_sigma = evaluate(dlp_dsigma, bindings);

    expectTrue(std::isfinite(log_prob));
    expectTrue(std::isfinite(grad_mu));
    expectTrue(std::isfinite(grad_sigma));
  };

  "IID observations via loop"_test = [] {
    auto mu = Normal(0.0, 10.0);
    auto sigma = HalfNormal(5.0);
    auto y = Normal(mu, sigma);

    // Prior log-prob only
    auto prior_lp = jointLogProb(mu) + jointLogProb(sigma);
    // Single observation term
    auto obs_lp = y.nodeLogProb();

    double data[] = {1.2, 2.5, 1.8};

    auto log_prob = [&](double mu_val, double sigma_val) {
      double total =
          evaluate(prior_lp, BinderPack{mu = mu_val, sigma = sigma_val});
      for (double yi : data) {
        total +=
            evaluate(obs_lp, BinderPack{mu = mu_val, sigma = sigma_val, y = yi});
      }
      return total;
    };

    double lp = log_prob(2.0, 1.0);
    expectTrue(std::isfinite(lp));
  };

  return TestRegistry::result();
}
