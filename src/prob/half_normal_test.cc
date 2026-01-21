#include "prob/half_normal.h"

#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    HalfNormal dist{1.0};

    // At x=0: p(0) = 2/√(2π) ≈ 0.7979
    expectNear(2.0 / std::sqrt(2.0 * std::numbers::pi), dist.prob(0.0), 1e-10);

    // log(p(0)) = log(2) - log(√(2π))
    expectNear(std::numbers::ln2 - 0.9189385332046727, dist.logProb(0.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    HalfNormal dist{1.0};
    expectNear(0.0, dist.unnormalizedLogProb(0.0), 1e-10);
    expectNear(-0.5, dist.unnormalizedLogProb(1.0), 1e-10);
  };

  "sample mean"_test = [] {
    HalfNormal dist{2.0};
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    // Mean of HalfNormal(σ) = σ√(2/π) ≈ σ * 0.7979
    double expected_mean = 2.0 * 0.7978845608028654;
    expectNear(expected_mean, mean, 0.2);
  };

  "accessors"_test = [] {
    HalfNormal dist{3.0};
    expectEq(3.0, dist.sigma());

    // Mean = σ√(2/π)
    expectNear(3.0 * 0.7978845608028654, dist.mean(), 1e-10);

    // Variance = σ²(1 - 2/π)
    expectNear(9.0 * 0.36338022763241866, dist.variance(), 1e-10);
  };

  return TestRegistry::result();
}
