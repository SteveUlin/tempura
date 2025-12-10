#include "bayes/bernoulli.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Bernoulli dist{0.7};
    expectNear(0.7, dist.prob(true));
    expectNear(0.3, dist.prob(false));

    // Degenerate cases: certain failure / certain success
    expectNear(0.0, Bernoulli{0.0}.prob(true));
    expectNear(1.0, Bernoulli{1.0}.prob(true));
  };

  "logProb"_test = [] {
    Bernoulli dist{0.8};
    expectNear(std::log(0.8), dist.logProb(true));
    expectNear(std::log(0.2), dist.logProb(false));

    // Impossible events have log-probability -∞
    expectTrue(std::isinf(Bernoulli{0.0}.logProb(true)));   // can't get 1 when p=0
    expectTrue(std::isinf(Bernoulli{1.0}.logProb(false)));  // can't get 0 when p=1
  };

  "cdf"_test = [] {
    Bernoulli dist{0.8};
    expectNear(0.2, dist.cdf(false));  // P(X ≤ 0) = 1 - p
    expectNear(1.0, dist.cdf(true));   // P(X ≤ 1) = 1
  };

  "mean and variance"_test = [] {
    expectNear(0.3, Bernoulli{0.3}.mean());

    // Variance = p(1-p), maximized at p=0.5
    expectNear(0.25, Bernoulli{0.5}.variance());
    expectNear(0.0, Bernoulli{0.0}.variance());
    expectNear(0.0, Bernoulli{1.0}.variance());

    // Symmetric: Var(p) = Var(1-p)
    expectNear(Bernoulli{0.3}.variance(), Bernoulli{0.7}.variance());
  };

  "sample"_test = [] {
    std::mt19937 g{42};
    Bernoulli dist{0.7};

    const int N = 10'000;
    int count = 0;
    for (int i = 0; i < N; ++i) {
      if (dist.sample(g)) ++count;
    }

    // Standard error = √(p(1-p)/N) ≈ 0.0046 for p=0.7, N=10k
    // Tolerance of 0.1 is ~20 standard errors (effectively never fails)
    expectNear(0.7, static_cast<double>(count) / N, 0.1);
  };

  "sample edge cases"_test = [] {
    std::mt19937 g{0};

    Bernoulli always_false{0.0};
    Bernoulli always_true{1.0};

    for (int i = 0; i < 100; ++i) {
      expectFalse(always_false.sample(g));
      expectTrue(always_true.sample(g));
    }
  };

  "constexpr"_test = [] {
    constexpr Bernoulli dist{0.3};
    static_assert(dist.p() == 0.3);
    static_assert(dist.mean() == 0.3);
    static_assert(dist.prob(true) == 0.3);
    static_assert(dist.variance() == 0.3 * 0.7);
  };

  return TestRegistry::result();
}
