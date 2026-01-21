#include "prob/bernoulli.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Bernoulli dist{0.7};

    expectNear(0.7, dist.prob(true), 1e-10);
    expectNear(0.3, dist.prob(false), 1e-10);
    expectNear(std::log(0.7), dist.logProb(true), 1e-10);
    expectNear(std::log(0.3), dist.logProb(false), 1e-10);

    // Also works with 1/0
    expectNear(0.7, dist.prob(1), 1e-10);
    expectNear(0.3, dist.prob(0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Bernoulli dist{0.7};
    // Same as logProb for Bernoulli
    expectNear(std::log(0.7), dist.unnormalizedLogProb(true), 1e-10);
    expectNear(std::log(0.3), dist.unnormalizedLogProb(false), 1e-10);
  };

  "sample proportion"_test = [] {
    Bernoulli dist{0.7};
    std::mt19937_64 rng{42};

    int count = 0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      if (dist.sample(rng)) {
        ++count;
      }
    }
    double empirical_p = static_cast<double>(count) / n;

    // SE = √(p(1-p)/N) ≈ 0.0046, tolerance = 10 SE
    expectNear(0.7, empirical_p, 0.05);
  };

  "accessors"_test = [] {
    Bernoulli dist{0.3};
    expectEq(0.3, dist.p());
    expectEq(0.3, dist.mean());
    expectNear(0.21, dist.variance(), 1e-10);  // 0.3 * 0.7
  };

  return TestRegistry::result();
}
