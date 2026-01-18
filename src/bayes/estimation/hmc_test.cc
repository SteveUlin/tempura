#include "bayes/estimation/hmc.h"

#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <random>

#include "bayes/estimation/metropolis_hastings.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  // ===========================================================================
  // 1D Scalar HMC Tests
  // ===========================================================================

  "scalar HMC on N(0,1)"_test = [] {
    // Target: N(0, 1) - log p(x) = -x^2/2 (up to constant)
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto grad_log = [](double x) { return -x; };  // d/dx[-x^2/2] = -x

    auto kernel = makeHMCScalar<double>(log_target, grad_log, 0.1, 20);
    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, 0.0};

    // Burn-in
    chain.advance(1000);

    // Collect samples
    auto [samples, acceptance_rate] = chain.takeWithStats(10000, 5);

    // Compute sample mean and variance
    double sum = 0.0;
    for (double s : samples) sum += s;
    double mean = sum / static_cast<double>(samples.size());

    double sum_sq = 0.0;
    for (double s : samples) sum_sq += (s - mean) * (s - mean);
    double variance = sum_sq / static_cast<double>(samples.size() - 1);

    // N(0,1): mean=0, variance=1
    // SE for mean ~ 1/sqrt(N) ~ 0.01, so tolerance of 0.1 is ~10 SE
    expectNear(mean, 0.0, 0.1);

    // Variance should be near 1. For large N, sample variance SE ~ sqrt(2/(N-1))
    expectNear(variance, 1.0, 0.15);

    // HMC should achieve high acceptance (typically 60-90% with good tuning)
    expectGT(acceptance_rate, 0.5);
  };

  "scalar HMC on N(5, 2)"_test = [] {
    // Target: N(5, 2)
    double mu = 5.0;
    double sigma = 2.0;
    auto log_target = [mu, sigma](double x) {
      double z = (x - mu) / sigma;
      return -z * z / 2.0;
    };
    auto grad_log = [mu, sigma](double x) {
      return -(x - mu) / (sigma * sigma);
    };

    auto kernel = makeHMCScalar<double>(log_target, grad_log, 0.15, 15);
    std::mt19937_64 gen{123};
    Chain chain{kernel, gen, mu};  // Start at mode

    chain.advance(500);
    auto samples = chain.take(5000, 3);

    double sum = 0.0;
    for (double s : samples) sum += s;
    double mean = sum / static_cast<double>(samples.size());

    expectNear(mean, mu, 0.2);
  };

  // ===========================================================================
  // N-Dimensional HMC Tests
  // ===========================================================================

  "2D HMC on uncorrelated normal"_test = [] {
    // Target: N([0,0], I) - independent standard normals
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1]};
    };

    auto kernel = makeHMC<double, 2>(log_target, grad_log, 0.1, 20);
    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::array{0.0, 0.0}};

    chain.advance(1000);
    auto [samples, acceptance_rate] = chain.takeWithStats(5000, 3);

    // Compute means
    double mean0 = 0.0, mean1 = 0.0;
    for (const auto& s : samples) {
      mean0 += s[0];
      mean1 += s[1];
    }
    mean0 /= static_cast<double>(samples.size());
    mean1 /= static_cast<double>(samples.size());

    expectNear(mean0, 0.0, 0.1);
    expectNear(mean1, 0.0, 0.1);

    expectGT(acceptance_rate, 0.5);
  };

  "2D HMC on correlated normal"_test = [] {
    // Correlated bivariate normal with covariance [[1, 0.8], [0.8, 1]]
    // Precision matrix = inverse of covariance
    // For rho=0.8: precision = 1/(1-rho^2) * [[1, -rho], [-rho, 1]]
    double rho = 0.8;
    double scale = 1.0 / (1.0 - rho * rho);  // ~2.78

    // log p(x) = -0.5 * x^T * precision * x (up to constant)
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
      // grad = -precision * x
      return std::array<double, 2>{-(prec_00 * x[0] + prec_01 * x[1]),
                                   -(prec_01 * x[0] + prec_11 * x[1])};
    };

    auto kernel = makeHMC<double, 2>(log_target, grad_log, 0.08, 25);
    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::array{0.0, 0.0}};

    chain.advance(1000);
    auto samples = chain.take(8000, 3);

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
    // SE for correlation ~ (1-rho^2)/sqrt(N) ~ 0.36/sqrt(8000) ~ 0.004
    // Tolerance of 0.15 is conservative due to MCMC autocorrelation
    expectNear(empirical_corr, rho, 0.15);
  };

  // ===========================================================================
  // HMC vs MH Acceptance Rate Comparison
  // ===========================================================================

  "HMC has higher acceptance than MH on 2D target"_test = [] {
    // Target: correlated 2D normal (challenging for random-walk MH)
    double rho = 0.9;  // High correlation makes random walk struggle
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

    // HMC with reasonable tuning
    auto hmc_kernel = makeHMC<double, 2>(log_target, grad_log, 0.1, 20);
    std::mt19937_64 hmc_gen{42};
    Chain hmc_chain{hmc_kernel, hmc_gen, std::array{0.0, 0.0}};

    hmc_chain.advance(500);
    auto [hmc_samples, hmc_rate] = hmc_chain.takeWithStats(3000);

    // MH with Gaussian random walk - step size that gives ~23% optimal for 2D
    GaussianWalkND<double, 2> mh_proposal{0.5};
    auto mh_kernel = MetropolisHastings<std::array<double, 2>,
                                         decltype(log_target),
                                         decltype(mh_proposal)>{
        log_target, mh_proposal};
    std::mt19937_64 mh_gen{42};
    Chain mh_chain{mh_kernel, mh_gen, std::array{0.0, 0.0}};

    mh_chain.advance(500);
    auto [mh_samples, mh_rate] = mh_chain.takeWithStats(3000);

    // HMC should have higher acceptance rate than MH on this target
    // Well-tuned HMC typically achieves 60-90% acceptance
    // Random-walk MH on correlated targets struggles (20-40% typical)
    expectGT(hmc_rate, mh_rate);
    expectGT(hmc_rate, 0.5);  // HMC should be at least 50%
  };

  // ===========================================================================
  // Custom Mass Matrix Tests
  // ===========================================================================

  "scalar HMC with custom mass"_test = [] {
    // Standard normal target
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto grad_log = [](double x) { return -x; };

    // Use mass = 2.0 (heavier particle, slower dynamics)
    auto kernel = makeHMCScalar<double>(log_target, grad_log, 0.15, 15, 2.0);

    expectNear(kernel.mass(), 2.0, 1e-10);

    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, 0.0};

    chain.advance(500);
    auto samples = chain.take(3000, 3);

    double sum = 0.0;
    for (double s : samples) sum += s;
    double mean = sum / static_cast<double>(samples.size());

    expectNear(mean, 0.0, 0.15);
  };

  "2D HMC with diagonal mass matrix"_test = [] {
    // Anisotropic target: variances are 1 and 4 (sigma = 1, 2)
    // Optimal mass matrix is inverse of target covariance
    auto log_target = [](const std::array<double, 2>& x) {
      return -(x[0] * x[0] / 2.0 + x[1] * x[1] / 8.0);  // Var[x0]=1, Var[x1]=4
    };
    auto grad_log = [](const std::array<double, 2>& x) {
      return std::array<double, 2>{-x[0], -x[1] / 4.0};
    };

    // Mass matrix = [1, 0.25] rescales to make effective target isotropic
    std::array<double, 2> mass{1.0, 0.25};
    auto kernel = makeHMC<double, 2>(log_target, grad_log, 0.12, 20, mass);

    expectNear(kernel.mass()[0], 1.0, 1e-10);
    expectNear(kernel.mass()[1], 0.25, 1e-10);

    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::array{0.0, 0.0}};

    chain.advance(1000);
    auto samples = chain.take(5000, 3);

    // Check variance in each dimension
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
    expectNear(var0, 1.0, 0.2);
    expectNear(var1, 4.0, 0.5);
  };

  "HMC accessors"_test = [] {
    auto log_target = [](double x) { return -x * x / 2.0; };
    auto grad_log = [](double x) { return -x; };

    auto kernel = makeHMCScalar<double>(log_target, grad_log, 0.1, 25, 1.5);

    expectNear(kernel.epsilon(), 0.1, 1e-10);
    expectEq(kernel.nSteps(), std::size_t{25});
    expectNear(kernel.mass(), 1.5, 1e-10);
  };

  "N-dim HMC accessors"_test = [] {
    auto log_target = [](const std::array<double, 3>& x) {
      return -(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]) / 2.0;
    };
    auto grad_log = [](const std::array<double, 3>& x) {
      return std::array<double, 3>{-x[0], -x[1], -x[2]};
    };

    std::array<double, 3> mass{1.0, 2.0, 3.0};
    auto kernel = makeHMC<double, 3>(log_target, grad_log, 0.05, 30, mass);

    expectNear(kernel.epsilon(), 0.05, 1e-10);
    expectEq(kernel.nSteps(), std::size_t{30});
    expectNear(kernel.mass()[0], 1.0, 1e-10);
    expectNear(kernel.mass()[1], 2.0, 1e-10);
    expectNear(kernel.mass()[2], 3.0, 1e-10);
  };

  return TestRegistry::result();
}
