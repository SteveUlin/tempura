#include "prob/gamma.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    // Gamma(1, 1) = Exp(1), at x=1: log(1) - 1 = -1
    Gamma dist{1.0, 1.0};
    expectNear(-1.0, dist.logProb(1.0), 1e-10);

    // Gamma(2, 1) at x=1
    Gamma dist2{2.0, 1.0};
    expectNear(-1.0, dist2.logProb(1.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Gamma dist{2.0, 1.0};
    // (α-1)log(x) - βx = log(1) - 1 = -1
    expectNear(-1.0, dist.unnormalizedLogProb(1.0), 1e-10);
  };

  "sample mean"_test = [] {
    Gamma dist{3.0, 2.0};  // Mean = 3/2 = 1.5
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    expectNear(1.5, mean, 0.15);
  };

  "sample with alpha < 1"_test = [] {
    Gamma dist{0.5, 1.0};  // Mean = 0.5
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    expectNear(0.5, mean, 0.1);
  };

  "accessors"_test = [] {
    Gamma dist{3.0, 2.0};
    expectEq(3.0, dist.shape());
    expectEq(2.0, dist.rate());
    expectEq(1.5, dist.mean());
    expectEq(0.75, dist.variance());
  };

  return TestRegistry::result();
}
