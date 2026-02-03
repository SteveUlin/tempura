// Tests for Multinomial distribution
#include "prob/multinomial.h"
#include "unit.h"

#include <cmath>
#include <numeric>
#include <random>

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "Multinomial sample sums to n"_test = [] {
    Multinomial<3> mult{10, {0.2, 0.3, 0.5}};
    std::mt19937 rng{42};

    for (int i = 0; i < 100; ++i) {
      auto x = mult.sample(rng);
      int64_t sum = x[0] + x[1] + x[2];
      expectEq(sum, int64_t{10});
      for (auto xi : x) {
        expectTrue(xi >= 0);
      }
    }
  };

  "Multinomial mean"_test = [] {
    Multinomial<3> mult{100, {0.2, 0.3, 0.5}};
    auto m = mult.mean();
    expectNear(20.0, m[0], 1e-10);
    expectNear(30.0, m[1], 1e-10);
    expectNear(50.0, m[2], 1e-10);
  };

  "Multinomial variance"_test = [] {
    Multinomial<3> mult{100, {0.2, 0.3, 0.5}};
    auto v = mult.variance();
    // Var = n * p * (1-p)
    expectNear(100 * 0.2 * 0.8, v[0], 1e-10);
    expectNear(100 * 0.3 * 0.7, v[1], 1e-10);
    expectNear(100 * 0.5 * 0.5, v[2], 1e-10);
  };

  "Multinomial logProb basic"_test = [] {
    Multinomial<2> mult{5, {0.4, 0.6}};
    // P(X=[2,3]) = C(5,2) * 0.4^2 * 0.6^3
    // = 10 * 0.16 * 0.216 = 0.3456
    std::array<int64_t, 2> x = {2, 3};
    double lp = mult.logProb(x);
    double expected = std::log(10.0) + 2.0 * std::log(0.4) + 3.0 * std::log(0.6);
    expectNear(expected, lp, 1e-10);
  };

  "Multinomial logProb invalid sum returns -inf"_test = [] {
    Multinomial<3> mult{10, {0.2, 0.3, 0.5}};
    std::array<int64_t, 3> x = {2, 3, 4};  // Sum = 9, should be 10
    double lp = mult.logProb(x);
    expectTrue(std::isinf(lp) && lp < 0);
  };

  "Multinomial sample mean converges"_test = [] {
    Multinomial<3> mult{100, {0.2, 0.3, 0.5}};
    std::mt19937 rng{123};

    constexpr int N = 1000;
    std::array<double, 3> sum = {0.0, 0.0, 0.0};
    for (int i = 0; i < N; ++i) {
      auto x = mult.sample(rng);
      sum[0] += static_cast<double>(x[0]);
      sum[1] += static_cast<double>(x[1]);
      sum[2] += static_cast<double>(x[2]);
    }

    // Expected: [20, 30, 50]
    // SE = sqrt(Var/N) = sqrt(16/1000) ≈ 0.13 for category 0
    expectNear(20.0, sum[0] / N, 1.0);
    expectNear(30.0, sum[1] / N, 1.0);
    expectNear(50.0, sum[2] / N, 1.0);
  };

  "MultinomialDynamic works"_test = [] {
    MultinomialDynamic mult{10, {0.25, 0.25, 0.25, 0.25}};
    std::mt19937 rng{42};

    auto x = mult.sample(rng);
    expectEq(x.size(), 4UL);
    int64_t sum = std::accumulate(x.begin(), x.end(), int64_t{0});
    expectEq(sum, int64_t{10});
  };

  "Multinomial n=0 returns all zeros"_test = [] {
    Multinomial<3> mult{0, {0.2, 0.3, 0.5}};
    std::mt19937 rng{42};
    auto x = mult.sample(rng);
    expectEq(x[0], int64_t{0});
    expectEq(x[1], int64_t{0});
    expectEq(x[2], int64_t{0});
  };

  return TestRegistry::result();
}
