// Tests for Dirichlet distribution
#include "prob/dirichlet.h"
#include "unit.h"

#include <cmath>
#include <numeric>
#include <random>

using namespace tempura;
using namespace tempura::prob;

auto main() -> int {
  "Dirichlet with K=3 samples sum to 1"_test = [] {
    Dirichlet<3> dir{2.0, 3.0, 5.0};
    std::mt19937 rng{42};

    for (int i = 0; i < 100; ++i) {
      auto x = dir.sample(rng);
      double sum = x[0] + x[1] + x[2];
      expectNear(1.0, sum, 1e-10);
      for (double xi : x) {
        expectTrue(xi >= 0.0 && xi <= 1.0);
      }
    }
  };

  "Dirichlet mean is correct"_test = [] {
    Dirichlet<3> dir{2.0, 3.0, 5.0};
    auto m = dir.mean();
    // Mean = alpha_k / sum(alpha) = [0.2, 0.3, 0.5]
    expectNear(0.2, m[0], 1e-10);
    expectNear(0.3, m[1], 1e-10);
    expectNear(0.5, m[2], 1e-10);
  };

  "Dirichlet logProb basic"_test = [] {
    Dirichlet<2> dir{2.0, 3.0};  // Equivalent to Beta(2, 3)
    std::array<double, 2> x = {0.4, 0.6};
    double lp = dir.logProb(x);

    // Manual calculation: log B(2,3) + (2-1)*log(0.4) + (3-1)*log(0.6)
    // B(2,3) = Gamma(2)*Gamma(3)/Gamma(5) = 1*2/24 = 1/12
    // log B(2,3) = log(1/12) = -log(12)
    double log_beta = std::lgamma(2) + std::lgamma(3) - std::lgamma(5);
    double expected = -log_beta + std::log(0.4) + 2.0 * std::log(0.6);
    expectNear(expected, lp, 1e-10);
  };

  "Dirichlet sample mean converges"_test = [] {
    Dirichlet<3> dir{10.0, 20.0, 30.0};
    std::mt19937 rng{123};

    constexpr int N = 10000;
    std::array<double, 3> sum = {0.0, 0.0, 0.0};
    for (int i = 0; i < N; ++i) {
      auto x = dir.sample(rng);
      sum[0] += x[0];
      sum[1] += x[1];
      sum[2] += x[2];
    }

    // Expected: [1/6, 1/3, 1/2]
    // SE = sqrt(Var/N), Var for Dirichlet component ≈ 0.003, so SE ≈ 0.0005
    expectNear(1.0 / 6.0, sum[0] / N, 0.02);
    expectNear(1.0 / 3.0, sum[1] / N, 0.02);
    expectNear(1.0 / 2.0, sum[2] / N, 0.02);
  };

  "DirichletDynamic works"_test = [] {
    DirichletDynamic dir{{1.0, 1.0, 1.0}};  // Uniform on simplex
    std::mt19937 rng{42};

    auto x = dir.sample(rng);
    expectEq(x.size(), 3UL);
    double sum = std::accumulate(x.begin(), x.end(), 0.0);
    expectNear(1.0, sum, 1e-10);
  };

  "Dirichlet symmetric case"_test = [] {
    // Dirichlet([1,1,1]) is uniform on the 2-simplex
    Dirichlet<3> dir{1.0, 1.0, 1.0};
    std::array<double, 3> x1 = {0.2, 0.3, 0.5};
    std::array<double, 3> x2 = {0.5, 0.3, 0.2};

    // For uniform Dirichlet, all valid points have equal probability
    // The log-prob should be -log(area of simplex) = -log(1/2) = log(2)
    // But unnormalized should be 0 since (alpha-1)=0
    double lp1 = dir.unnormalizedLogProb(std::span<const double, 3>(x1));
    double lp2 = dir.unnormalizedLogProb(std::span<const double, 3>(x2));
    expectNear(0.0, lp1, 1e-10);
    expectNear(0.0, lp2, 1e-10);
  };

  return TestRegistry::result();
}
