// Example: Tuple-based Metropolis-Hastings
//
// Tuples allow heterogeneous state spaces - mix continuous and discrete,
// or use different proposal distributions per parameter.
// Uses TupleWalk and HomogeneousWalk from the header.

#include "bayes/estimation/metropolis_hastings.h"

#include <cmath>
#include <random>
#include <tuple>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  "homogeneous tuple - 3D normal"_test = [] {
    using State = std::tuple<double, double, double>;

    // log p(x,y,z) = -(x² + y² + z²)/2
    auto log_target = [](const State& s) {
      auto [x, y, z] = s;
      return -(x * x + y * y + z * z) / 2.0;
    };

    auto kernel = makeMetropolisHastings<State>(
        log_target, HomogeneousWalk{GaussianWalk{0.5}});

    Chain chain{kernel, std::mt19937_64{42}, State{0.0, 0.0, 0.0}};

    chain.advance(1000);
    auto samples = chain.take(10'000);

    // Check each dimension has mean ≈ 0, var ≈ 1
    double sum_x = 0;
    double sum_y = 0;
    double sum_z = 0;
    for (const auto& [x, y, z] : samples) {
      sum_x += x;
      sum_y += y;
      sum_z += z;
    }
    double n = static_cast<double>(samples.size());

    expectNear(0.0, sum_x / n, 0.1);
    expectNear(0.0, sum_y / n, 0.1);
    expectNear(0.0, sum_z / n, 0.1);
  };

  "heterogeneous proposals"_test = [] {
    using State = std::tuple<double, double>;

    // Different variances: x ~ N(0, 1), y ~ N(0, 4)
    auto log_target = [](const State& s) {
      auto [x, y] = s;
      return -x * x / 2.0 - y * y / 8.0;
    };

    // Use different step sizes for each dimension
    auto kernel = makeMetropolisHastings<State>(
        log_target, TupleWalk{GaussianWalk{0.3}, GaussianWalk{0.8}});

    Chain chain{kernel, std::mt19937_64{123}, State{0.0, 0.0}};

    chain.advance(2000);
    auto samples = chain.take(20'000);

    double sum_x = 0;
    double sum_y = 0;
    double sum_x2 = 0;
    double sum_y2 = 0;
    for (const auto& [x, y] : samples) {
      sum_x += x;
      sum_y += y;
      sum_x2 += x * x;
      sum_y2 += y * y;
    }
    double n = static_cast<double>(samples.size());
    double mean_x = sum_x / n;
    double mean_y = sum_y / n;
    double var_x = sum_x2 / n - mean_x * mean_x;
    double var_y = sum_y2 / n - mean_y * mean_y;

    expectNear(0.0, mean_x, 0.1);
    expectNear(0.0, mean_y, 0.15);
    expectNear(1.0, var_x, 0.15);
    expectNear(4.0, var_y, 0.4);
  };

  "mixed proposal types"_test = [] {
    using State = std::tuple<double, double>;

    auto log_target = [](const State& s) {
      auto [x, y] = s;
      return -x * x / 2.0 - y * y / 2.0;
    };

    // Mix Gaussian and Uniform proposals
    auto kernel = makeMetropolisHastings<State>(
        log_target, TupleWalk{GaussianWalk{0.5}, UniformWalk{0.8}});

    Chain chain{kernel, std::mt19937_64{42}, State{0.0, 0.0}};

    chain.advance(1000);
    auto [samples, rate] = chain.takeWithStats(5000);

    expectTrue(rate > 0.2);
    expectTrue(rate < 0.8);
    expectEq(samples.size(), std::size_t{5000});
  };

  "correlated parameters via tuple"_test = [] {
    using State = std::tuple<double, double>;

    // Bivariate normal with ρ = 0.7
    auto log_target = [](const State& s) {
      auto [x, y] = s;
      double rho = 0.7;
      double inv_det = 1.0 / (1.0 - rho * rho);
      return -0.5 * inv_det * (x * x - 2.0 * rho * x * y + y * y);
    };

    auto kernel = makeMetropolisHastings<State>(
        log_target, HomogeneousWalk{GaussianWalk{0.4}});

    Chain chain{kernel, std::mt19937_64{777}, State{0.0, 0.0}};

    chain.advance(2000);
    auto samples = chain.take(20'000);

    // Compute correlation
    double sum_x = 0;
    double sum_y = 0;
    double sum_xy = 0;
    double sum_x2 = 0;
    double sum_y2 = 0;
    for (const auto& [x, y] : samples) {
      sum_x += x;
      sum_y += y;
      sum_xy += x * y;
      sum_x2 += x * x;
      sum_y2 += y * y;
    }
    double n = static_cast<double>(samples.size());
    double mean_x = sum_x / n;
    double mean_y = sum_y / n;
    double cov_xy = sum_xy / n - mean_x * mean_y;
    double var_x = sum_x2 / n - mean_x * mean_x;
    double var_y = sum_y2 / n - mean_y * mean_y;
    double corr = cov_xy / std::sqrt(var_x * var_y);

    expectNear(0.7, corr, 0.05);
  };

  "named parameters via structured bindings"_test = [] {
    // Bayesian linear regression: y = α + βx + ε
    // Posterior for (α, β, σ) given data
    using State = std::tuple<double, double, double>;  // (intercept, slope, noise_sd)

    // Fake posterior (pretend we computed this from data)
    // α ~ N(2, 0.1²), β ~ N(0.5, 0.05²), log(σ) ~ N(0, 0.2²)
    auto log_target = [](const State& s) {
      auto [alpha, beta, log_sigma] = s;
      double lp = 0.0;
      lp += -(alpha - 2.0) * (alpha - 2.0) / (2.0 * 0.01);      // α prior
      lp += -(beta - 0.5) * (beta - 0.5) / (2.0 * 0.0025);      // β prior
      lp += -log_sigma * log_sigma / (2.0 * 0.04);              // log(σ) prior
      return lp;
    };

    auto kernel = makeMetropolisHastings<State>(
        log_target,
        TupleWalk{GaussianWalk{0.05}, GaussianWalk{0.02}, GaussianWalk{0.1}});

    Chain chain{kernel, std::mt19937_64{42}, State{2.0, 0.5, 0.0}};

    chain.advance(1000);
    auto samples = chain.take(5000);

    // Extract posterior means
    double sum_alpha = 0;
    double sum_beta = 0;
    double sum_log_sigma = 0;
    for (const auto& [alpha, beta, log_sigma] : samples) {
      sum_alpha += alpha;
      sum_beta += beta;
      sum_log_sigma += log_sigma;
    }
    double n = static_cast<double>(samples.size());

    expectNear(2.0, sum_alpha / n, 0.05);
    expectNear(0.5, sum_beta / n, 0.03);
    expectNear(0.0, sum_log_sigma / n, 0.1);
  };

  return TestRegistry::result();
}
