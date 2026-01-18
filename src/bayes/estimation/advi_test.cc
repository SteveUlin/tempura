#include "bayes/estimation/advi.h"

#include <array>
#include <cmath>
#include <numbers>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  // ===========================================================================
  // MeanFieldGaussian Tests
  // ===========================================================================

  "MeanFieldGaussian entropy"_test = [] {
    // For a single N(0,1): entropy = 0.5 * log(2*pi*e) ~ 1.4189
    MeanFieldGaussian<double, 1> q;
    q.mu[0] = 0.0;
    q.omega[0] = 0.0;  // sigma = exp(0) = 1

    double expected_entropy = 0.5 * (1.0 + std::log(2 * std::numbers::pi));
    expectNear(q.entropy(), expected_entropy, 1e-10);
  };

  "MeanFieldGaussian entropy 2D"_test = [] {
    MeanFieldGaussian<double, 2> q;
    q.mu = {0.0, 0.0};
    q.omega = {0.0, std::log(2.0)};  // sigmas: 1, 2

    // H = omega[0] + omega[1] + 2 * 0.5 * log(2*pi*e)
    // = 0 + log(2) + log(2*pi*e)
    double single_const = 0.5 * (1.0 + std::log(2 * std::numbers::pi));
    double expected = 0.0 + std::log(2.0) + 2.0 * single_const;
    expectNear(q.entropy(), expected, 1e-10);
  };

  "MeanFieldGaussian sampling"_test = [] {
    MeanFieldGaussian<double, 2> q;
    q.mu = {5.0, -3.0};
    q.omega = {0.0, std::log(0.5)};  // sigmas: 1, 0.5

    std::mt19937_64 gen{42};

    // Sample many times and check empirical mean
    double sum0 = 0.0, sum1 = 0.0;
    constexpr std::size_t n = 10000;
    for (std::size_t i = 0; i < n; ++i) {
      auto s = q.sample(gen);
      sum0 += s[0];
      sum1 += s[1];
    }

    // SE ~ sigma/sqrt(N) ~ 1/100 for first, 0.5/100 for second
    // Tolerance of 0.1 is ~10 SE
    expectNear(sum0 / n, 5.0, 0.1);
    expectNear(sum1 / n, -3.0, 0.1);
  };

  // ===========================================================================
  // Adam Optimizer Tests
  // ===========================================================================

  "Adam basic update"_test = [] {
    AdamState<double, 1> adam;
    MeanFieldGaussian<double, 1> params;
    params.mu[0] = 0.0;
    params.omega[0] = 0.0;

    ADVIOptions opts;
    opts.learning_rate = 0.1;

    // Positive gradient should increase mu
    std::array<double, 1> grad_mu = {1.0};
    std::array<double, 1> grad_omega = {0.0};

    adam.update(params, grad_mu, grad_omega, opts);

    expectGT(params.mu[0], 0.0);  // mu should increase
  };

  // ===========================================================================
  // ADVI on Simple Targets
  // ===========================================================================

  "ADVI on 1D standard normal"_test = [] {
    // Target: N(0, 1)
    // log p(x) = -x^2/2 (up to constant)
    auto log_joint = [](const std::array<double, 1>& x) {
      return -x[0] * x[0] / 2.0;
    };
    auto grad_log_joint = [](const std::array<double, 1>& x) {
      return std::array<double, 1>{-x[0]};
    };

    ADVIOptions opts;
    opts.learning_rate = 0.1;
    opts.mc_samples = 10;

    auto advi = makeADVI<double, 1>(log_joint, grad_log_joint, opts);

    std::mt19937_64 gen{42};
    auto result = advi.fit(500, gen);

    // Optimal: mu=0, omega=0 (sigma=1)
    // ADVI should recover the exact posterior for Gaussian targets
    expectNear(result.mu[0], 0.0, 0.2);
    expectNear(std::exp(result.omega[0]), 1.0, 0.2);

    // ELBO should increase (or stay roughly constant at convergence)
    const auto& history = advi.elboHistory();
    expectGT(history.size(), std::size_t{0});
  };

  "ADVI on 1D shifted normal"_test = [] {
    // Target: N(5, 2)
    double mu_true = 5.0;
    double sigma_true = 2.0;

    auto log_joint = [mu_true, sigma_true](const std::array<double, 1>& x) {
      double z = (x[0] - mu_true) / sigma_true;
      return -z * z / 2.0;
    };
    auto grad_log_joint = [mu_true, sigma_true](const std::array<double, 1>& x) {
      return std::array<double, 1>{-(x[0] - mu_true) / (sigma_true * sigma_true)};
    };

    ADVIOptions opts;
    opts.learning_rate = 0.15;
    opts.mc_samples = 5;

    auto advi = makeADVI<double, 1>(log_joint, grad_log_joint, opts);

    // Initialize closer to the solution for faster convergence
    MeanFieldGaussian<double, 1> init;
    init.mu[0] = 3.0;
    init.omega[0] = 0.5;
    advi.initialize(init);

    std::mt19937_64 gen{123};
    auto result = advi.fit(1000, gen);

    // Should recover mu=5, sigma=2
    expectNear(result.mu[0], mu_true, 0.5);
    expectNear(std::exp(result.omega[0]), sigma_true, 0.5);
  };

  "ADVI on 2D independent normals"_test = [] {
    // Target: N([3, -2], diag([1, 4]))
    std::array<double, 2> mu_true = {3.0, -2.0};
    std::array<double, 2> sigma_true = {1.0, 2.0};

    auto log_joint = [mu_true, sigma_true](const std::array<double, 2>& x) {
      double lp = 0.0;
      for (std::size_t i = 0; i < 2; ++i) {
        double z = (x[i] - mu_true[i]) / sigma_true[i];
        lp -= z * z / 2.0;
      }
      return lp;
    };
    auto grad_log_joint = [mu_true, sigma_true](const std::array<double, 2>& x) {
      std::array<double, 2> grad;
      for (std::size_t i = 0; i < 2; ++i) {
        grad[i] = -(x[i] - mu_true[i]) / (sigma_true[i] * sigma_true[i]);
      }
      return grad;
    };

    ADVIOptions opts;
    opts.learning_rate = 0.15;
    opts.mc_samples = 20;  // More samples for lower variance

    auto advi = makeADVI<double, 2>(log_joint, grad_log_joint, opts);

    // Initialize closer to true values for faster convergence
    MeanFieldGaussian<double, 2> init;
    init.mu = {1.0, 0.0};  // Closer to {3, -2} than default {0, 0}
    init.omega = {0.0, 0.5};
    advi.initialize(init);

    std::mt19937_64 gen{42};
    auto result = advi.fit(2000, gen);

    // Check means - increased tolerance due to stochastic optimization
    expectNear(result.mu[0], mu_true[0], 0.5);
    expectNear(result.mu[1], mu_true[1], 0.5);

    // Check standard deviations
    expectNear(std::exp(result.omega[0]), sigma_true[0], 0.4);
    expectNear(std::exp(result.omega[1]), sigma_true[1], 0.6);
  };

  "ADVI sampling from fitted approximation"_test = [] {
    // Fit to N(2, 0.5)
    double mu_true = 2.0;
    double sigma_true = 0.5;

    auto log_joint = [mu_true, sigma_true](const std::array<double, 1>& x) {
      double z = (x[0] - mu_true) / sigma_true;
      return -z * z / 2.0;
    };
    auto grad_log_joint = [mu_true, sigma_true](const std::array<double, 1>& x) {
      return std::array<double, 1>{-(x[0] - mu_true) / (sigma_true * sigma_true)};
    };

    ADVIOptions opts;
    opts.learning_rate = 0.1;
    opts.mc_samples = 5;

    auto advi = makeADVI<double, 1>(log_joint, grad_log_joint, opts);

    std::mt19937_64 gen1{42};
    advi.fit(500, gen1);

    // Sample from fitted approximation
    std::mt19937_64 gen2{123};
    auto samples = advi.sample(gen2, 5000);

    // Compute sample mean
    double sum = 0.0;
    for (const auto& s : samples) {
      sum += s[0];
    }
    double mean = sum / static_cast<double>(samples.size());

    // Should be near mu_true
    expectNear(mean, mu_true, 0.2);
  };

  "ADVI ELBO increases"_test = [] {
    // Simple target to verify ELBO optimization
    auto log_joint = [](const std::array<double, 1>& x) {
      return -x[0] * x[0] / 2.0;
    };
    auto grad_log_joint = [](const std::array<double, 1>& x) {
      return std::array<double, 1>{-x[0]};
    };

    ADVIOptions opts;
    opts.learning_rate = 0.05;  // Smaller LR for smoother convergence
    opts.mc_samples = 20;       // More samples for lower variance
    opts.elbo_tol = 1e-8;       // Don't stop early

    auto advi = makeADVI<double, 1>(log_joint, grad_log_joint, opts);

    // Start from a bad initial point
    MeanFieldGaussian<double, 1> init;
    init.mu[0] = 5.0;  // Far from optimal mu=0
    init.omega[0] = 1.0;
    advi.initialize(init);

    std::mt19937_64 gen{42};
    advi.fit(200, gen);

    const auto& history = advi.elboHistory();
    expectGT(history.size(), std::size_t{10});

    // Check that ELBO generally increases (smoothed)
    // Compare first 10 vs last 10 average
    double first_avg = 0.0;
    double last_avg = 0.0;
    std::size_t window = std::min(std::size_t{10}, history.size() / 2);

    for (std::size_t i = 0; i < window; ++i) {
      first_avg += history[i];
      last_avg += history[history.size() - 1 - i];
    }
    first_avg /= static_cast<double>(window);
    last_avg /= static_cast<double>(window);

    expectGT(last_avg, first_avg);  // ELBO should increase
  };

  // ===========================================================================
  // Finite Difference Gradient Tests
  // ===========================================================================

  "ADVI with finite difference gradient"_test = [] {
    // Target: N(3, 1)
    double mu_true = 3.0;

    auto log_joint = [mu_true](const std::array<double, 1>& x) {
      return -(x[0] - mu_true) * (x[0] - mu_true) / 2.0;
    };

    ADVIOptions opts;
    opts.learning_rate = 0.1;
    opts.mc_samples = 10;

    auto advi = makeADVIWithFiniteDiff<double, 1>(log_joint, opts);

    std::mt19937_64 gen{42};
    auto result = advi.fit(500, gen);

    // Should recover approximately correct mu
    // Finite diff is less accurate, so use larger tolerance
    expectNear(result.mu[0], mu_true, 0.5);
  };

  return TestRegistry::result();
}
