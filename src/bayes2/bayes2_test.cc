#include "bayes2/bayes2.h"

#include <cmath>
#include <random>

#include "autodiff/dual.h"
#include "prob/log_prob.h"
#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes2;
using namespace tempura::symbolic3;
using namespace tempura::autodiff;

auto main() -> int {
  // ===========================================================================
  // Basic RandomVar Tests
  // ===========================================================================

  "normal creates RandomVar with correct structure"_test = [] {
    auto mu = normal(0.0, 10.0);

    static_assert(is_random_var<decltype(mu)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(mu.parents())>> == 0);
  };

  "halfNormal creates RandomVar"_test = [] {
    auto sigma = halfNormal(5.0);

    static_assert(is_random_var<decltype(sigma)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(sigma.parents())>> == 0);
  };

  "RandomVar with RandomVar parent captures dependency"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto y = normal(mu, 1.0);

    static_assert(is_random_var<decltype(y)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(y.parents())>> == 1);
  };

  "RandomVar with two RandomVar parents captures both"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);

    static_assert(is_random_var<decltype(y)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(y.parents())>> == 2);
  };

  // ===========================================================================
  // jointLogProb Tests
  // ===========================================================================

  "jointLogProb single root returns nodeLogProb"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto lp = jointLogProb(mu);

    double val = evaluate(lp, BinderPack{mu = 0.0});

    // lognormal(0, 0, 1) = -log(sqrt(2pi)) ≈ -0.9189
    expectNear(-0.9189, val, 0.001);
  };

  "jointLogProb traverses to parent"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y = normal(mu, 1.0);

    auto lp = jointLogProb(y);
    double val = evaluate(lp, BinderPack{mu = 0.0, y = 0.0});

    // Two standard normals at their modes
    expectNear(-1.8379, val, 0.001);
  };

  "jointLogProb deduplicates shared parents"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y1 = normal(mu, 1.0);
    auto y2 = normal(mu, 1.0);

    auto lp = jointLogProb(y1, y2);

    // mu counted once, y1 and y2 each once = 3 terms
    double val = evaluate(lp, BinderPack{mu = 0.0, y1 = 0.0, y2 = 0.0});
    expectNear(-2.7568, val, 0.001);
  };

  "jointLogProb with diamond dependency"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto sigma = halfNormal(1.0);
    auto y1 = normal(mu, sigma);
    auto y2 = normal(mu, sigma);

    auto lp = jointLogProb(y1, y2);

    double val =
        evaluate(lp, BinderPack{mu = 0.0, sigma = 1.0, y1 = 0.0, y2 = 0.0});

    // mu, sigma, y1, y2 = 4 nodes
    double log_half_normal = std::log(2) - 0.9189 - 0.5;
    double expected = -0.9189 + log_half_normal + (-0.9189) + (-0.9189);
    expectNear(expected, val, 0.01);
  };

  // ===========================================================================
  // Gradient Tests
  // ===========================================================================

  "diff computes gradient correctly"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto y = normal(mu, 1.0);

    auto lp = jointLogProb(y);
    auto dlp_dmu = diff(lp, mu);

    // At mu=0, y=1: gradient = (y - mu) - mu/100 = 1 - 0 = 1
    double grad = evaluate(dlp_dmu, BinderPack{mu = 0.0, y = 1.0});
    expectNear(1.0, grad, 0.001);
  };

  // ===========================================================================
  // jointLogProb with Dual Numbers (Forward-Mode Autodiff)
  // ===========================================================================

  "jointLogProb with Dual numbers - single parameter"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto lp = jointLogProb(mu);

    // Evaluate with Dual to get gradient w.r.t. mu
    Dual<double> mu_dual{0.0, 1.0};  // seed gradient = 1
    auto result = evaluate(lp, BinderPack{mu = mu_dual});

    // Value should match double evaluation
    expectNear(-0.9189, result.value, 0.001);

    // Gradient ∂log p/∂mu = -mu/σ² = 0 at mu=0
    expectNear(0.0, result.gradient, 1e-10);
  };

  "jointLogProb with Dual numbers - hierarchical model"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto y = normal(mu, 1.0);
    auto lp = jointLogProb(y);

    // Seed mu to get gradient
    Dual<double> mu_dual{0.0, 1.0};
    Dual<double> y_dual{1.0, 0.0};
    auto result = evaluate(lp, BinderPack{mu = mu_dual, y = y_dual});

    // Compare with symbolic gradient
    auto dlp_dmu = diff(lp, mu);
    double symbolic_grad = evaluate(dlp_dmu, BinderPack{mu = 0.0, y = 1.0});

    expectNear(symbolic_grad, result.gradient, 1e-10);
  };

  "jointLogProb Dual matches symbolic for complete model"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);
    auto lp = jointLogProb(y);

    // Test point
    double mu_val = 2.0, sigma_val = 1.5, y_val = 2.5;

    // Symbolic gradients
    auto dlp_dmu = diff(lp, mu);
    auto dlp_dsigma = diff(lp, sigma);
    auto bindings = BinderPack{mu = mu_val, sigma = sigma_val, y = y_val};
    double sym_grad_mu = evaluate(dlp_dmu, bindings);
    double sym_grad_sigma = evaluate(dlp_dsigma, bindings);

    // Dual gradient w.r.t. mu
    auto result_mu = evaluate(
        lp, BinderPack{mu = Dual{mu_val, 1.0}, sigma = Dual{sigma_val, 0.0},
                       y = Dual{y_val, 0.0}});
    expectNear(sym_grad_mu, result_mu.gradient, 1e-10);

    // Dual gradient w.r.t. sigma
    auto result_sigma = evaluate(
        lp, BinderPack{mu = Dual{mu_val, 0.0}, sigma = Dual{sigma_val, 1.0},
                       y = Dual{y_val, 0.0}});
    expectNear(sym_grad_sigma, result_sigma.gradient, 1e-10);
  };

  "jointLogProb Dual with Beta distribution"_test = [] {
    auto p = beta(2.0, 5.0);
    auto lp = jointLogProb(p);

    // Evaluate with Dual
    Dual<double> p_dual{0.3, 1.0};
    auto result = evaluate(lp, BinderPack{p = p_dual});

    // Value should match
    double expected_val = evaluate(lp, BinderPack{p = 0.3});
    expectNear(expected_val, result.value, 1e-10);

    // Gradient should be finite (involves digamma via lgamma derivative)
    expectTrue(std::isfinite(result.gradient));
  };

  "jointLogProb Dual with Gamma distribution"_test = [] {
    auto x = gamma(3.0, 2.0);
    auto lp = jointLogProb(x);

    Dual<double> x_dual{1.0, 1.0};
    auto result = evaluate(lp, BinderPack{x = x_dual});

    double expected_val = evaluate(lp, BinderPack{x = 1.0});
    expectNear(expected_val, result.value, 1e-10);
    expectTrue(std::isfinite(result.gradient));
  };

  // ===========================================================================
  // DeterministicVar Tests
  // ===========================================================================

  "RandomVar arithmetic creates DeterministicVar"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 10.0);

    auto y_hat = alpha + beta;

    static_assert(is_deterministic_var<decltype(y_hat)>);
    static_assert(
        std::tuple_size_v<std::remove_cvref_t<decltype(y_hat.parents())>> == 2);
  };

  "DeterministicVar with scalar"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto y_hat = alpha * 2.0 + 1.0;

    static_assert(is_deterministic_var<decltype(y_hat)>);
  };

  "DeterministicVar in distribution"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 10.0);
    auto sigma = halfNormal(1.0);

    auto y_hat = alpha + beta * 2.0;
    auto y = normal(y_hat, sigma);

    static_assert(is_random_var<decltype(y)>);

    auto lp = jointLogProb(y);
    double val = evaluate(
        lp,
        BinderPack{alpha = 1.0, beta = 2.0, sigma = 1.0, y_hat = 5.0, y = 5.0});
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Various Distributions
  // ===========================================================================

  "beta distribution"_test = [] {
    auto p = beta(2.0, 5.0);
    auto lp = jointLogProb(p);

    double val = evaluate(lp, BinderPack{p = 0.3});
    expectTrue(std::isfinite(val));
  };

  "exponential distribution"_test = [] {
    auto x = exponential(2.0);
    auto lp = jointLogProb(x);

    double val = evaluate(lp, BinderPack{x = 1.0});
    expectNear(-1.307, val, 0.01);
  };

  "gamma distribution"_test = [] {
    auto x = gamma(2.0, 1.0);
    auto lp = jointLogProb(x);

    double val = evaluate(lp, BinderPack{x = 1.0});
    expectTrue(std::isfinite(val));
  };

  "bernoulli distribution"_test = [] {
    auto p_prior = beta(1.0, 1.0);
    auto x = bernoulli(p_prior);

    auto lp = jointLogProb(x);
    double val = evaluate(lp, BinderPack{p_prior = 0.7, x = 1.0});
    expectTrue(std::isfinite(val));
  };

  "binomial distribution"_test = [] {
    auto p = beta(2.0, 5.0);
    auto k = binomial(10.0, p);

    auto lp = jointLogProb(k);
    double val = evaluate(lp, BinderPack{p = 0.3, k = 3.0});

    // Manual calculation: log C(10,3) + 3*log(0.3) + 7*log(0.7)
    // log C(10,3) = log(120) ≈ 4.787
    // 3*log(0.3) ≈ -3.612
    // 7*log(0.7) ≈ -2.499
    // Total ≈ -1.324 (binomial) + beta prior
    expectTrue(std::isfinite(val));
  };

  "binomial with scalar n"_test = [] {
    auto k = binomial(20.0, 0.4);
    auto lp = jointLogProb(k);
    double val = evaluate(lp, BinderPack{k = 8.0});

    // Binomial(k=8 | n=20, p=0.4) should be reasonable
    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // Complete Workflow
  // ===========================================================================

  "Complete Bayesian model workflow"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);

    auto lp = jointLogProb(y);
    auto dlp_dmu = diff(lp, mu);
    auto dlp_dsigma = diff(lp, sigma);

    auto bindings = BinderPack{mu = 2.0, sigma = 1.5, y = 2.5};

    double log_prob = evaluate(lp, bindings);
    double grad_mu = evaluate(dlp_dmu, bindings);
    double grad_sigma = evaluate(dlp_dsigma, bindings);

    expectTrue(std::isfinite(log_prob));
    expectTrue(std::isfinite(grad_mu));
    expectTrue(std::isfinite(grad_sigma));
  };

  // ===========================================================================
  // Sampling Tests
  // ===========================================================================

  "sample normal with bindings"_test = [] {
    auto mu = normal(0.0, 1.0);
    std::mt19937 rng{42};

    // Sample 1000 times and check mean is reasonable
    double sum = 0.0;
    for (int i = 0; i < 1000; ++i) {
      sum += sample(mu, BinderPack{}, rng);
    }
    double mean = sum / 1000.0;

    // Standard error = 1/sqrt(1000) ≈ 0.032, so ±0.2 is ~6 SE
    expectNear(0.0, mean, 0.2);
  };

  "sample hierarchical model"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);

    std::mt19937 rng{42};
    auto bindings = BinderPack{mu = 2.0, sigma = 1.5};

    double sum = 0.0;
    for (int i = 0; i < 1000; ++i) {
      sum += sample(y, bindings, rng);
    }
    double mean = sum / 1000.0;

    // Should be near mu=2.0, SE = sigma/sqrt(N) = 1.5/31.6 ≈ 0.047
    expectNear(2.0, mean, 0.3);
  };

  "samplePrior from root node"_test = [] {
    auto mu = normal(5.0, 1.0);
    std::mt19937 rng{42};

    double sum = 0.0;
    for (int i = 0; i < 1000; ++i) {
      sum += samplePrior(mu, rng);
    }
    double mean = sum / 1000.0;

    expectNear(5.0, mean, 0.2);
  };

  "samplePrior from node with parents"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y = normal(mu, 0.1);
    std::mt19937 rng{42};

    // samplePrior should sample mu first, then y given mu
    double sum = 0.0;
    for (int i = 0; i < 1000; ++i) {
      sum += samplePrior(y, rng);
    }
    double mean = sum / 1000.0;

    // Marginal distribution of y has variance 1 + 0.01 ≈ 1, mean 0
    expectNear(0.0, mean, 0.2);
  };

  "sample different distributions"_test = [] {
    std::mt19937 rng{42};

    auto b = beta(2.0, 2.0);
    double beta_sample = sample(b, BinderPack{}, rng);
    expectTrue(beta_sample >= 0.0 && beta_sample <= 1.0);

    auto e = exponential(1.0);
    double exp_sample = sample(e, BinderPack{}, rng);
    expectTrue(exp_sample >= 0.0);

    auto u = uniform(0.0, 10.0);
    double unif_sample = sample(u, BinderPack{}, rng);
    expectTrue(unif_sample >= 0.0 && unif_sample <= 10.0);

    auto g = gamma(2.0, 1.0);
    double gamma_sample = sample(g, BinderPack{}, rng);
    expectTrue(gamma_sample >= 0.0);

    auto hn = halfNormal(2.0);
    double hn_sample = sample(hn, BinderPack{}, rng);
    expectTrue(hn_sample >= 0.0);
  };

  "sample bernoulli"_test = [] {
    auto p_prior = beta(1.0, 1.0);
    auto x = bernoulli(p_prior);
    std::mt19937 rng{42};

    auto bindings = BinderPack{p_prior = 0.7};
    int successes = 0;
    for (int i = 0; i < 1000; ++i) {
      if (sample(x, bindings, rng)) ++successes;
    }

    // Standard error ≈ sqrt(0.7 * 0.3 / 1000) ≈ 0.014
    expectNear(0.7, successes / 1000.0, 0.1);
  };

  // ===========================================================================
  // Trace Sampling Tests
  // ===========================================================================

  "sampleTrace root node"_test = [] {
    auto mu = normal(0.0, 1.0);
    std::mt19937 rng{42};

    auto trace = sampleTrace(mu, rng);

    // Can query by node
    double mu_val = trace[mu];
    expectTrue(std::isfinite(mu_val));
  };

  "sampleTrace hierarchical model"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);
    std::mt19937 rng{42};

    auto trace = sampleTrace(y, rng);

    // Can query all nodes in the DAG
    double mu_val = trace[mu];
    double sigma_val = trace[sigma];
    double y_val = trace[y];

    expectTrue(std::isfinite(mu_val));
    expectTrue(sigma_val > 0);  // half-normal is positive
    expectTrue(std::isfinite(y_val));
  };

  "sampleTrace values are consistent"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y = normal(mu, 0.1);  // small sigma so y ≈ mu
    std::mt19937 rng{42};

    // Sample many times and check y is close to mu
    double sum_diff = 0.0;
    for (int i = 0; i < 100; ++i) {
      auto trace = sampleTrace(y, rng);
      double mu_val = trace[mu];
      double y_val = trace[y];
      sum_diff += std::abs(y_val - mu_val);
    }

    // Average difference should be small (order of sigma = 0.1)
    expectTrue(sum_diff / 100.0 < 0.5);
  };

  "sampleTrace can be used as bindings for jointLogProb"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);
    std::mt19937 rng{42};

    auto lp = jointLogProb(y);
    auto trace = sampleTrace(y, rng);

    // Trace can be used directly as bindings for evaluate
    double log_prob = evaluate(lp, trace);
    expectTrue(std::isfinite(log_prob));
  };

  "sampleTrace get multiple values as tuple"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);
    std::mt19937 rng{42};

    auto trace = sampleTrace(y, rng);

    // Get multiple values as tuple
    auto [mu_val, sigma_val, y_val] = trace.get(mu, sigma, y);

    expectTrue(std::isfinite(mu_val));
    expectTrue(sigma_val > 0);
    expectTrue(std::isfinite(y_val));

    // Values should match individual lookups
    expectEq(mu_val, trace[mu]);
    expectEq(sigma_val, trace[sigma]);
    expectEq(y_val, trace[y]);
  };

  "sampleTrace deduplicates diamond dependencies"_test = [] {
    // Diamond: mu is parent to both y1 and y2
    //          mu
    //         /  \
    //        y1   y2
    // Without deduplication, mu would be sampled twice with different values
    auto mu = normal(0.0, 100.0);  // Wide prior to show any difference
    auto y1 = normal(mu, 0.001);   // Tiny sigma so y1 ≈ mu
    auto y2 = normal(mu, 0.001);   // Tiny sigma so y2 ≈ mu
    std::mt19937 rng{42};

    // Create a node that depends on both y1 and y2
    auto combined = y1 + y2;
    auto z = normal(combined, 0.001);

    auto trace = sampleTrace(z, rng);

    // Both y1 and y2 should be very close to mu (tiny sigma)
    double mu_val = trace[mu];
    double y1_val = trace[y1];
    double y2_val = trace[y2];

    // If deduplication works, both y1 and y2 use the SAME mu sample
    // So y1 ≈ y2 ≈ mu (within small tolerance due to tiny sigma)
    expectNear(mu_val, y1_val, 0.01);
    expectNear(mu_val, y2_val, 0.01);
    // This would fail without deduplication: y1 and y2 would have different mu values
    expectNear(y1_val, y2_val, 0.01);
  };

  // ===========================================================================
  // Reparameterization Tests
  // ===========================================================================

  "reparamNormal concrete"_test = [] {
    // z = μ + σ * ε
    double z = reparamNormal(2.0, 1.5, 0.5);
    expectNear(2.75, z, 1e-10);  // 2.0 + 1.5 * 0.5 = 2.75
  };

  "reparamNormal symbolic differentiation"_test = [] {
    // Create symbols for parameters and noise
    constexpr Symbol<struct MuTag> mu;
    constexpr Symbol<struct SigmaTag> sigma;
    constexpr Symbol<struct EpsTag> epsilon;

    // z = μ + σ * ε
    auto z = reparamNormal(mu, sigma, epsilon);

    // Differentiate w.r.t. parameters
    auto dz_dmu = diff(z, mu);
    auto dz_dsigma = diff(z, sigma);
    auto dz_deps = diff(z, epsilon);

    // Evaluate gradients
    auto bindings = BinderPack{mu = 2.0, sigma = 1.5, epsilon = 0.5};

    // ∂z/∂μ = 1
    expectNear(1.0, evaluate(dz_dmu, bindings), 1e-10);

    // ∂z/∂σ = ε = 0.5
    expectNear(0.5, evaluate(dz_dsigma, bindings), 1e-10);

    // ∂z/∂ε = σ = 1.5
    expectNear(1.5, evaluate(dz_deps, bindings), 1e-10);
  };

  "reparamExponential concrete"_test = [] {
    // x = -log(u) / λ
    double x = reparamExponential(2.0, 0.5);
    expectNear(std::log(2.0) / 2.0, x, 1e-10);  // -log(0.5) / 2 = log(2)/2
  };

  "reparamExponential symbolic differentiation"_test = [] {
    constexpr Symbol<struct LambdaTag> lambda;
    constexpr Symbol<struct UTag> u;

    auto x = reparamExponential(lambda, u);
    auto dx_dlambda = diff(x, lambda);

    auto bindings = BinderPack{lambda = 2.0, u = 0.5};

    // x = -log(u) / λ, so ∂x/∂λ = log(u) / λ² = log(0.5) / 4
    expectNear(std::log(0.5) / 4.0, evaluate(dx_dlambda, bindings), 1e-10);
  };

  "reparamUniform concrete"_test = [] {
    // x = a + (b - a) * u
    double x = reparamUniform(1.0, 5.0, 0.25);
    expectNear(2.0, x, 1e-10);  // 1 + (5-1) * 0.25 = 2
  };

  "reparamUniform symbolic differentiation"_test = [] {
    constexpr Symbol<struct ATag> a;
    constexpr Symbol<struct BTag> b;
    constexpr Symbol<struct UTag2> u;

    auto x = reparamUniform(a, b, u);
    auto dx_da = diff(x, a);
    auto dx_db = diff(x, b);

    auto bindings = BinderPack{a = 1.0, b = 5.0, u = 0.25};

    // x = a + (b - a) * u = a(1-u) + b*u
    // ∂x/∂a = 1 - u = 0.75
    expectNear(0.75, evaluate(dx_da, bindings), 1e-10);

    // ∂x/∂b = u = 0.25
    expectNear(0.25, evaluate(dx_db, bindings), 1e-10);
  };

  "reparamHalfNormal concrete"_test = [] {
    // z = σ * ε (ε assumed positive from half-normal)
    double z = reparamHalfNormal(2.0, 0.8);
    expectNear(1.6, z, 1e-10);
  };

  "reparamHalfNormal symbolic differentiation"_test = [] {
    constexpr Symbol<struct SigmaTag2> sigma;
    constexpr Symbol<struct EpsTag2> epsilon;

    auto z = reparamHalfNormal(sigma, epsilon);
    auto dz_dsigma = diff(z, sigma);

    auto bindings = BinderPack{sigma = 2.0, epsilon = 0.8};

    // ∂z/∂σ = ε = 0.8
    expectNear(0.8, evaluate(dz_dsigma, bindings), 1e-10);
  };

  "gradient flows through reparameterized sample to logProb"_test = [] {
    // This is the key test: gradient of log p(z) w.r.t. μ, σ
    // where z = μ + σ * ε

    constexpr Symbol<struct MuTag2> mu;
    constexpr Symbol<struct SigmaTag3> sigma;
    constexpr Symbol<struct EpsTag3> epsilon;

    // Reparameterized sample
    auto z = reparamNormal(mu, sigma, epsilon);

    // Log probability of z under some target distribution, say N(0, 1)
    auto lp = prob::logNormal(z, Literal{0.0}, Literal{1.0});

    // Gradients w.r.t. variational parameters
    auto dlp_dmu = diff(lp, mu);
    auto dlp_dsigma = diff(lp, sigma);

    // At μ=0, σ=1, ε=0.5: z = 0.5
    // log p(z) = -0.5 * z² - log(√2π)
    // ∂(log p)/∂z = -z = -0.5
    // ∂z/∂μ = 1, so ∂(log p)/∂μ = -z = -0.5
    // ∂z/∂σ = ε = 0.5, so ∂(log p)/∂σ = -z * ε = -0.25

    auto bindings = BinderPack{mu = 0.0, sigma = 1.0, epsilon = 0.5};

    expectNear(-0.5, evaluate(dlp_dmu, bindings), 1e-10);
    expectNear(-0.25, evaluate(dlp_dsigma, bindings), 1e-10);
  };

  return TestRegistry::result();
}
