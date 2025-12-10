#include "bayes/student_t.h"

#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "prob"_test = [] {
    StudentT dist{5.0};
    expectTrue(dist.prob(0.0) > dist.prob(1.0));   // peak at mode
    expectNear(dist.prob(1.0), dist.prob(-1.0));   // symmetry
    expectNear(std::log(dist.prob(1.0)), dist.logProb(1.0));

    // ν = 1 is Cauchy: p(x) = 1 / (π(1 + x²))
    StudentT cauchy{1.0};
    expectNear(1.0 / std::numbers::pi, cauchy.prob(0.0), 1e-10);
    expectNear(1.0 / (std::numbers::pi * 2.0), cauchy.prob(1.0), 1e-10);
  };

  "logProb"_test = [] {
    StudentT dist{5.0};
    expectNear(std::log(dist.prob(2.0)), dist.logProb(2.0));

    // Avoids underflow for large x
    expectTrue(std::isfinite(dist.logProb(100.0)));
    expectTrue(dist.logProb(100.0) < -10.0);
  };

  "cdf"_test = [] {
    StudentT dist{5.0};
    expectNear(0.5, dist.cdf(0.0), 1e-10);         // symmetry
    expectNear(dist.cdf(-1.0), 1.0 - dist.cdf(1.0), 1e-8);

    // Monotonic
    expectTrue(dist.cdf(-1.0) < dist.cdf(0.0));
    expectTrue(dist.cdf(0.0) < dist.cdf(1.0));

    // ν = 1 is Cauchy: F(x) = 0.5 + arctan(x)/π
    StudentT cauchy{1.0};
    expectNear(0.5 + std::atan(1.0) / std::numbers::pi, cauchy.cdf(1.0), 1e-6);
  };

  "mean and variance"_test = [] {
    // E[X] = 0 for ν > 1, undefined for ν ≤ 1
    expectEq(0.0, StudentT{5.0}.mean());
    expectTrue(std::isnan(StudentT{1.0}.mean()));

    // Var[X] = ν/(ν-2) for ν > 2
    expectNear(5.0 / 3.0, StudentT{5.0}.variance());
    expectNear(1.0, StudentT{100.0}.variance(), 0.03);  // → 1 as ν → ∞

    // Var = ∞ for 1 < ν ≤ 2, undefined for ν ≤ 1
    expectTrue(std::isinf(StudentT{2.0}.variance()));
    expectTrue(std::isnan(StudentT{1.0}.variance()));
  };

  "sample"_test = [] {
    std::mt19937_64 g{42};
    StudentT dist{10.0};

    const int N = 10'000;
    double sum = 0.0;
    for (int i = 0; i < N; ++i) {
      sum += dist.sample(g);
    }

    // Standard error = √(Var/N) = √(1.25/10000) ≈ 0.011
    // Tolerance of 0.1 is ~9 standard errors
    expectNear(0.0, sum / N, 0.1);
  };

  "constexpr"_test = [] {
    // Note: lgamma is not constexpr, so can't use static_assert
    StudentT dist{5.0};
    expectEq(5.0, dist.nu());
    expectEq(0.0, dist.mean());
    expectNear(5.0 / 3.0, dist.variance());
  };

  return TestRegistry::result();
}
