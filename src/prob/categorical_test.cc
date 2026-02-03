// Tests for Categorical distribution
#include "prob/categorical.h"
#include "unit.h"

#include <cmath>
#include <random>

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "Categorical probs sum to 1"_test = [] {
    Categorical<3> cat{0.2, 0.3, 0.5};
    auto p = cat.probs();
    double sum = p[0] + p[1] + p[2];
    expectNear(1.0, sum, 1e-10);
  };

  "Categorical normalizes unnormalized probs"_test = [] {
    Categorical<3> cat{2.0, 3.0, 5.0};  // Unnormalized weights
    auto p = cat.probs();
    expectNear(0.2, p[0], 1e-10);
    expectNear(0.3, p[1], 1e-10);
    expectNear(0.5, p[2], 1e-10);
  };

  "Categorical logProb"_test = [] {
    Categorical<3> cat{0.2, 0.3, 0.5};
    expectNear(std::log(0.2), cat.logProb(0), 1e-10);
    expectNear(std::log(0.3), cat.logProb(1), 1e-10);
    expectNear(std::log(0.5), cat.logProb(2), 1e-10);
  };

  "Categorical sample distribution"_test = [] {
    Categorical<3> cat{0.2, 0.3, 0.5};
    std::mt19937 rng{42};

    constexpr int N = 10000;
    std::array<int, 3> counts = {0, 0, 0};
    for (int i = 0; i < N; ++i) {
      auto k = cat.sample(rng);
      ++counts[k];
    }

    // SE = sqrt(p(1-p)/N) ≈ 0.005 for p=0.5
    // Use tolerance of ~5 SE
    expectNear(0.2, static_cast<double>(counts[0]) / N, 0.03);
    expectNear(0.3, static_cast<double>(counts[1]) / N, 0.03);
    expectNear(0.5, static_cast<double>(counts[2]) / N, 0.03);
  };

  "Categorical mode"_test = [] {
    Categorical<4> cat{0.1, 0.4, 0.3, 0.2};
    expectEq(cat.mode(), 1UL);
  };

  "Categorical mean for ordered categories"_test = [] {
    // If categories represent 0, 1, 2
    Categorical<3> cat{0.2, 0.3, 0.5};
    // E[X] = 0*0.2 + 1*0.3 + 2*0.5 = 1.3
    expectNear(1.3, cat.mean(), 1e-10);
  };

  "CategoricalDynamic works"_test = [] {
    CategoricalDynamic cat{{0.25, 0.25, 0.25, 0.25}};
    std::mt19937 rng{42};

    auto k = cat.sample(rng);
    expectTrue(k < 4);
    expectNear(0.25, cat.prob(k), 1e-10);
  };

  "Categorical degenerate case"_test = [] {
    // One probability is 1.0, others are 0
    Categorical<3> cat{0.0, 1.0, 0.0};
    std::mt19937 rng{42};

    for (int i = 0; i < 100; ++i) {
      expectEq(cat.sample(rng), 1UL);
    }
  };

  return TestRegistry::result();
}
