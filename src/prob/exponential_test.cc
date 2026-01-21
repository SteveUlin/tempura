#include "prob/exponential.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Exponential dist{2.0};

    // p(0) = λ = 2
    expectNear(2.0, dist.prob(0.0), 1e-10);

    // log(p(1)) = log(2) - 2
    expectNear(std::log(2.0) - 2.0, dist.logProb(1.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Exponential dist{2.0};
    expectNear(0.0, dist.unnormalizedLogProb(0.0), 1e-10);
    expectNear(-2.0, dist.unnormalizedLogProb(1.0), 1e-10);
  };

  "sample mean"_test = [] {
    Exponential dist{0.5};  // Mean = 2
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    // Mean = 1/λ = 2, variance = 1/λ² = 4, SE = 0.02
    expectNear(2.0, mean, 0.2);
  };

  "cdf and invCdf"_test = [] {
    Exponential dist{1.0};
    expectNear(0.0, dist.cdf(0.0), 1e-10);
    expectNear(1.0 - std::exp(-1.0), dist.cdf(1.0), 1e-10);

    expectNear(0.0, dist.invCdf(0.0), 1e-10);
    expectNear(1.0, dist.invCdf(1.0 - std::exp(-1.0)), 1e-10);
  };

  "accessors"_test = [] {
    Exponential dist{0.5};
    expectEq(0.5, dist.lambda());
    expectEq(2.0, dist.mean());
    expectEq(4.0, dist.variance());
  };

  return TestRegistry::result();
}
