#include "bayes/beta.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Beta dist{2.0, 5.0};
    expectTrue(dist.prob(0.3) > 0.0);
    expectNear(std::log(dist.prob(0.3)), dist.logProb(0.3));

    // Out of bounds
    expectEq(0.0, dist.prob(-0.1));
    expectEq(0.0, dist.prob(1.1));

    // Beta(1,1) = Uniform(0,1) with constant PDF = 1
    expectNear(1.0, Beta{1.0, 1.0}.prob(0.0));
    expectNear(1.0, Beta{1.0, 1.0}.prob(0.5));
    expectNear(1.0, Beta{1.0, 1.0}.prob(1.0));

    // α,β > 1: density → 0 at boundaries
    expectNear(0.0, Beta{2.0, 2.0}.prob(0.0));
    expectNear(0.0, Beta{2.0, 2.0}.prob(1.0));

    // α,β < 1 (U-shaped): density → +∞ at boundaries
    expectTrue(std::isinf(Beta{0.5, 0.5}.prob(0.0)));
    expectTrue(std::isinf(Beta{0.5, 0.5}.prob(1.0)));
  };

  "logProb"_test = [] {
    Beta dist{2.0, 5.0};
    expectNear(std::log(dist.prob(0.3)), dist.logProb(0.3));

    // Out of bounds → -∞
    expectTrue(std::isinf(dist.logProb(-0.1)) && dist.logProb(-0.1) < 0);

    // α,β > 1 at boundaries → -∞ (log of 0)
    expectTrue(std::isinf(Beta{2.0, 2.0}.logProb(0.0)));

    // α,β < 1 at boundaries → +∞ (log of +∞)
    expectTrue(Beta{0.5, 0.5}.logProb(0.0) > 0);
  };

  "cdf"_test = [] {
    Beta dist{2.0, 5.0};
    expectEq(0.0, dist.cdf(0.0));
    expectEq(1.0, dist.cdf(1.0));

    // Monotonically increasing
    expectTrue(dist.cdf(0.3) < dist.cdf(0.5));

    // Beta(1,1) = Uniform: CDF(x) = x
    expectNear(0.5, Beta{1.0, 1.0}.cdf(0.5));
    expectNear(0.25, Beta{1.0, 1.0}.cdf(0.25));
  };

  "mean and variance"_test = [] {
    // E[X] = α/(α+β)
    expectNear(0.5, Beta{1.0, 1.0}.mean());
    expectNear(2.0 / 7.0, Beta{2.0, 5.0}.mean());

    // Var[Uniform(0,1)] = 1/12
    expectNear(1.0 / 12.0, Beta{1.0, 1.0}.variance());

    // Var[Beta(2,2)] = (2·2)/((4)²·5) = 4/80 = 0.05
    expectNear(0.05, Beta{2.0, 2.0}.variance());
  };

  "mode"_test = [] {
    // Mode = (α-1)/(α+β-2) for α,β > 1
    expectNear(1.0 / 3.0, Beta{3.0, 5.0}.mode());
    expectNear(0.5, Beta{4.0, 4.0}.mode());

    // Undefined for α ≤ 1 or β ≤ 1 (U-shaped or uniform)
    expectTrue(std::isnan(Beta{0.5, 0.5}.mode()));
    expectTrue(std::isnan(Beta{1.0, 1.0}.mode()));
  };

  "sample"_test = [] {
    std::mt19937_64 g{42};
    Beta dist{2.0, 5.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      double x = dist.sample(g);
      expectTrue(x >= 0.0 && x <= 1.0);
      sum += x;
    }

    // Standard error = √(Var/N) ≈ √(0.026/10000) ≈ 0.0016
    // Tolerance of 0.02 is ~12 standard errors
    expectNear(2.0 / 7.0, sum / N, 0.02);
  };

  "constexpr"_test = [] {
    constexpr Beta dist{2.0, 5.0};
    static_assert(dist.alpha() == 2.0);
    static_assert(dist.beta() == 5.0);
    static_assert(dist.mean() == 2.0 / 7.0);
    static_assert(dist.variance() == 10.0 / 392.0);
  };

  return TestRegistry::result();
}
