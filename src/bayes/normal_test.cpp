#include "bayes/normal.h"

#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Normal dist{0.0, 1.0};
    expectNear(1.0 / std::sqrt(2.0 * std::numbers::pi), dist.prob(0.0));
    expectTrue(dist.prob(-3.0) < 0.01);  // Tails decay rapidly

    Normal dist2{5.0, 2.0};
    expectNear(1.0 / (2.0 * std::sqrt(2.0 * std::numbers::pi)), dist2.prob(5.0));
  };

  "logProb"_test = [] {
    Normal dist{0.0, 1.0};
    expectNear(-0.5 * std::log(2.0 * std::numbers::pi), dist.logProb(0.0));
    expectNear(std::log(dist.prob(1.0)), dist.logProb(1.0));
    expectTrue(dist.logProb(10.0) < -50.0);  // Far tails handled in log-space

    Normal dist2{3.0, 0.5};
    expectNear(-std::log(0.5) - 0.5 * std::log(2.0 * std::numbers::pi), dist2.logProb(3.0));
  };

  "cdf"_test = [] {
    Normal dist{0.0, 1.0};
    expectNear(0.5, dist.cdf(0.0));     // Median at mean
    expectNear(0.0, dist.cdf(-5.0), 0.001);
    expectNear(1.0, dist.cdf(5.0), 0.001);
    expectNear(0.8413, dist.cdf(1.0), 0.001);   // 68-95-99.7 rule
    expectNear(0.1587, dist.cdf(-1.0), 0.001);
  };

  "mean and variance"_test = [] {
    expectEq(0.0, Normal{0.0, 1.0}.mean());
    expectEq(5.0, Normal{5.0, 2.0}.mean());

    expectNear(1.0, Normal{0.0, 1.0}.variance());
    expectNear(4.0, Normal{5.0, 2.0}.variance());

    expectEq(1.0, Normal{0.0, 1.0}.stddev());
    expectEq(2.0, Normal{5.0, 2.0}.stddev());

    Normal dist{3.5, 1.5};
    expectEq(3.5, dist.mu());
    expectEq(1.5, dist.sigma());
  };

  "sample"_test = [] {
    std::mt19937_64 g{123};
    Normal dist{2.5, 1.5};

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

    // Standard error of mean = σ/√N = 1.5/100 = 0.015
    // Standard error of variance ≈ σ²√(2/N) = 2.25√(2/10000) ≈ 0.032
    // Tolerances are ~7 standard errors (effectively never fail)
    expectNear(2.5, sample_mean, 0.1);
    expectNear(2.25, sample_var, 0.2);
  };

  "constexpr"_test = [] {
    constexpr Normal dist{1.0, 2.0};
    static_assert(dist.mean() == 1.0);
    static_assert(dist.variance() == 4.0);
    static_assert(dist.stddev() == 2.0);
    static_assert(dist.mu() == 1.0);
    static_assert(dist.sigma() == 2.0);
  };

  return TestRegistry::result();
}

