#include "prob/beta.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    // Beta(1, 1) is Uniform(0, 1)
    Beta dist{1.0, 1.0};
    expectNear(0.0, dist.logProb(0.5), 1e-10);

    // Beta(2, 2) at x=0.5
    // B(2,2) = 1/6, log p(0.5) = log(0.5) + log(0.5) + log(6)
    Beta dist2{2.0, 2.0};
    double expected = std::log(0.5) + std::log(0.5) + std::log(6.0);
    expectNear(expected, dist2.logProb(0.5), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Beta dist{2.0, 2.0};
    // (α-1)log(x) + (β-1)log(1-x) = log(0.5) + log(0.5)
    double expected = std::log(0.5) + std::log(0.5);
    expectNear(expected, dist.unnormalizedLogProb(0.5), 1e-10);
  };

  "sample mean"_test = [] {
    Beta dist{2.0, 5.0};  // Mean = 2/7 ≈ 0.286
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    expectNear(2.0 / 7.0, mean, 0.05);
  };

  "accessors"_test = [] {
    Beta dist{2.0, 5.0};
    expectEq(2.0, dist.alpha());
    expectEq(5.0, dist.beta());
    expectNear(2.0 / 7.0, dist.mean(), 1e-10);

    // Variance = αβ / ((α+β)²(α+β+1))
    double var = (2.0 * 5.0) / (49.0 * 8.0);
    expectNear(var, dist.variance(), 1e-10);

    // Mode = (α-1)/(α+β-2) = 1/5
    expectNear(0.2, dist.mode(), 1e-10);
  };

  return TestRegistry::result();
}
