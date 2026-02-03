// Tests for symbolic4/distributions/wrappers.h
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/interpreter/eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // NormalDist
  // ===========================================================================

  "NormalDist stores parameters"_test = [] {
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;
    NormalDist dist{mu, sigma};

    static_assert(std::is_same_v<decltype(dist.mu()), Symbol<struct Mu>>);
    static_assert(std::is_same_v<decltype(dist.sigma()), Symbol<struct Sigma>>);
  };

  "NormalDist logProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Normal(1.0, 0.5);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 2.0);

    double z = (2.0 - 1.0) / 0.5;
    double expected = -0.5 * z * z - std::log(0.5) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "NormalDist unnormalizedLogProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Normal(1.0, 0.5);
    auto lp = dist.unnormalizedLogProbFor(x);
    double val = evaluate(lp, x = 2.0);

    double z = (2.0 - 1.0) / 0.5;
    double expected = -0.5 * z * z;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // HalfNormalDist
  // ===========================================================================

  "HalfNormalDist stores parameters"_test = [] {
    Symbol<struct Sigma> sigma;
    HalfNormalDist dist{sigma};

    static_assert(std::is_same_v<decltype(dist.sigma()), Symbol<struct Sigma>>);
  };

  "HalfNormalDist logProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = HalfNormal(2.0);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 1.0);

    double expected = 0.2257913526447274 - std::log(2.0) - 1.0 / (2.0 * 4.0);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // BetaDist
  // ===========================================================================

  "BetaDist stores parameters"_test = [] {
    Symbol<struct Alpha> alpha;
    Symbol<struct Beta> beta;
    BetaDist dist{alpha, beta};

    static_assert(std::is_same_v<decltype(dist.alpha()), Symbol<struct Alpha>>);
    static_assert(std::is_same_v<decltype(dist.beta()), Symbol<struct Beta>>);
  };

  "BetaDist unnormalizedLogProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Beta(2.0, 5.0);
    auto lp = dist.unnormalizedLogProbFor(x);
    double val = evaluate(lp, x = 0.3);

    // Unnormalized: (alpha-1)*log(x) + (beta-1)*log(1-x)
    double expected = std::log(0.3) + 4.0 * std::log(0.7);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // GammaDist
  // ===========================================================================

  "GammaDist stores parameters"_test = [] {
    Symbol<struct Alpha> alpha;
    Symbol<struct Beta> beta;
    GammaDist dist{alpha, beta};

    static_assert(std::is_same_v<decltype(dist.alpha()), Symbol<struct Alpha>>);
    static_assert(std::is_same_v<decltype(dist.beta()), Symbol<struct Beta>>);
  };

  "GammaDist unnormalizedLogProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Gamma(3.0, 0.5);
    auto lp = dist.unnormalizedLogProbFor(x);
    double val = evaluate(lp, x = 2.0);

    // Unnormalized: (alpha-1)*log(x) - beta*x
    double expected = 2.0 * std::log(2.0) - 0.5 * 2.0;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // ExponentialDist
  // ===========================================================================

  "ExponentialDist stores parameters"_test = [] {
    Symbol<struct Lambda> lambda;
    ExponentialDist dist{lambda};

    static_assert(std::is_same_v<decltype(dist.lambda()), Symbol<struct Lambda>>);
  };

  "ExponentialDist logProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Exponential(0.5);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 2.0);

    double expected = std::log(0.5) - 1.0;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // UniformDist
  // ===========================================================================

  "UniformDist stores parameters"_test = [] {
    Symbol<struct A> a;
    Symbol<struct B> b;
    UniformDist dist{a, b};

    static_assert(std::is_same_v<decltype(dist.a()), Symbol<struct A>>);
    static_assert(std::is_same_v<decltype(dist.b()), Symbol<struct B>>);
  };

  "UniformDist logProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Uniform(0.0, 2.0);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 0.5);

    expectNear(-std::log(2.0), val, 1e-10);
  };

  // ===========================================================================
  // CauchyDist
  // ===========================================================================

  "CauchyDist stores parameters"_test = [] {
    Symbol<struct X0> x0;
    Symbol<struct Gamma> gamma;
    CauchyDist dist{x0, gamma};

    static_assert(std::is_same_v<decltype(dist.x0()), Symbol<struct X0>>);
    static_assert(std::is_same_v<decltype(dist.gamma()), Symbol<struct Gamma>>);
  };

  "CauchyDist logProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Cauchy(0.0, 1.0);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 1.0);

    double expected = -std::log(M_PI) - std::log(1.0) - std::log(2.0);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // BernoulliDist
  // ===========================================================================

  "BernoulliDist stores parameters"_test = [] {
    Symbol<struct P> p;
    BernoulliDist dist{p};

    static_assert(std::is_same_v<decltype(dist.p()), Symbol<struct P>>);
  };

  "BernoulliDist logProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = Bernoulli(0.7);

    double val1 = evaluate(dist.logProbFor(x), x = 1.0);
    expectNear(std::log(0.7), val1, 1e-10);

    double val0 = evaluate(dist.logProbFor(x), x = 0.0);
    expectNear(std::log(0.3), val0, 1e-10);
  };

  // ===========================================================================
  // StudentTDist
  // ===========================================================================

  "StudentTDist stores parameters"_test = [] {
    Symbol<struct Nu> nu;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;
    StudentTDist dist{nu, mu, sigma};

    static_assert(std::is_same_v<decltype(dist.nu()), Symbol<struct Nu>>);
    static_assert(std::is_same_v<decltype(dist.mu()), Symbol<struct Mu>>);
    static_assert(std::is_same_v<decltype(dist.sigma()), Symbol<struct Sigma>>);
  };

  "StudentTDist unnormalizedLogProbFor"_test = [] {
    Symbol<struct X> x;
    auto dist = StudentT(3.0, 0.0, 1.0);
    auto lp = dist.unnormalizedLogProbFor(x);
    double val = evaluate(lp, x = 1.0);

    // Unnormalized: -(nu+1)/2 * log(1 + z^2/nu)
    // For nu=3, mu=0, sigma=1, x=1: z=1, log(1 + 1/3) = log(4/3)
    // -(3+1)/2 * log(4/3) = -2 * log(4/3)
    double expected = -2.0 * std::log(4.0 / 3.0);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // DirichletDist
  // ===========================================================================

  "DirichletDist stores parameters"_test = [] {
    Symbol<struct Alpha> alpha;
    DirichletDist dist{alpha};

    static_assert(std::is_same_v<decltype(dist.alpha()), Symbol<struct Alpha>>);
  };

  "DirichletDist logProbFor"_test = [] {
    // Test single component: (alpha - 1) * log(x)
    Symbol<struct X> x;
    auto dist = Dirichlet(2.0);  // alpha = 2
    auto lp = dist.logProbFor(x);

    // For x = 0.5, alpha = 2: (2-1) * log(0.5) = log(0.5)
    double val = evaluate(lp, x = 0.5);
    expectNear(std::log(0.5), val, 1e-10);

    // For x = 0.25, alpha = 2: (2-1) * log(0.25) = log(0.25)
    double val2 = evaluate(lp, x = 0.25);
    expectNear(std::log(0.25), val2, 1e-10);
  };

  // ===========================================================================
  // CategoricalDist
  // ===========================================================================

  "CategoricalDist stores parameters"_test = [] {
    Symbol<struct LogP> log_p;
    CategoricalDist dist{log_p};

    static_assert(std::is_same_v<decltype(dist.logP()), Symbol<struct LogP>>);
  };

  "CategoricalDist logProbFor"_test = [] {
    // Test: x * log_p
    Symbol<struct X> x;
    auto dist = CategoricalLogP(-1.0);  // log_p = -1

    // For x=1: log p = -1
    double val1 = evaluate(dist.logProbFor(x), x = 1.0);
    expectNear(-1.0, val1, 1e-10);

    // For x=0: log p = 0
    double val0 = evaluate(dist.logProbFor(x), x = 0.0);
    expectNear(0.0, val0, 1e-10);
  };

  "CategoricalDist with probability factory"_test = [] {
    Symbol<struct X> x;
    Symbol<struct P> p;
    auto dist = Categorical(p);  // Uses log(p) internally

    // For p=0.5, x=1: log(0.5)
    double val = evaluate(dist.logProbFor(x), x = 1.0, p = 0.5);
    expectNear(std::log(0.5), val, 1e-10);
  };

  return TestRegistry::result();
}
