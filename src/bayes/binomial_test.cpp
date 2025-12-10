#include "bayes/binomial.h"

#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    Binomial dist{10, 0.5};
    expectNear(1.0 / 1024.0, dist.prob(0));
    expectNear(252.0 / 1024.0, dist.prob(5));
    expectNear(1.0 / 1024.0, dist.prob(10));

    // Out of bounds
    expectNear(0.0, dist.prob(-1));
    expectNear(0.0, dist.prob(11));

    // Edge cases: p=0 means only k=0 is possible
    expectNear(1.0, Binomial{10, 0.0}.prob(0));
    expectNear(0.0, Binomial{10, 0.0}.prob(1));

    // Edge cases: p=1 means only k=n is possible
    expectNear(0.0, Binomial{10, 1.0}.prob(0));
    expectNear(1.0, Binomial{10, 1.0}.prob(10));

    // Binomial(1, p) = Bernoulli(p)
    expectNear(0.3, Binomial{1, 0.7}.prob(0));
    expectNear(0.7, Binomial{1, 0.7}.prob(1));
  };

  "logProb"_test = [] {
    Binomial dist{10, 0.5};
    expectNear(std::log(dist.prob(5)), dist.logProb(5));

    // Out of bounds → -∞
    expectTrue(std::isinf(dist.logProb(-1)) && dist.logProb(-1) < 0);
    expectTrue(std::isinf(dist.logProb(11)) && dist.logProb(11) < 0);

    // Impossible events have log-probability -∞
    expectTrue(std::isinf(Binomial{10, 0.0}.logProb(1)));
    expectTrue(std::isinf(Binomial{10, 1.0}.logProb(0)));

    // Certain events have log-probability 0
    expectNear(0.0, Binomial{10, 0.0}.logProb(0));
    expectNear(0.0, Binomial{10, 1.0}.logProb(10));
  };

  "cdf"_test = [] {
    Binomial dist{10, 0.5};
    expectNear(0.0, dist.cdf(-1));
    expectNear(1.0, dist.cdf(10));

    // CDF(0) = P(X=0)
    expectNear(dist.prob(0), dist.cdf(0));

    // Monotonically increasing
    for (int64_t k = 0; k < 10; ++k) {
      expectTrue(dist.cdf(k) <= dist.cdf(k + 1));
    }

    // Via summation
    Binomial dist2{5, 0.3};
    expectNear(dist2.prob(0) + dist2.prob(1) + dist2.prob(2), dist2.cdf(2), 1e-9);
  };

  "mean and variance"_test = [] {
    expectNear(3.0, Binomial{10, 0.3}.mean());
    expectNear(5.0, Binomial{10, 0.5}.mean());

    // Variance = np(1-p), maximized at p=0.5
    expectNear(2.5, Binomial{10, 0.5}.variance());
    expectNear(0.0, Binomial{10, 0.0}.variance());
    expectNear(0.0, Binomial{10, 1.0}.variance());

    // Symmetric: Var(p) = Var(1-p)
    expectNear(Binomial{10, 0.3}.variance(), Binomial{10, 0.7}.variance());

    // Binomial(1, p) variance = p(1-p) = Bernoulli(p) variance
    expectNear(0.25, Binomial{1, 0.5}.variance());
  };

  "sample"_test = [] {
    std::mt19937 g{42};
    Binomial dist{100, 0.3};

    const int N = 10'000;
    int64_t sum = 0;
    for (int i = 0; i < N; ++i) {
      int64_t sample = dist.sample(g);
      expectTrue(sample >= 0 && sample <= 100);
      sum += sample;
    }

    // Standard error = √(Var/N) = √(21/10000) ≈ 0.046
    // Tolerance of 1.0 is ~22 standard errors
    expectNear(30.0, static_cast<double>(sum) / N, 1.0);
  };

  "sample edge cases"_test = [] {
    std::mt19937 g{0};

    Binomial always_zero{10, 0.0};
    Binomial always_n{10, 1.0};

    for (int i = 0; i < 100; ++i) {
      expectEq(0, always_zero.sample(g));
      expectEq(10, always_n.sample(g));
    }
  };

  "constexpr"_test = [] {
    constexpr Binomial dist{10, 0.3};
    static_assert(dist.n() == 10);
    static_assert(dist.p() == 0.3);
    static_assert(dist.mean() == 3.0);
    expectNear(2.1, dist.variance());
  };

  "different integer types"_test = [] {
    Binomial<double, int32_t> dist32{10, 0.5};
    expectEq(10, dist32.n());
    expectNear(5.0, dist32.mean());

    Binomial<double, uint32_t> distu{100u, 0.1};
    expectEq(100u, distu.n());
    expectNear(10.0, distu.mean());

    std::mt19937 g{42};
    expectTrue(dist32.sample(g) >= 0 && dist32.sample(g) <= 10);
  };

  "different floating types"_test = [] {
    Binomial<float> f{10, 0.5f};
    Binomial<double> d{10, 0.5};
    Binomial<long double> ld{10, 0.5L};

    expectNear(5.0f, f.mean());
    expectNear(5.0, d.mean());
    expectNear(5.0L, ld.mean());
  };

  "large n extreme p"_test = [] {
    // Test log-space computation avoids overflow
    Binomial dist1{1000, 0.01};
    expectNear(10.0, dist1.mean());
    expectTrue(dist1.prob(10) > 0.0 && std::isfinite(dist1.prob(10)));

    Binomial dist2{1000, 0.99};
    expectNear(990.0, dist2.mean());
    expectTrue(dist2.prob(990) > 0.0 && std::isfinite(dist2.prob(990)));
  };

  return TestRegistry::result();
}
