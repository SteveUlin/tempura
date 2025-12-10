#include "bayes/uniform.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Uniform dist{2.0, 5.0};
    expectNear(1.0 / 3.0, dist.prob(2.5));
    expectNear(1.0 / 3.0, dist.prob(4.0));

    // Out of bounds
    expectEq(0.0, dist.prob(1.0));
    expectEq(0.0, dist.prob(6.0));
  };

  "logProb"_test = [] {
    Uniform dist{1.0, 4.0};
    expectNear(-std::log(3.0), dist.logProb(2.0));
    expectNear(std::log(dist.prob(2.0)), dist.logProb(2.0));

    // Out of bounds → -∞
    expectEq(-std::numeric_limits<double>::infinity(), dist.logProb(0.5));
    expectEq(-std::numeric_limits<double>::infinity(), dist.logProb(5.0));
  };

  "cdf"_test = [] {
    Uniform dist{0.0, 10.0};
    expectEq(0.0, dist.cdf(-1.0));
    expectEq(0.0, dist.cdf(0.0));
    expectEq(1.0, dist.cdf(10.0));
    expectEq(1.0, dist.cdf(11.0));

    // Linear in [a,b]
    expectNear(0.25, dist.cdf(2.5));
    expectNear(0.5, dist.cdf(5.0));
    expectNear(0.75, dist.cdf(7.5));
  };

  "mean and variance"_test = [] {
    expectEq(2.5, Uniform{0.0, 5.0}.mean());
    expectEq(5.0, Uniform{2.0, 8.0}.mean());

    // Variance = (b-a)²/12
    expectNear(25.0 / 12.0, Uniform{0.0, 5.0}.variance());
    expectNear(36.0 / 12.0, Uniform{2.0, 8.0}.variance());
  };

  "sample"_test = [] {
    std::mt19937_64 g{42};
    Uniform dist{0.0, 10.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      double x = dist.sample(g);
      expectTrue(x >= 0.0 && x <= 10.0);
      sum += x;
    }

    // Standard error = √(Var/N) = √((100/12)/10000) ≈ 0.029
    // Tolerance of 0.2 is ~7 standard errors
    expectNear(5.0, sum / N, 0.2);
  };

  "constexpr"_test = [] {
    constexpr Uniform dist{0.0, 2.0};
    static_assert(dist.lower() == 0.0);
    static_assert(dist.upper() == 2.0);
    static_assert(dist.mean() == 1.0);
    static_assert(dist.prob(1.0) == 0.5);
    static_assert(dist.cdf(1.0) == 0.5);
  };

  return TestRegistry::result();
}
