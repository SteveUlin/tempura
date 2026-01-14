// Example: 2D Metropolis-Hastings using std::array
//
// Demonstrates the built-in GaussianWalkND and UniformWalkND proposals.

#include "bayes/estimation/metropolis_hastings.h"

#include <array>
#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

using State2D = std::array<double, 2>;

auto main() -> int {
  "2D standard normal"_test = [] {
    // Target: independent N(0,1) in each dimension
    // log p(x,y) = -x²/2 - y²/2
    auto log_target = [](const State2D& s) {
      return -s[0] * s[0] / 2.0 - s[1] * s[1] / 2.0;
    };

    auto kernel =
        makeMetropolisHastings<State2D>(log_target, GaussianWalkND<double, 2>{0.5});

    Chain chain{kernel, std::mt19937_64{42}, State2D{0.0, 0.0}};

    chain.advance(1000);  // burn-in
    auto samples = chain.take(10'000);

    // Compute means
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (const auto& s : samples) {
      sum_x += s[0];
      sum_y += s[1];
    }
    double n = static_cast<double>(samples.size());
    double mean_x = sum_x / n;
    double mean_y = sum_y / n;

    // Compute variances
    double var_x = 0.0;
    double var_y = 0.0;
    for (const auto& s : samples) {
      var_x += (s[0] - mean_x) * (s[0] - mean_x);
      var_y += (s[1] - mean_y) * (s[1] - mean_y);
    }
    var_x /= n;
    var_y /= n;

    expectNear(0.0, mean_x, 0.1);
    expectNear(0.0, mean_y, 0.1);
    expectNear(1.0, var_x, 0.15);
    expectNear(1.0, var_y, 0.15);
  };

  "2D correlated normal"_test = [] {
    // Target: bivariate normal with correlation ρ = 0.8
    // Σ = [[1, 0.8], [0.8, 1]], Σ⁻¹ = [[2.78, -2.22], [-2.22, 2.78]]
    auto log_target = [](const State2D& s) {
      double x = s[0];
      double y = s[1];
      // Precision matrix for ρ=0.8: 1/(1-ρ²) * [[1, -ρ], [-ρ, 1]]
      double inv_det = 1.0 / (1.0 - 0.64);  // 1/(1-0.8²) = 2.78
      return -0.5 * inv_det * (x * x - 2.0 * 0.8 * x * y + y * y);
    };

    auto kernel =
        makeMetropolisHastings<State2D>(log_target, GaussianWalkND<double, 2>{0.5});

    Chain chain{kernel, std::mt19937_64{123}, State2D{0.0, 0.0}};

    chain.advance(2000);
    auto samples = chain.take(20'000);

    // Compute correlation
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (const auto& s : samples) {
      sum_x += s[0];
      sum_y += s[1];
    }
    double n = static_cast<double>(samples.size());
    double mean_x = sum_x / n;
    double mean_y = sum_y / n;

    double cov_xy = 0.0;
    double var_x = 0.0;
    double var_y = 0.0;
    for (const auto& s : samples) {
      double dx = s[0] - mean_x;
      double dy = s[1] - mean_y;
      cov_xy += dx * dy;
      var_x += dx * dx;
      var_y += dy * dy;
    }
    double corr = cov_xy / std::sqrt(var_x * var_y);

    expectNear(0.8, corr, 0.05);
  };

  "2D banana distribution"_test = [] {
    // Banana-shaped distribution: twist a normal
    // p(x,y) ∝ exp(-x²/2) * exp(-(y - x²)²/2)
    auto log_target = [](const State2D& s) {
      double x = s[0];
      double y = s[1];
      double residual = y - x * x;
      return -x * x / 2.0 - residual * residual / 2.0;
    };

    auto kernel =
        makeMetropolisHastings<State2D>(log_target, GaussianWalkND<double, 2>{0.8});

    Chain chain{kernel, std::mt19937_64{777}, State2D{0.0, 0.0}};

    chain.advance(5000);
    auto samples = chain.take(20'000);

    // x should be ~ N(0,1)
    double sum_x = 0.0;
    for (const auto& s : samples) {
      sum_x += s[0];
    }
    double n = static_cast<double>(samples.size());
    double mean_x = sum_x / n;
    expectNear(0.0, mean_x, 0.1);

    // y should have mean ≈ E[x²] = 1 (variance of x)
    double sum_y = 0.0;
    for (const auto& s : samples) {
      sum_y += s[1];
    }
    double mean_y = sum_y / n;
    expectNear(1.0, mean_y, 0.2);
  };

  "2D acceptance rate with UniformWalkND"_test = [] {
    auto log_target = [](const State2D& s) {
      return -s[0] * s[0] / 2.0 - s[1] * s[1] / 2.0;
    };

    auto kernel =
        makeMetropolisHastings<State2D>(log_target, UniformWalkND<double, 2>{1.0});

    Chain chain{kernel, std::mt19937_64{42}, State2D{0.0, 0.0}};

    chain.advance(1000);
    auto [samples, rate] = chain.takeWithStats(5000);

    // 2D typically has lower acceptance than 1D for same step size
    expectTrue(rate > 0.1);
    expectTrue(rate < 0.8);
  };

  return TestRegistry::result();
}
