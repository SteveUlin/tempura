#include "bayes/estimation/dual_averaging.h"

#include <array>
#include <cmath>
#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  // ===========================================================================
  // DualAveragingState Tests
  // ===========================================================================

  "DualAveragingState initialization"_test = [] {
    auto state = DualAveragingState<double>::init(0.1);

    expectNear(state.epsilon(), 0.1, 1e-10);
    expectNear(state.log_epsilon, std::log(0.1), 1e-10);
    expectNear(state.log_epsilon_bar, 0.0, 1e-10);
    expectNear(state.h_bar, 0.0, 1e-10);
    expectEq(state.m, std::size_t{0});
  };

  "DualAveragingState converges with consistent acceptance"_test = [] {
    // If we consistently report the target acceptance rate,
    // h_bar should stay near zero and epsilon should stabilize
    auto state = DualAveragingState<double>::init(0.1);
    DualAveragingOptions opts;

    // Simulate 100 iterations with exactly target acceptance
    for (int i = 0; i < 100; ++i) {
      state.update(opts.target_accept, opts);
    }

    // h_bar should be near zero when acceptance matches target
    expectNear(state.h_bar, 0.0, 0.05);

    // Final epsilon should be near initial after many consistent updates
    // The algorithm should have converged to a stable value
    expectTrue(state.finalEpsilon() > 0.0);
  };

  "DualAveragingState decreases step size when acceptance too low"_test = [] {
    auto state = DualAveragingState<double>::init(1.0);
    DualAveragingOptions opts;
    opts.target_accept = 0.8;

    double initial_epsilon = state.epsilon();

    // Simulate low acceptance (step size too large)
    for (int i = 0; i < 50; ++i) {
      state.update(0.3, opts);  // Way below target
    }

    // Step size should have decreased
    expectLT(state.epsilon(), initial_epsilon);
    // h_bar should be positive (want more acceptance)
    expectGT(state.h_bar, 0.0);
  };

  "DualAveragingState increases step size when acceptance too high"_test = [] {
    auto state = DualAveragingState<double>::init(0.01);
    DualAveragingOptions opts;
    opts.target_accept = 0.8;

    double initial_epsilon = state.epsilon();

    // Simulate high acceptance (step size too small, inefficient)
    for (int i = 0; i < 50; ++i) {
      state.update(0.99, opts);  // Way above target
    }

    // Step size should have increased
    expectGT(state.epsilon(), initial_epsilon);
    // h_bar should be negative (want less acceptance = larger steps)
    expectLT(state.h_bar, 0.0);
  };

  // ===========================================================================
  // AdaptiveHMC Tests
  // ===========================================================================

  "AdaptiveHMC adapts step size on 2D standard normal"_test = [] {
    // Target: N([0,0], I)
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1]};
    };

    // Start with intentionally bad step size
    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 0.5, 20);

    std::mt19937_64 gen{42};
    std::array<double, 2> state{0.0, 0.0};

    // Warmup: adapt step size
    adaptive_hmc.warmup(state, gen, 1000);

    // Step size should have been adjusted
    double adapted_epsilon = adaptive_hmc.epsilon();
    expectGT(adapted_epsilon, 0.0);

    // Sample with adapted step size
    auto samples = adaptive_hmc.sample(state, gen, 5000);

    // Acceptance rate should be reasonably high (HMC typically achieves > 0.6)
    // The dual averaging may not perfectly hit target, but should be in reasonable range
    double accept_rate = adaptive_hmc.acceptanceRate();
    expectGT(accept_rate, 0.5);
    expectLT(accept_rate, 1.0);

    // Samples should have correct mean
    // With more samples and longer warmup, MCMC should mix well
    double mean0 = 0.0, mean1 = 0.0;
    for (const auto& s : samples) {
      mean0 += s[0];
      mean1 += s[1];
    }
    mean0 /= static_cast<double>(samples.size());
    mean1 /= static_cast<double>(samples.size());

    // Tolerance for mean: SE ~ 1/sqrt(N/ESS), with ESS ~ N/tau where tau is autocorrelation time
    // For well-tuned HMC, tau is small, so tolerance of 0.2 should be safe
    expectNear(mean0, 0.0, 0.2);
    expectNear(mean1, 0.0, 0.2);
  };

  "AdaptiveHMC on 2D correlated Gaussian"_test = [] {
    // Correlated bivariate normal with covariance [[1, 0.8], [0.8, 1]]
    double rho = 0.8;
    double scale = 1.0 / (1.0 - rho * rho);

    auto log_target = [scale, rho](const std::array<double, 2>& x) {
      double prec_00 = scale;
      double prec_11 = scale;
      double prec_01 = -scale * rho;
      return -0.5 * (prec_00 * x[0] * x[0] + 2.0 * prec_01 * x[0] * x[1] +
                     prec_11 * x[1] * x[1]);
    };

    auto grad_log = [scale, rho](const std::array<double, 2>& x) {
      double prec_00 = scale;
      double prec_11 = scale;
      double prec_01 = -scale * rho;
      return std::array<double, 2>{-(prec_00 * x[0] + prec_01 * x[1]),
                                   -(prec_01 * x[0] + prec_11 * x[1])};
    };

    // Start with a reasonable initial step size
    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 0.1, 25);

    std::mt19937_64 gen{123};
    std::array<double, 2> state{0.0, 0.0};

    // Warmup
    adaptive_hmc.warmup(state, gen, 1000);

    // Sample
    auto samples = adaptive_hmc.sample(state, gen, 3000);

    // Check acceptance rate is reasonable
    double accept_rate = adaptive_hmc.acceptanceRate();
    expectGT(accept_rate, 0.5);

    // Compute sample correlation
    double mean0 = 0.0, mean1 = 0.0;
    for (const auto& s : samples) {
      mean0 += s[0];
      mean1 += s[1];
    }
    mean0 /= static_cast<double>(samples.size());
    mean1 /= static_cast<double>(samples.size());

    double cov = 0.0, var0 = 0.0, var1 = 0.0;
    for (const auto& s : samples) {
      double d0 = s[0] - mean0;
      double d1 = s[1] - mean1;
      cov += d0 * d1;
      var0 += d0 * d0;
      var1 += d1 * d1;
    }
    double empirical_corr = cov / std::sqrt(var0 * var1);

    // Correlation should be near 0.8
    // SE ~ (1-rho^2)/sqrt(N) ~ 0.007 for N=3000
    // Tolerance 0.15 accounts for MCMC autocorrelation
    expectNear(empirical_corr, rho, 0.15);
  };

  "AdaptiveHMC handles step size too large"_test = [] {
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1]};
    };

    // Start with step size way too large (will cause rejections)
    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 5.0, 20);

    std::mt19937_64 gen{42};
    std::array<double, 2> state{0.0, 0.0};

    double initial_epsilon = adaptive_hmc.epsilon();

    // Warmup should adapt step size down
    adaptive_hmc.warmup(state, gen, 500);

    // Step size should have decreased significantly
    expectLT(adaptive_hmc.epsilon(), initial_epsilon);

    // Sample and check acceptance rate is reasonable
    auto samples = adaptive_hmc.sample(state, gen, 1000);
    expectGT(adaptive_hmc.acceptanceRate(), 0.5);
  };

  "AdaptiveHMC handles step size too small"_test = [] {
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1]};
    };

    // Start with step size too small (inefficient)
    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 0.001, 20);

    std::mt19937_64 gen{42};
    std::array<double, 2> state{0.0, 0.0};

    double initial_epsilon = adaptive_hmc.epsilon();

    // Warmup should adapt step size up
    adaptive_hmc.warmup(state, gen, 500);

    // Step size should have increased
    expectGT(adaptive_hmc.epsilon(), initial_epsilon);
  };

  "AdaptiveHMC with custom options"_test = [] {
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1]};
    };

    // Custom options: lower target acceptance
    DualAveragingOptions opts;
    opts.target_accept = 0.65;

    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 0.1, 20, opts);

    std::mt19937_64 gen{42};
    std::array<double, 2> state{0.0, 0.0};

    adaptive_hmc.warmup(state, gen, 1000);
    auto samples = adaptive_hmc.sample(state, gen, 3000);

    // Acceptance rate should be in a reasonable range
    // Dual averaging converges to target over time, but may not be exact
    double accept_rate = adaptive_hmc.acceptanceRate();
    expectGT(accept_rate, 0.4);
    expectLT(accept_rate, 1.0);
  };

  "AdaptiveHMC accessors"_test = [] {
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1]};
    };

    DualAveragingOptions opts;
    opts.target_accept = 0.75;
    opts.gamma = 0.1;

    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 0.15, 25, opts);

    expectNear(adaptive_hmc.epsilon(), 0.15, 1e-10);
    expectNear(adaptive_hmc.options().target_accept, 0.75, 1e-10);
    expectNear(adaptive_hmc.options().gamma, 0.1, 1e-10);
    expectEq(adaptive_hmc.daState().m, std::size_t{0});
  };

  "AdaptiveHMC with custom mass matrix"_test = [] {
    // Anisotropic target: variances are 1 and 4
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] / 2.0 + x[1] * x[1] / 8.0);
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1] / 4.0};
    };

    // Mass matrix tuned to target covariance
    std::array<double, 2> mass{1.0, 0.25};
    auto adaptive_hmc =
        makeAdaptiveHMC<double, 2>(log_target, grad_log, 0.1, 20, mass);

    std::mt19937_64 gen{42};
    std::array<double, 2> state{0.0, 0.0};

    adaptive_hmc.warmup(state, gen, 500);
    auto samples = adaptive_hmc.sample(state, gen, 3000);

    // Check variances
    double mean0 = 0.0, mean1 = 0.0;
    for (const auto& s : samples) {
      mean0 += s[0];
      mean1 += s[1];
    }
    mean0 /= static_cast<double>(samples.size());
    mean1 /= static_cast<double>(samples.size());

    double var0 = 0.0, var1 = 0.0;
    for (const auto& s : samples) {
      var0 += (s[0] - mean0) * (s[0] - mean0);
      var1 += (s[1] - mean1) * (s[1] - mean1);
    }
    var0 /= static_cast<double>(samples.size() - 1);
    var1 /= static_cast<double>(samples.size() - 1);

    // Expected: Var[x0]=1, Var[x1]=4
    expectNear(var0, 1.0, 0.25);
    expectNear(var1, 4.0, 0.6);
  };

  return TestRegistry::result();
}
