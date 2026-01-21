#include "prob/poisson.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Poisson dist{5.0};

    // P(X=0) = e^(-5)
    expectNear(std::exp(-5.0), dist.prob(0), 1e-10);

    // P(X=5) = 5^5 * e^(-5) / 5! = 3125 * e^(-5) / 120
    double expected = 3125.0 * std::exp(-5.0) / 120.0;
    expectNear(expected, dist.prob(5), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Poisson dist{5.0};
    // k*log(λ) - lgamma(k+1) for k=5
    double expected = 5 * std::log(5.0) - std::lgamma(6.0);
    expectNear(expected, dist.unnormalizedLogProb(5), 1e-10);
  };

  "sample mean small lambda"_test = [] {
    Poisson dist{3.0};  // Mean = 3
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    // SE = √λ/√N = √3/100 ≈ 0.017
    expectNear(3.0, mean, 0.2);
  };

  "sample mean large lambda"_test = [] {
    Poisson dist{50.0};  // Uses PTRD algorithm
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    expectNear(50.0, mean, 1.0);
  };

  "accessors"_test = [] {
    Poisson dist{7.0};
    expectEq(7.0, dist.lambda());
    expectEq(7.0, dist.mean());
    expectEq(7.0, dist.variance());
  };

  return TestRegistry::result();
}
