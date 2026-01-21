#include "prob/binomial.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Binomial dist{10, 0.5};

    // P(X=5) for Binomial(10, 0.5) = C(10,5) * 0.5^10 = 252/1024 ≈ 0.246
    expectNear(0.24609375, dist.prob(5), 1e-10);

    // P(X=0) = 0.5^10
    expectNear(std::pow(0.5, 10), dist.prob(0), 1e-10);

    // P(X=10) = 0.5^10
    expectNear(std::pow(0.5, 10), dist.prob(10), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Binomial dist{10, 0.5};
    // k*log(p) + (n-k)*log(1-p) for k=5
    double expected = 5 * std::log(0.5) + 5 * std::log(0.5);
    expectNear(expected, dist.unnormalizedLogProb(5), 1e-10);
  };

  "sample mean"_test = [] {
    Binomial dist{20, 0.3};  // Mean = 6
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    // Mean = np = 6, variance = np(1-p) = 4.2, SE ≈ 0.02
    expectNear(6.0, mean, 0.3);
  };

  "accessors"_test = [] {
    Binomial dist{20, 0.3};
    expectEq(20, dist.n());
    expectEq(0.3, dist.p());
    expectEq(6.0, dist.mean());
    expectNear(4.2, dist.variance(), 1e-10);
  };

  return TestRegistry::result();
}
