#include "prob/uniform.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Uniform dist{0.0, 1.0};
    expectNear(1.0, dist.prob(0.5), 1e-10);
    expectNear(0.0, dist.logProb(0.5), 1e-10);

    Uniform dist2{0.0, 10.0};
    expectNear(0.1, dist2.prob(5.0), 1e-10);
    expectNear(-std::log(10.0), dist2.logProb(5.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Uniform dist{0.0, 10.0};
    expectNear(0.0, dist.unnormalizedLogProb(5.0), 1e-10);
  };

  "sample mean"_test = [] {
    Uniform dist{-5.0, 5.0};  // Mean = 0
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    expectNear(0.0, mean, 0.2);
  };

  "cdf"_test = [] {
    Uniform dist{0.0, 10.0};
    expectNear(0.0, dist.cdf(0.0), 1e-10);
    expectNear(0.5, dist.cdf(5.0), 1e-10);
    expectNear(1.0, dist.cdf(10.0), 1e-10);
  };

  "accessors"_test = [] {
    Uniform dist{2.0, 8.0};
    expectEq(2.0, dist.lo());
    expectEq(8.0, dist.hi());
    expectEq(5.0, dist.mean());
    expectEq(3.0, dist.variance());  // (8-2)²/12 = 36/12 = 3
  };

  return TestRegistry::result();
}
