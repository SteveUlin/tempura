#include "bayes/cauchy.h"

#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Cauchy dist{0.0, 1.0};
    expectNear(1.0 / std::numbers::pi, dist.prob(0.0));  // peak at μ
    expectNear(std::log(dist.prob(0.5)), dist.logProb(0.5));

    // Symmetric around μ
    Cauchy dist2{5.0, 2.0};
    expectNear(dist2.prob(7.0), dist2.prob(3.0));

    // Heavy tails: PDF decays slowly (1/x²) compared to Normal
    expectTrue(dist.prob(10.0) > 0.0);
  };

  "logProb"_test = [] {
    Cauchy dist{0.0, 1.0};
    expectNear(-std::log(std::numbers::pi), dist.logProb(0.0));

    // Consistent with prob
    Cauchy dist2{2.0, 3.0};
    for (double x : {-10.0, 0.0, 2.0, 20.0}) {
      expectNear(std::log(dist2.prob(x)), dist2.logProb(x));
    }
  };

  "cdf"_test = [] {
    Cauchy dist{0.0, 1.0};
    expectNear(0.5, dist.cdf(0.0));  // CDF(μ) = 0.5

    // Symmetric: CDF(μ-δ) + CDF(μ+δ) = 1
    Cauchy dist2{5.0, 2.0};
    expectNear(1.0, dist2.cdf(3.0) + dist2.cdf(7.0));

    // Known values: CDF(μ±σ) = 0.5 ± arctan(1)/π = 0.25, 0.75
    expectNear(0.75, dist.cdf(1.0));
    expectNear(0.25, dist.cdf(-1.0));
  };

  "invCdf"_test = [] {
    Cauchy dist{0.0, 1.0};
    expectNear(0.0, dist.invCdf(0.5));
    expectNear(1.0, dist.invCdf(0.75));
    expectNear(-1.0, dist.invCdf(0.25));

    // Round-trip: invCDF(CDF(x)) = x
    Cauchy dist2{5.0, 2.0};
    for (double p : {0.1, 0.25, 0.5, 0.75, 0.9}) {
      expectNear(p, dist2.cdf(dist2.invCdf(p)));
    }
  };

  "median and moments"_test = [] {
    Cauchy dist{5.0, 2.0};
    expectEq(5.0, dist.median());
    expectEq(5.0, dist.mu());
    expectEq(2.0, dist.sigma());

    // Mean and variance don't exist
    expectTrue(std::isnan(dist.mean()));
    expectTrue(std::isnan(dist.variance()));
  };

  "sample"_test = [] {
    std::mt19937_64 g{123};
    Cauchy dist{5.0, 2.0};

    const int N = 10'000;
    std::vector<double> samples;
    samples.reserve(N);

    for (int i = 0; i < N; ++i) {
      double x = dist.sample(g);
      expectTrue(std::isfinite(x));
      samples.push_back(x);
    }

    // Sample median approximates theoretical median
    std::sort(samples.begin(), samples.end());
    double sample_median = samples[N / 2];

    // Standard error ≈ 1.36·σ/√N ≈ 0.027 for Cauchy median
    // Tolerance of 0.2 is ~7 standard errors
    expectNear(5.0, sample_median, 0.2);
  };

  "constexpr"_test = [] {
    constexpr Cauchy dist{0.0, 1.0};
    static_assert(dist.median() == 0.0);
    static_assert(dist.mu() == 0.0);
    static_assert(dist.sigma() == 1.0);
    static_assert(dist.prob(0.0) == 1.0 / std::numbers::pi);
    static_assert(dist.cdf(0.0) == 0.5);
  };

  return TestRegistry::result();
}
