#include "bayes2/reparam.h"

#include <cmath>
#include <numbers>
#include <random>

#include "autodiff/dual.h"
#include "prob/log_prob.h"
#include "symbolic3/constants.h"
#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes2;
using namespace tempura::prob;
using namespace tempura::symbolic3;
using namespace tempura::autodiff;

auto main() -> int {
  // ===========================================================================
  // NORMAL REPARAMETERIZATION: z = μ + σ * ε
  // ===========================================================================

  "reparamNormal double"_test = [] {
    // z = μ + σ * ε = 2.0 + 1.5 * 0.5 = 2.75
    expectNear(2.75, reparamNormal(2.0, 1.5, 0.5), 1e-10);

    // At ε = 0, z = μ
    expectNear(3.0, reparamNormal(3.0, 2.0, 0.0), 1e-10);

    // Negative epsilon
    expectNear(0.5, reparamNormal(2.0, 1.5, -1.0), 1e-10);
  };

  "reparamNormal symbolic"_test = [] {
    constexpr Symbol<struct MuN> mu;
    constexpr Symbol<struct SigmaN> sigma;
    constexpr Symbol<struct EpsN> epsilon;

    auto z = reparamNormal(mu, sigma, epsilon);
    static_assert(Symbolic<decltype(z)>);

    // Evaluate
    auto b = BinderPack{mu = 2.0, sigma = 1.5, epsilon = 0.5};
    expectNear(2.75, evaluate(z, b), 1e-10);

    // Gradients
    auto dz_dmu = diff(z, mu);
    auto dz_dsigma = diff(z, sigma);
    auto dz_deps = diff(z, epsilon);

    // ∂z/∂μ = 1
    expectNear(1.0, evaluate(dz_dmu, b), 1e-10);

    // ∂z/∂σ = ε = 0.5
    expectNear(0.5, evaluate(dz_dsigma, b), 1e-10);

    // ∂z/∂ε = σ = 1.5
    expectNear(1.5, evaluate(dz_deps, b), 1e-10);
  };

  "reparamNormal dual"_test = [] {
    // Seed mu
    Dual<double> mu{2.0, 1.0};
    Dual<double> sigma{1.5, 0.0};
    Dual<double> epsilon{0.5, 0.0};

    auto z = reparamNormal(mu, sigma, epsilon);
    expectNear(2.75, z.value, 1e-10);
    expectNear(1.0, z.gradient, 1e-10);  // ∂z/∂μ = 1

    // Seed sigma
    Dual<double> mu2{2.0, 0.0};
    Dual<double> sigma2{1.5, 1.0};
    auto z2 = reparamNormal(mu2, sigma2, epsilon);
    expectNear(0.5, z2.gradient, 1e-10);  // ∂z/∂σ = ε

    // Seed epsilon
    Dual<double> epsilon2{0.5, 1.0};
    auto z3 = reparamNormal(mu2, Dual<double>{1.5, 0.0}, epsilon2);
    expectNear(1.5, z3.gradient, 1e-10);  // ∂z/∂ε = σ
  };

  // ===========================================================================
  // HALF-NORMAL REPARAMETERIZATION: z = σ * ε (ε > 0)
  // ===========================================================================

  "reparamHalfNormal double"_test = [] {
    // z = σ * ε = 2.0 * 0.8 = 1.6
    expectNear(1.6, reparamHalfNormal(2.0, 0.8), 1e-10);

    // At ε = 1
    expectNear(2.0, reparamHalfNormal(2.0, 1.0), 1e-10);
  };

  "reparamHalfNormal symbolic"_test = [] {
    constexpr Symbol<struct SigmaH> sigma;
    constexpr Symbol<struct EpsH> epsilon;

    auto z = reparamHalfNormal(sigma, epsilon);
    static_assert(Symbolic<decltype(z)>);

    auto b = BinderPack{sigma = 2.0, epsilon = 0.8};
    expectNear(1.6, evaluate(z, b), 1e-10);

    // ∂z/∂σ = ε
    auto dz_dsigma = diff(z, sigma);
    expectNear(0.8, evaluate(dz_dsigma, b), 1e-10);

    // ∂z/∂ε = σ
    auto dz_deps = diff(z, epsilon);
    expectNear(2.0, evaluate(dz_deps, b), 1e-10);
  };

  "reparamHalfNormal dual"_test = [] {
    Dual<double> sigma{2.0, 1.0};
    Dual<double> epsilon{0.8, 0.0};

    auto z = reparamHalfNormal(sigma, epsilon);
    expectNear(1.6, z.value, 1e-10);
    expectNear(0.8, z.gradient, 1e-10);  // ∂z/∂σ = ε
  };

  // ===========================================================================
  // EXPONENTIAL REPARAMETERIZATION: x = -log(u) / λ
  // ===========================================================================

  "reparamExponential double"_test = [] {
    // x = -log(0.5) / 2 = log(2) / 2 ≈ 0.347
    expectNear(std::log(2.0) / 2.0, reparamExponential(2.0, 0.5), 1e-10);

    // u = e^(-1) gives x = 1/λ
    expectNear(0.5, reparamExponential(2.0, std::exp(-1.0)), 1e-10);
  };

  "reparamExponential symbolic"_test = [] {
    constexpr Symbol<struct LambdaE> lambda;
    constexpr Symbol<struct UE> u;

    auto x = reparamExponential(lambda, u);
    static_assert(Symbolic<decltype(x)>);

    auto b = BinderPack{lambda = 2.0, u = 0.5};
    expectNear(std::log(2.0) / 2.0, evaluate(x, b), 1e-10);

    // ∂x/∂λ = log(u) / λ² = log(0.5) / 4
    auto dx_dlambda = diff(x, lambda);
    expectNear(std::log(0.5) / 4.0, evaluate(dx_dlambda, b), 1e-10);

    // ∂x/∂u = -1 / (λ * u) = -1 / (2 * 0.5) = -1
    auto dx_du = diff(x, u);
    expectNear(-1.0, evaluate(dx_du, b), 1e-10);
  };

  "reparamExponential dual"_test = [] {
    Dual<double> lambda{2.0, 1.0};
    Dual<double> u{0.5, 0.0};

    auto x = reparamExponential(lambda, u);
    expectNear(std::log(2.0) / 2.0, x.value, 1e-10);
    expectNear(std::log(0.5) / 4.0, x.gradient, 1e-10);
  };

  // ===========================================================================
  // UNIFORM REPARAMETERIZATION: x = a + (b - a) * u
  // ===========================================================================

  "reparamUniform double"_test = [] {
    // x = 1 + (5 - 1) * 0.25 = 1 + 1 = 2
    expectNear(2.0, reparamUniform(1.0, 5.0, 0.25), 1e-10);

    // u = 0 gives x = a
    expectNear(1.0, reparamUniform(1.0, 5.0, 0.0), 1e-10);

    // u = 1 gives x = b
    expectNear(5.0, reparamUniform(1.0, 5.0, 1.0), 1e-10);
  };

  "reparamUniform symbolic"_test = [] {
    constexpr Symbol<struct AU> a;
    constexpr Symbol<struct BU> b;
    constexpr Symbol<struct UU> u;

    auto x = reparamUniform(a, b, u);
    static_assert(Symbolic<decltype(x)>);

    auto bindings = BinderPack{a = 1.0, b = 5.0, u = 0.25};
    expectNear(2.0, evaluate(x, bindings), 1e-10);

    // ∂x/∂a = 1 - u = 0.75
    auto dx_da = diff(x, a);
    expectNear(0.75, evaluate(dx_da, bindings), 1e-10);

    // ∂x/∂b = u = 0.25
    auto dx_db = diff(x, b);
    expectNear(0.25, evaluate(dx_db, bindings), 1e-10);

    // ∂x/∂u = b - a = 4
    auto dx_du = diff(x, u);
    expectNear(4.0, evaluate(dx_du, bindings), 1e-10);
  };

  "reparamUniform dual"_test = [] {
    Dual<double> a{1.0, 1.0};
    Dual<double> b{5.0, 0.0};
    Dual<double> u{0.25, 0.0};

    auto x = reparamUniform(a, b, u);
    expectNear(2.0, x.value, 1e-10);
    expectNear(0.75, x.gradient, 1e-10);  // ∂x/∂a = 1 - u
  };

  // ===========================================================================
  // CAUCHY REPARAMETERIZATION: x = x₀ + γ * tan(π * (u - 0.5))
  // ===========================================================================

  "reparamCauchy double"_test = [] {
    // At u = 0.5: tan(0) = 0, so x = x₀
    expectNear(2.0, reparamCauchy(2.0, 1.0, 0.5), 1e-10);

    // At u = 0.75: tan(π/4) = 1, so x = x₀ + γ
    expectNear(3.0, reparamCauchy(2.0, 1.0, 0.75), 1e-10);
  };

  "reparamCauchy symbolic"_test = [] {
    constexpr Symbol<struct LocC> location;
    constexpr Symbol<struct ScaleC> scale;
    constexpr Symbol<struct UC> u;

    auto x = reparamCauchy(location, scale, u);
    static_assert(Symbolic<decltype(x)>);

    auto b = BinderPack{location = 2.0, scale = 1.0, u = 0.5};
    expectNear(2.0, evaluate(x, b), 1e-10);

    // ∂x/∂location = 1
    auto dx_dloc = diff(x, location);
    expectNear(1.0, evaluate(dx_dloc, b), 1e-10);

    // ∂x/∂scale = tan(π(u - 0.5)) = 0 at u = 0.5
    auto dx_dscale = diff(x, scale);
    expectNear(0.0, evaluate(dx_dscale, b), 1e-10);
  };

  "reparamCauchy dual"_test = [] {
    Dual<double> location{2.0, 1.0};
    Dual<double> scale{1.0, 0.0};
    Dual<double> u{0.5, 0.0};

    auto x = reparamCauchy(location, scale, u);
    expectNear(2.0, x.value, 1e-10);
    expectNear(1.0, x.gradient, 1e-10);  // ∂x/∂location = 1
  };

  // ===========================================================================
  // GAMMA REPARAMETERIZATION (Marsaglia-Tsang for α ≥ 1)
  // ===========================================================================

  "reparamGamma double"_test = [] {
    // Basic sanity check - output should be positive
    double x = reparamGamma(2.0, 1.0, 0.5, 0.3);
    expectTrue(x > 0);

    // At z = 0, v = 1, so x = d / β = (α - 1/3) / β
    double x2 = reparamGamma(2.0, 1.0, 0.0, 0.5);
    expectNear(2.0 - 1.0 / 3.0, x2, 1e-10);
  };

  "reparamGamma symbolic"_test = [] {
    constexpr Symbol<struct AlphaG> alpha;
    constexpr Symbol<struct BetaG> beta;
    constexpr Symbol<struct ZG> z;
    constexpr Symbol<struct UG> u;

    auto x = reparamGamma(alpha, beta, z, u);
    static_assert(Symbolic<decltype(x)>);

    // At z = 0, should get (α - 1/3) / β
    auto b = BinderPack{alpha = 2.0, beta = 1.0, z = 0.0, u = 0.5};
    expectNear(2.0 - 1.0 / 3.0, evaluate(x, b), 1e-10);

    // Can differentiate w.r.t. alpha
    auto dx_dalpha = diff(x, alpha);
    expectTrue(std::isfinite(evaluate(dx_dalpha, b)));
  };

  "reparamGamma dual"_test = [] {
    Dual<double> alpha{2.0, 1.0};
    Dual<double> beta{1.0, 0.0};
    Dual<double> z{0.0, 0.0};
    Dual<double> u{0.5, 0.0};

    auto x = reparamGamma(alpha, beta, z, u);
    expectNear(2.0 - 1.0 / 3.0, x.value, 1e-10);
    expectTrue(std::isfinite(x.gradient));
  };

  // ===========================================================================
  // BETA REPARAMETERIZATION (via Gamma ratio)
  // ===========================================================================

  "reparamBeta double"_test = [] {
    // If g1 = g2 = 1, then x = 0.5
    expectNear(0.5, reparamBeta(2.0, 2.0, 1.0, 1.0), 1e-10);

    // If g1 = 2, g2 = 1, then x = 2/3
    expectNear(2.0 / 3.0, reparamBeta(2.0, 2.0, 2.0, 1.0), 1e-10);

    // If g1 = 1, g2 = 3, then x = 0.25
    expectNear(0.25, reparamBeta(2.0, 2.0, 1.0, 3.0), 1e-10);
  };

  "reparamBeta symbolic"_test = [] {
    constexpr Symbol<struct AlphaBeta> alpha;
    constexpr Symbol<struct BetaBeta> beta;
    constexpr Symbol<struct G1> g1;
    constexpr Symbol<struct G2> g2;

    auto x = reparamBeta(alpha, beta, g1, g2);
    static_assert(Symbolic<decltype(x)>);

    auto b = BinderPack{alpha = 2.0, beta = 2.0, g1 = 2.0, g2 = 1.0};
    expectNear(2.0 / 3.0, evaluate(x, b), 1e-10);

    // ∂x/∂g1 = g2 / (g1 + g2)² = 1 / 9
    auto dx_dg1 = diff(x, g1);
    expectNear(1.0 / 9.0, evaluate(dx_dg1, b), 1e-10);
  };

  "reparamBeta dual"_test = [] {
    Dual<double> alpha{2.0, 0.0};
    Dual<double> beta{2.0, 0.0};
    Dual<double> g1{2.0, 1.0};
    Dual<double> g2{1.0, 0.0};

    auto x = reparamBeta(alpha, beta, g1, g2);
    expectNear(2.0 / 3.0, x.value, 1e-10);
    expectNear(1.0 / 9.0, x.gradient, 1e-10);
  };

  // ===========================================================================
  // GRADIENT FLOW: log p(z) through reparameterized z
  // ===========================================================================

  "gradient flows through reparam to logProb"_test = [] {
    constexpr Symbol<struct MuFlow> mu;
    constexpr Symbol<struct SigmaFlow> sigma;
    constexpr Symbol<struct EpsFlow> epsilon;

    // z = μ + σ * ε (reparameterized sample)
    auto z = reparamNormal(mu, sigma, epsilon);

    // log p(z | 0, 1) = log-prob under standard normal target
    auto lp = logNormal(z, Literal{0.0}, Literal{1.0});

    // Gradients w.r.t. variational parameters
    auto dlp_dmu = diff(lp, mu);
    auto dlp_dsigma = diff(lp, sigma);

    // At μ=0, σ=1, ε=0.5: z = 0.5
    // log p(z) = -0.5 * z² - log(√2π)
    // ∂(log p)/∂z = -z
    // ∂z/∂μ = 1, so ∂(log p)/∂μ = -z = -0.5
    // ∂z/∂σ = ε, so ∂(log p)/∂σ = -z * ε = -0.25
    auto b = BinderPack{mu = 0.0, sigma = 1.0, epsilon = 0.5};

    expectNear(-0.5, evaluate(dlp_dmu, b), 1e-10);
    expectNear(-0.25, evaluate(dlp_dsigma, b), 1e-10);
  };

  "ELBO gradient estimation pattern"_test = [] {
    // Typical ELBO gradient estimation:
    // 1. Sample ε ~ N(0, 1)
    // 2. Compute z = μ + σ * ε
    // 3. Compute log p(z) - log q(z|μ,σ)
    // 4. Differentiate w.r.t. μ, σ

    constexpr Symbol<struct MuE> mu;
    constexpr Symbol<struct SigmaE> sigma;
    constexpr Symbol<struct EpsE> epsilon;

    auto z = reparamNormal(mu, sigma, epsilon);

    // Target: p(z) = N(z | 0, 2)
    auto log_p = logNormal(z, Literal{0.0}, Literal{2.0});

    // Variational: q(z | μ, σ)
    auto log_q = logNormal(z, mu, sigma);

    // ELBO term (for one sample)
    auto elbo_term = log_p - log_q;

    // Gradients
    auto delbo_dmu = diff(elbo_term, mu);
    auto delbo_dsigma = diff(elbo_term, sigma);

    auto b = BinderPack{mu = 0.5, sigma = 1.0, epsilon = 0.3};

    // Just verify they're finite
    expectTrue(std::isfinite(evaluate(elbo_term, b)));
    expectTrue(std::isfinite(evaluate(delbo_dmu, b)));
    expectTrue(std::isfinite(evaluate(delbo_dsigma, b)));
  };

  // ===========================================================================
  // CROSS-VALIDATION: Symbolic and Dual match
  // ===========================================================================

  "symbolic and dual reparameterization gradients match"_test = [] {
    // Normal reparameterization
    {
      constexpr Symbol<struct MuS> mu;
      constexpr Symbol<struct SigmaS> sigma;
      constexpr Symbol<struct EpsS> eps;

      auto z = reparamNormal(mu, sigma, eps);
      auto dz_dmu = diff(z, mu);
      auto dz_dsigma = diff(z, sigma);

      auto b = BinderPack{mu = 1.5, sigma = 2.0, eps = 0.7};
      double sym_grad_mu = evaluate(dz_dmu, b);
      double sym_grad_sigma = evaluate(dz_dsigma, b);

      Dual<double> mud{1.5, 1.0};
      Dual<double> sigmad{2.0, 0.0};
      Dual<double> epsd{0.7, 0.0};
      double dual_grad_mu = reparamNormal(mud, sigmad, epsd).gradient;

      Dual<double> mud2{1.5, 0.0};
      Dual<double> sigmad2{2.0, 1.0};
      double dual_grad_sigma = reparamNormal(mud2, sigmad2, epsd).gradient;

      expectNear(sym_grad_mu, dual_grad_mu, 1e-10);
      expectNear(sym_grad_sigma, dual_grad_sigma, 1e-10);
    }

    // Uniform reparameterization
    {
      constexpr Symbol<struct AS> a;
      constexpr Symbol<struct BS> b;
      constexpr Symbol<struct US> u;

      auto x = reparamUniform(a, b, u);
      auto dx_da = diff(x, a);
      auto dx_db = diff(x, b);

      auto bindings = BinderPack{a = 2.0, b = 8.0, u = 0.4};
      double sym_grad_a = evaluate(dx_da, bindings);
      double sym_grad_b = evaluate(dx_db, bindings);

      Dual<double> ad{2.0, 1.0};
      Dual<double> bd{8.0, 0.0};
      Dual<double> ud{0.4, 0.0};
      double dual_grad_a = reparamUniform(ad, bd, ud).gradient;

      Dual<double> ad2{2.0, 0.0};
      Dual<double> bd2{8.0, 1.0};
      double dual_grad_b = reparamUniform(ad2, bd2, ud).gradient;

      expectNear(sym_grad_a, dual_grad_a, 1e-10);
      expectNear(sym_grad_b, dual_grad_b, 1e-10);
    }
  };

  return TestRegistry::result();
}
