#include "bayes/gamma.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Gamma dist{2.0, 1.0};
    expectTrue(dist.prob(1.0) > 0.0);
    expectNear(std::log(dist.prob(1.0)), dist.logProb(1.0));

    // Out of bounds
    expectEq(0.0, dist.prob(-1.0));
    expectEq(0.0, dist.prob(0.0));

    // Gamma(1, λ) = Exponential(λ): PDF = λ·exp(-λx)
    expectNear(2.0 * std::exp(-2.0), Gamma{1.0, 2.0}.prob(1.0));
  };

  "logProb"_test = [] {
    Gamma dist{2.5, 1.5};
    expectNear(std::log(dist.prob(1.0)), dist.logProb(1.0));

    // Log-space avoids underflow
    expectTrue(std::isfinite(dist.logProb(100.0)));

    // Out of bounds → -∞
    expectTrue(std::isinf(dist.logProb(-1.0)) && dist.logProb(-1.0) < 0);
    expectTrue(std::isinf(dist.logProb(0.0)) && dist.logProb(0.0) < 0);
  };

  "cdf"_test = [] {
    Gamma dist{2.0, 1.0};
    expectEq(0.0, dist.cdf(-1.0));
    expectEq(0.0, dist.cdf(0.0));
    expectTrue(dist.cdf(20.0) > 0.99);

    // Monotonically increasing
    expectTrue(dist.cdf(0.0) < dist.cdf(1.0));
    expectTrue(dist.cdf(1.0) < dist.cdf(2.0));

    // Gamma(1, λ) = Exponential(λ): CDF = 1 - exp(-λx)
    expectNear(1.0 - std::exp(-2.0), Gamma{1.0, 2.0}.cdf(1.0));
  };

  "mean and variance"_test = [] {
    // E[X] = α/β, Var[X] = α/β²
    expectNear(2.0, Gamma{2.0, 1.0}.mean());
    expectNear(1.0, Gamma{2.0, 2.0}.mean());
    expectNear(2.0, Gamma{2.0, 1.0}.variance());
    expectNear(0.5, Gamma{2.0, 2.0}.variance());

    // Exponential special case: Gamma(1, λ)
    expectNear(0.5, Gamma{1.0, 2.0}.mean());
    expectNear(0.25, Gamma{1.0, 2.0}.variance());
  };

  "sample"_test = [] {
    std::mt19937_64 g{42};
    Gamma dist{2.0, 1.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      double x = dist.sample(g);
      expectTrue(x > 0.0 && std::isfinite(x));
      sum += x;
    }

    // Standard error = √(Var/N) = √(2/10000) ≈ 0.014
    // Tolerance of 0.1 is ~7 standard errors
    expectNear(2.0, sum / N, 0.1);
  };

  "sample with α < 1"_test = [] {
    std::mt19937_64 g{321};
    Gamma dist{0.5, 2.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      double x = dist.sample(g);
      expectTrue(x > 0.0 && std::isfinite(x));
      sum += x;
    }

    // Standard error = √(Var/N) = √(0.125/10000) ≈ 0.0035
    // Tolerance of 0.05 is ~14 standard errors
    expectNear(0.25, sum / N, 0.05);
  };

  "constexpr"_test = [] {
    constexpr Gamma dist{2.0, 1.0};
    static_assert(dist.alpha() == 2.0);
    static_assert(dist.beta() == 1.0);
    static_assert(dist.mean() == 2.0);
    static_assert(dist.variance() == 2.0);
    static_assert(dist.prob(0.0) == 0.0);
    static_assert(dist.cdf(0.0) == 0.0);
  };

  return TestRegistry::result();
}
