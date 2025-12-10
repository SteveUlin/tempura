#include "bayes/poisson.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Poisson dist{3.0};

    // Known values: P(X=k) = λ^k e^(-λ) / k!
    expectNear(std::exp(-3.0), dist.prob(0), 1e-6);
    expectNear(3.0 * std::exp(-3.0), dist.prob(1), 1e-6);
    expectNear(4.5 * std::exp(-3.0), dist.prob(2), 1e-6);

    // Decreases in tails
    expectTrue(dist.prob(3) > dist.prob(10));
    expectTrue(dist.prob(3) > dist.prob(0));

    // Negative values
    expectEq(0.0, dist.prob(-1));
  };

  "logProb"_test = [] {
    Poisson dist{4.0};
    expectNear(std::log(dist.prob(2)), dist.logProb(2));
    expectNear(std::log(dist.prob(5)), dist.logProb(5));

    // Log-space avoids underflow for large k
    expectTrue(std::isfinite(dist.logProb(100)));

    // Negative k → -∞
    expectTrue(std::isinf(Poisson{3.0}.logProb(-1)) && Poisson{3.0}.logProb(-1) < 0);
  };

  "cdf"_test = [] {
    Poisson dist{3.0};
    expectNear(std::exp(-3.0), dist.cdf(0));  // CDF(0) = P(X=0)

    // Monotonically increasing
    expectTrue(dist.cdf(0) < dist.cdf(1));
    expectTrue(dist.cdf(1) < dist.cdf(5));

    // Approaches 1
    expectTrue(dist.cdf(20) > 0.999);

    expectEq(0.0, dist.cdf(-1));
  };

  "mean and variance"_test = [] {
    // Mean = variance = λ
    expectEq(2.0, Poisson{2.0}.mean());
    expectEq(5.0, Poisson{5.0}.mean());

    expectEq(2.0, Poisson{2.0}.variance());
    expectEq(5.0, Poisson{5.0}.variance());

    for (double λ : {0.5, 2.5, 10.0}) {
      Poisson dist{λ};
      expectEq(dist.mean(), dist.variance());
    }
  };

  "sample"_test = [] {
    std::mt19937_64 g{42};
    Poisson dist{5.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      auto k = dist.sample(g);
      expectTrue(k >= 0);
      sum += static_cast<double>(k);
    }

    // Standard error = √(Var/N) = √(5/10000) ≈ 0.022
    // Tolerance of 0.15 is ~7 standard errors
    expectNear(5.0, sum / N, 0.15);
  };

  "sample edge cases"_test = [] {
    std::mt19937_64 g{42};

    // Small λ uses Knuth's algorithm
    Poisson small{0.5};
    double sum_small = 0.0;
    for (int i = 0; i < 5'000; ++i) {
      sum_small += static_cast<double>(small.sample(g));
    }
    expectNear(0.5, sum_small / 5'000, 0.05);

    // Large λ uses PTRD
    Poisson large{50.0};
    double sum_large = 0.0;
    for (int i = 0; i < 5'000; ++i) {
      sum_large += static_cast<double>(large.sample(g));
    }
    expectNear(50.0, sum_large / 5'000, 1.0);
  };

  "constexpr"_test = [] {
    constexpr Poisson dist{3.0};
    static_assert(dist.lambda() == 3.0);
    static_assert(dist.mean() == 3.0);
    static_assert(dist.variance() == 3.0);
  };

  return TestRegistry::result();
}
