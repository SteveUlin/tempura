#include "bayes/logistic.h"

#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Logistic dist{5.0, 2.0};

    // At μ, PDF = 1/(4s) = 1/8 = 0.125
    expectNear(0.125, dist.prob(5.0));

    // Symmetric: p(μ + δ) = p(μ - δ)
    expectNear(dist.prob(7.0), dist.prob(3.0));
    expectNear(dist.prob(8.0), dist.prob(2.0));
  };

  "logProb"_test = [] {
    Logistic dist{0.0, 1.0};

    // At μ, log(1/(4s)) = -log(4)
    expectNear(-std::log(4.0), dist.logProb(0.0));

    // Consistency with prob
    expectNear(std::log(dist.prob(2.0)), dist.logProb(2.0));
    expectNear(std::log(dist.prob(10.0)), dist.logProb(10.0));

    // No underflow at extreme values
    expectTrue(std::isfinite(dist.logProb(50.0)));
  };

  "cdf"_test = [] {
    Logistic dist{5.0, 2.0};

    // CDF(μ) = 0.5
    expectNear(0.5, dist.cdf(5.0));

    // Symmetric: CDF(μ - δ) + CDF(μ + δ) = 1
    expectNear(1.0, dist.cdf(3.0) + dist.cdf(7.0));

    // Monotonically increasing
    expectTrue(dist.cdf(0.0) < dist.cdf(5.0));
    expectTrue(dist.cdf(5.0) < dist.cdf(10.0));

    // Bounds
    expectTrue(dist.cdf(-20.0) < 0.001);
    expectTrue(dist.cdf(20.0) > 0.999);
  };

  "mean and variance"_test = [] {
    expectEq(0.0, Logistic{0.0, 1.0}.mean());
    expectEq(5.0, Logistic{5.0, 2.0}.mean());

    // Var = (π²s²)/3
    const double π² = std::numbers::pi * std::numbers::pi;
    expectNear(π² / 3.0, Logistic{0.0, 1.0}.variance());
    expectNear(4.0 * π² / 3.0, Logistic{0.0, 2.0}.variance());
  };

  "sample"_test = [] {
    std::mt19937_64 g{123};
    Logistic dist{5.0, 2.0};

    const int N = 10'000;
    double sum = 0.0;
    double sum_sq = 0.0;

    for (int i = 0; i < N; ++i) {
      auto x = dist.sample(g);
      sum += x;
      sum_sq += x * x;
    }

    double sample_mean = sum / N;
    double sample_var = (sum_sq / N) - (sample_mean * sample_mean);

    // Standard error for mean = √(Var/N) ≈ √(13.16/10000) ≈ 0.036
    // Tolerance of 0.1 is ~3 standard errors
    expectNear(5.0, sample_mean, 0.1);

    // Var ≈ 13.16, SE for variance ≈ 0.19
    // Tolerance of 0.3 is ~1.5 standard errors
    const double π² = std::numbers::pi * std::numbers::pi;
    expectNear(4.0 * π² / 3.0, sample_var, 0.3);
  };

  "constexpr"_test = [] {
    constexpr Logistic dist{0.0, 1.0};
    static_assert(dist.mean() == 0.0);
    static_assert(dist.mu() == 0.0);
    static_assert(dist.s() == 1.0);
    static_assert(dist.prob(0.0) == 0.25);
    static_assert(dist.cdf(0.0) == 0.5);
  };

  return TestRegistry::result();
}
