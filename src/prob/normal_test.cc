#include "prob/normal.h"

#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "prob and logProb"_test = [] {
    Normal dist{0.0, 1.0};

    // At mode: p(0) = 1/√(2π) ≈ 0.3989
    expectNear(1.0 / std::sqrt(2.0 * std::numbers::pi), dist.prob(0.0), 1e-10);

    // log(p(0)) = -log(√(2π)) ≈ -0.9189
    expectNear(-0.9189385332046727, dist.logProb(0.0), 1e-10);

    // N(5, 2) at mode
    Normal dist2{5.0, 2.0};
    expectNear(-std::numbers::ln2 - 0.9189385332046727, dist2.logProb(5.0), 1e-10);
  };

  "unnormalizedLogProb"_test = [] {
    Normal dist{0.0, 1.0};
    // At mode, unnormalized should be 0 (no quadratic term)
    expectNear(0.0, dist.unnormalizedLogProb(0.0), 1e-10);
    // At x=1, should be -0.5
    expectNear(-0.5, dist.unnormalizedLogProb(1.0), 1e-10);
  };

  "sample mean"_test = [] {
    Normal dist{10.0, 2.0};
    std::mt19937_64 rng{42};

    double sum = 0.0;
    constexpr int n = 10000;
    for (int i = 0; i < n; ++i) {
      sum += dist.sample(rng);
    }
    double mean = sum / n;

    // SE = σ/√N = 2/100 = 0.02, tolerance = 10 SE
    expectNear(10.0, mean, 0.2);
  };

  "accessors"_test = [] {
    Normal dist{3.0, 2.0};
    expectEq(3.0, dist.mu());
    expectEq(2.0, dist.sigma());
    expectEq(3.0, dist.mean());
    expectEq(4.0, dist.variance());
    expectEq(2.0, dist.stddev());
  };

  "different parameter types"_test = [] {
    Normal<float, double> dist{1.0f, 2.0};
    expectNear(-std::numbers::ln2 - 0.9189385332046727, dist.logProb(1.0), 1e-10);

    Normal<int, int> dist2{0, 1};
    expectTrue(std::isfinite(dist2.logProb(0.5)));
  };

  return TestRegistry::result();
}
