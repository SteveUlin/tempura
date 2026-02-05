// Tests for symbolic4/distributions/sample.h
#include "symbolic4/distributions/sample.h"
#include "symbolic4/distributions/joint.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/interpreter/eval.h"
#include "unit.h"

#include <random>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Trace type
  // ===========================================================================

  "Trace stores values"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto trace = Trace{x = 1.0, y = 2.0};
    expectNear(1.0, trace[x], 1e-10);
    expectNear(2.0, trace[y], 1e-10);
  };

  "empty Trace"_test = [] {
    Trace<> trace{};
    (void)trace;  // Just verify it compiles
  };

  "addToTrace adds binding"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Y> y;

    auto trace = Trace{x = 1.0};
    auto extended = addToTrace(trace, y = 2.0);

    expectNear(1.0, extended[x], 1e-10);
    expectNear(2.0, extended[y], 1e-10);
  };

  // ===========================================================================
  // sampleDist() from distribution wrappers
  // ===========================================================================

  "sampleDist from NormalDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Normal(0_c, 1_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(std::isfinite(val));
  };

  "sampleDist from NormalDist with symbolic params"_test = [] {
    std::mt19937 rng{42};
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;
    NormalDist dist{mu, sigma};

    auto bindings = BinderPack{mu = 5.0, sigma = 2.0};
    double val = sampleDist(dist, bindings, rng);
    expectTrue(std::isfinite(val));
  };

  "sampleDist from HalfNormalDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = HalfNormal(2_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(std::isfinite(val));
    expectTrue(val >= 0.0);
  };

  "sampleDist from BetaDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Beta(2_c, 5_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(std::isfinite(val));
    expectTrue(val > 0.0 && val < 1.0);
  };

  "sampleDist from GammaDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Gamma(2_c, 0.5_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(std::isfinite(val));
    expectTrue(val > 0.0);
  };

  "sampleDist from ExponentialDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Exponential(0.5_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(std::isfinite(val));
    expectTrue(val > 0.0);
  };

  "sampleDist from UniformDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Uniform(1_c, 5_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(val >= 1.0 && val <= 5.0);
  };

  "sampleDist from CauchyDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Cauchy(0_c, 1_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(std::isfinite(val));
  };

  "sampleDist from BernoulliDist"_test = [] {
    std::mt19937 rng{42};
    auto dist = Bernoulli(0.7_c);
    double val = sampleDist(dist, Trace<>{}, rng);

    expectTrue(val == 0.0 || val == 1.0);
  };

  // ===========================================================================
  // sample() from RandomVar
  // ===========================================================================

  "sample from RandomVar with no deps"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 1_c);

    double val = sample(mu, rng);
    expectTrue(std::isfinite(val));
  };

  "sample from RandomVar with bindings"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    double mu_val = sample(mu, rng);
    double y_val = sample(y, rng, mu = mu_val);

    expectTrue(std::isfinite(mu_val));
    expectTrue(std::isfinite(y_val));
  };

  // ===========================================================================
  // sampleAll() - sample multiple RVs in dependency order
  // ===========================================================================

  "sampleAll for single RV"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 1_c);

    auto trace = sampleAll(rng, mu);
    double val = trace[mu];

    expectTrue(std::isfinite(val));
  };

  "sampleAll for two RVs"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    auto trace = sampleAll(rng, mu, y);

    double mu_val = trace[mu];
    double y_val = trace[y];

    expectTrue(std::isfinite(mu_val));
    expectTrue(std::isfinite(y_val));
  };

  "sampleAll for three RVs"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 10_c);
    auto sigma = halfNormal(5_c);
    auto y = normal(mu, sigma);

    auto trace = sampleAll(rng, mu, sigma, y);

    double mu_val = trace[mu];
    double sigma_val = trace[sigma];
    double y_val = trace[y];

    expectTrue(std::isfinite(mu_val));
    expectTrue(std::isfinite(sigma_val));
    expectTrue(sigma_val > 0.0);
    expectTrue(std::isfinite(y_val));
  };

  "sampleAll trace can be used as bindings"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    auto trace = sampleAll(rng, mu, y);

    // Use trace as bindings for evaluation
    auto lp = logProb(mu, y);
    double val = evaluate(lp, trace);

    expectTrue(std::isfinite(val));
  };

  // ===========================================================================
  // samplePlate() - sample n iid copies
  // ===========================================================================

  "samplePlate returns vector"_test = [] {
    std::mt19937 rng{42};
    auto y = normal(0_c, 1_c);

    auto samples = samplePlate(y, 100, rng);

    expectEq(100UL, samples.size());
    for (double s : samples) {
      expectTrue(std::isfinite(s));
    }
  };

  "samplePlate with bindings"_test = [] {
    std::mt19937 rng{42};
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    // Sample mu once
    double mu_val = sample(mu, rng);

    // Sample 50 y values given mu
    auto samples = samplePlate(y, 50, rng, mu = mu_val);

    expectEq(50UL, samples.size());
    for (double s : samples) {
      expectTrue(std::isfinite(s));
    }
  };

  // ===========================================================================
  // Statistical tests (loose bounds)
  // ===========================================================================

  "sampleDist from Normal has correct mean (approx)"_test = [] {
    std::mt19937 rng{12345};
    auto dist = Normal(5_c, 2_c);

    double sum = 0.0;
    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) {
      sum += sampleDist(dist, Trace<>{}, rng);
    }
    double mean = sum / N;

    // Mean should be close to 5.0
    // SE = 2.0 / sqrt(10000) = 0.02, use 10*SE tolerance
    expectNear(5.0, mean, 0.2);
  };

  "sampleDist from Beta has correct mean (approx)"_test = [] {
    std::mt19937 rng{12345};
    auto dist = Beta(2_c, 5_c);

    double sum = 0.0;
    constexpr int N = 10000;
    for (int i = 0; i < N; ++i) {
      sum += sampleDist(dist, Trace<>{}, rng);
    }
    double mean = sum / N;

    // E[Beta(2,5)] = 2/(2+5) = 2/7 ≈ 0.286
    expectNear(2.0 / 7.0, mean, 0.05);
  };

  return TestRegistry::result();
}
