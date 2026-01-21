#include "prob/logistic.h"

#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Logistic dist{0.0, 1.0};

    // At mode: p(0) = 1/4 (exp(0)/(1+exp(0))² = 1/4)
    expectNear(0.25, dist.prob(0.0), 1e-10);

    // log(p(0)) = log(1/4) = -log(4)
    expectNear(-std::log(4.0), dist.logProb(0.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Logistic dist{0.0, 1.0};
    // At mode, unnormalized = -|z| - 2*log(1 + exp(-|z|)) = 0 - 2*log(2)
    expectNear(-2.0 * std::numbers::ln2, dist.unnormalizedLogProb(0.0), 1e-10);
  };

  "sample mean"_test = [] {
    Logistic dist{5.0, 2.0};
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    expectNear(5.0, mean, 0.3);
  };

  "cdf is sigmoid"_test = [] {
    Logistic dist{0.0, 1.0};
    expectNear(0.5, dist.cdf(0.0), 1e-10);

    // cdf(x) = 1/(1+exp(-x))
    double expected = 1.0 / (1.0 + std::exp(-1.0));
    expectNear(expected, dist.cdf(1.0), 1e-10);
  };

  "cdf and invCdf"_test = [] {
    Logistic dist{0.0, 1.0};
    expectNear(0.0, dist.invCdf(0.5), 1e-10);
    expectNear(1.0, dist.invCdf(dist.cdf(1.0)), 1e-10);
  };

  "accessors"_test = [] {
    Logistic dist{3.0, 2.0};
    expectEq(3.0, dist.location());
    expectEq(2.0, dist.scale());
    expectEq(3.0, dist.mean());

    // Variance = π²s²/3
    double expected_var =
        (std::numbers::pi * std::numbers::pi * 4.0) / 3.0;
    expectNear(expected_var, dist.variance(), 1e-10);
  };

  return TestRegistry::result();
}
