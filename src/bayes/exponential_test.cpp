#include "bayes/exponential.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Exponential dist{1.0};
    expectNear(1.0, dist.prob(0.0));  // PDF = λ at x=0
    expectTrue(dist.prob(0.0) > dist.prob(1.0));  // decays exponentially
    expectNear(std::log(dist.prob(1.0)), dist.logProb(1.0));

    // Out of bounds
    expectEq(0.0, dist.prob(-1.0));
  };

  "logProb"_test = [] {
    Exponential dist{1.5};
    expectNear(std::log(dist.prob(1.0)), dist.logProb(1.0));

    // Avoids underflow for large x
    expectTrue(std::isfinite(dist.logProb(1000.0)));

    // Out of bounds → -∞
    expectTrue(std::isinf(dist.logProb(-1.0)) && dist.logProb(-1.0) < 0);
  };

  "cdf"_test = [] {
    Exponential dist{2.0};
    expectEq(0.0, dist.cdf(0.0));
    expectNear(1.0 - std::exp(-1.0), dist.cdf(0.5));  // CDF(1/λ) = 1-e^(-1)

    // Monotonically increasing
    expectTrue(dist.cdf(0.5) < dist.cdf(1.0));

    // Out of bounds
    expectEq(0.0, dist.cdf(-1.0));
  };

  "invCdf"_test = [] {
    Exponential dist{1.5};

    // Roundtrip: invCdf(cdf(x)) = x
    for (double p : {0.1, 0.5, 0.9}) {
      expectNear(p, dist.cdf(dist.invCdf(p)));
    }

    // Median = ln(2)/λ
    expectNear(std::log(2.0) / 1.5, dist.invCdf(0.5));
  };

  "mean and variance"_test = [] {
    // E[X] = 1/λ
    expectNear(1.0, Exponential{1.0}.mean());
    expectNear(0.5, Exponential{2.0}.mean());

    // Var[X] = 1/λ²
    expectNear(1.0, Exponential{1.0}.variance());
    expectNear(0.25, Exponential{2.0}.variance());
  };

  "sample"_test = [] {
    std::mt19937_64 g{42};
    Exponential dist{2.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      double x = dist.sample(g);
      expectTrue(x >= 0.0);
      sum += x;
    }

    // Standard error = σ/√N = (1/λ)/√N = 0.5/100 = 0.005
    // Tolerance of 0.05 is 10 standard errors
    expectNear(0.5, sum / N, 0.05);
  };

  "constexpr"_test = [] {
    constexpr Exponential dist{2.0};
    static_assert(dist.lambda() == 2.0);
    static_assert(dist.mean() == 0.5);
    static_assert(dist.variance() == 0.25);
    static_assert(dist.prob(0.0) == 2.0);
    static_assert(dist.cdf(0.0) == 0.0);
  };

  return TestRegistry::result();
}
