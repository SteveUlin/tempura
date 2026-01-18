#include "prob/log_prob.h"

#include <cmath>

#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/matching.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::prob;
using namespace tempura::symbolic3;

auto main() -> int {
  // ===========================================================================
  // Concrete (numeric) tests
  // ===========================================================================

  "logNormal concrete"_test = [] {
    // Standard normal at x=0 should give log(1/√(2π)) ≈ -0.9189
    double lp = logNormal(0.0, 0.0, 1.0);
    expectNear(lp, -kLogSqrt2Pi, 1e-10);

    // N(0,1) at x=1: -½(1)² - log(1) - log(√(2π)) = -0.5 - 0.9189 = -1.4189
    double lp2 = logNormal(1.0, 0.0, 1.0);
    expectNear(lp2, -0.5 - kLogSqrt2Pi, 1e-10);
  };

  "logHalfNormal concrete"_test = [] {
    // HalfNormal(1) at x=0: log(2) - log(√(2π)) - log(1) - 0
    double lp = logHalfNormal(0.0, 1.0);
    expectNear(lp, kLog2 - kLogSqrt2Pi, 1e-10);
  };

  "logExponential concrete"_test = [] {
    // Exp(1) at x=0: log(1) - 1*0 = 0
    double lp = logExponential(0.0, 1.0);
    expectNear(lp, 0.0, 1e-10);

    // Exp(2) at x=1: log(2) - 2*1 = log(2) - 2
    double lp2 = logExponential(1.0, 2.0);
    expectNear(lp2, std::log(2.0) - 2.0, 1e-10);
  };

  "logBeta concrete"_test = [] {
    // Beta(1,1) is Uniform(0,1), so log-prob is 0 everywhere
    double lp = logBeta(0.5, 1.0, 1.0);
    expectNear(lp, 0.0, 1e-10);
  };

  "logUniform concrete"_test = [] {
    // Uniform(0, 2) has density 0.5, log-prob = -log(2)
    double lp = logUniform(1.0, 0.0, 2.0);
    expectNear(lp, -std::log(2.0), 1e-10);
  };

  // ===========================================================================
  // Symbolic tests
  // ===========================================================================

  "logNormal symbolic structure"_test = [] {
    constexpr Symbol x, μ, σ;

    // Build symbolic log-probability
    constexpr auto lp = logNormal(x, μ, σ);

    // It should be a symbolic expression (not a concrete value)
    static_assert(Symbolic<decltype(lp)>);

    // We can differentiate it
    constexpr auto dlp_dx = diff(lp, x);
    static_assert(Symbolic<decltype(dlp_dx)>);

    // Gradient w.r.t. μ should also work
    constexpr auto dlp_dμ = diff(lp, μ);
    static_assert(Symbolic<decltype(dlp_dμ)>);
  };

  "logNormal symbolic evaluation"_test = [] {
    constexpr Symbol x, μ, σ;

    constexpr auto lp = logNormal(x, μ, σ);

    // Evaluate at concrete values
    auto bindings = BinderPack{x = 0.0, μ = 0.0, σ = 1.0};
    double result = evaluate(lp, bindings);

    // Should match concrete implementation
    expectNear(result, logNormal(0.0, 0.0, 1.0), 1e-10);
  };

  "logHalfNormal symbolic"_test = [] {
    constexpr Symbol x, σ;

    constexpr auto lp = logHalfNormal(x, σ);
    static_assert(Symbolic<decltype(lp)>);

    auto bindings = BinderPack{x = 0.0, σ = 1.0};
    double result = evaluate(lp, bindings);
    expectNear(result, logHalfNormal(0.0, 1.0), 1e-10);
  };

  "logExponential symbolic"_test = [] {
    constexpr Symbol x, λ;

    constexpr auto lp = logExponential(x, λ);
    static_assert(Symbolic<decltype(lp)>);

    auto bindings = BinderPack{x = 1.0, λ = 2.0};
    double result = evaluate(lp, bindings);
    expectNear(result, logExponential(1.0, 2.0), 1e-10);
  };

  "logCauchy symbolic"_test = [] {
    constexpr Symbol x, x0, γ;

    constexpr auto lp = logCauchy(x, x0, γ);
    static_assert(Symbolic<decltype(lp)>);

    auto bindings = BinderPack{x = 0.0, x0 = 0.0, γ = 1.0};
    double result = evaluate(lp, bindings);
    expectNear(result, logCauchy(0.0, 0.0, 1.0), 1e-10);
  };

  // ===========================================================================
  // Gradient tests - verify symbolic derivatives are correct
  // ===========================================================================

  "logNormal gradient correctness"_test = [] {
    constexpr Symbol x;
    constexpr Symbol μ;
    constexpr Symbol σ;

    constexpr auto lp = logNormal(x, μ, σ);

    // ∂/∂x log p(x|μ,σ) = -(x-μ)/σ²
    constexpr auto dlp_dx = diff(lp, x);

    // Verify we get a symbolic expression
    static_assert(Symbolic<decltype(dlp_dx)>);

    // Evaluate at x=1, μ=0, σ=2: gradient = -(1-0)/4 = -0.25
    auto bindings = BinderPack{x = 1.0, μ = 0.0, σ = 2.0};
    double grad = evaluate(dlp_dx, bindings);
    expectNear(grad, -0.25, 1e-10);
  };

  "model composition"_test = [] {
    // Test that we can compose multiple log-probabilities
    constexpr Symbol μ, σ, y;

    // Prior: μ ~ Normal(0, 10)
    constexpr auto log_prior_μ = logNormal(μ, 0_c, 10_c);

    // Prior: σ ~ HalfNormal(5)
    constexpr auto log_prior_σ = logHalfNormal(σ, 5_c);

    // Likelihood: y ~ Normal(μ, σ)
    constexpr auto log_likelihood = logNormal(y, μ, σ);

    // Joint log-probability
    constexpr auto log_joint = log_prior_μ + log_prior_σ + log_likelihood;

    static_assert(Symbolic<decltype(log_joint)>);

    // Can compute gradients of the joint
    constexpr auto grad_μ = diff(log_joint, μ);
    constexpr auto grad_σ = diff(log_joint, σ);

    static_assert(Symbolic<decltype(grad_μ)>);
    static_assert(Symbolic<decltype(grad_σ)>);
  };

  return TestRegistry::result();
}
