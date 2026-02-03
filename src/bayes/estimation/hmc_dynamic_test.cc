#include "bayes/estimation/hmc.h"

#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "bayes/estimation/metropolis_hastings.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  // ===========================================================================
  // 2D Dynamic HMC Tests
  // ===========================================================================

  "dynamic HMC on 2D uncorrelated normal"_test = [] {
    // Target: N([0,0], I) - independent standard normals
    auto log_target = [](std::span<const double> x) {
      return -(x[0] * x[0] + x[1] * x[1]) / 2.0;
    };
    auto grad_log = [](std::span<const double> x) {
      return std::vector<double>{-x[0], -x[1]};
    };

    auto kernel = makeHMCDynamic<double>(log_target, grad_log, 0.1, 20, 2);
    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::vector{0.0, 0.0}};

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

    // N(0,1): mean=0 for each dimension
    // SE for mean ~ 1/sqrt(N) ~ 0.014, so tolerance of 0.1 is ~7 SE
    expectNear(mean0, 0.0, 0.1);
    expectNear(mean1, 0.0, 0.1);

    // HMC should achieve high acceptance (typically 60-90% with good tuning)
    expectGT(acceptance_rate, 0.5);
  };

  "dynamic HMC on correlated 2D normal"_test = [] {
    // Correlated bivariate normal with covariance [[1, 0.8], [0.8, 1]]
    // Precision matrix = inverse of covariance
    // For rho=0.8: precision = 1/(1-rho^2) * [[1, -rho], [-rho, 1]]
    double rho = 0.8;
    double scale = 1.0 / (1.0 - rho * rho);  // ~2.78

    // log p(x) = -0.5 * x^T * precision * x (up to constant)
    auto log_target = [scale, rho](std::span<const double> x) {
      double prec_00 = scale;
      double prec_11 = scale;
      double prec_01 = -scale * rho;
      return -0.5 * (prec_00 * x[0] * x[0] + 2.0 * prec_01 * x[0] * x[1] +
                     prec_11 * x[1] * x[1]);
    };

    auto grad_log = [scale, rho](std::span<const double> x) {
      double prec_00 = scale;
      double prec_11 = scale;
      double prec_01 = -scale * rho;
      // grad = -precision * x
      return std::vector<double>{-(prec_00 * x[0] + prec_01 * x[1]),
                                  -(prec_01 * x[0] + prec_11 * x[1])};
    };

    auto kernel = makeHMCDynamic<double>(log_target, grad_log, 0.08, 25, 2);
    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::vector{0.0, 0.0}};

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
  // Energy Conservation Test
  // ===========================================================================

  "leapfrog energy conservation"_test = [] {
    // For a simple harmonic oscillator (quadratic potential), leapfrog should
    // nearly conserve energy. Use a 2D standard normal as the target.
    auto grad_u = [](const std::vector<double>& q) {
      // grad U = grad(-log p) = q for N(0,I)
      return q;
    };

    LeapfrogIntegratorDynamic<double, decltype(grad_u)> integrator{
        grad_u, 0.1, 20, {1.0, 1.0}};

    // Start at q = [1, 0] with p = [0, 1]
    std::vector<double> q0{1.0, 0.0};
    std::vector<double> p0{0.0, 1.0};

    // Initial energy: U = q^T q / 2 = 0.5, K = p^T p / 2 = 0.5, H = 1.0
    auto compute_H = [](const std::vector<double>& q, const std::vector<double>& p) {
      double U = (q[0] * q[0] + q[1] * q[1]) / 2.0;
      double K = (p[0] * p[0] + p[1] * p[1]) / 2.0;
      return U + K;
    };

    double H0 = compute_H(q0, p0);

    auto [qf, pf] = integrator.integrate(q0, p0);

    double Hf = compute_H(qf, pf);

    // Energy should be approximately conserved
    // For step size 0.1 and 20 steps, error should be small
    // Leapfrog has O(epsilon^2) global error
    expectNear(H0, Hf, 0.01);
  };

  // ===========================================================================
  // Acceptance Rate Verification
  // ===========================================================================

  "dynamic HMC acceptance rate reasonable"_test = [] {
    // Well-tuned HMC on a simple target should have acceptance rate 60-90%
    auto log_target = [](std::span<const double> x) {
      double sum = 0.0;
      for (double xi : x) sum += xi * xi;
      return -sum / 2.0;
    };
    auto grad_log = [](std::span<const double> x) {
      std::vector<double> g(x.size());
      for (std::size_t i = 0; i < x.size(); ++i) g[i] = -x[i];
      return g;
    };

    // 5D standard normal
    auto kernel = makeHMCDynamic<double>(log_target, grad_log, 0.15, 15, 5);
    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::vector<double>(5, 0.0)};

    chain.advance(500);
    auto [samples, rate] = chain.takeWithStats(3000);

    // Acceptance rate should be reasonably high for well-tuned HMC
    expectGT(rate, 0.5);
    expectLT(rate, 1.0);  // Not everything should be accepted (would indicate step size too small)
  };

  // ===========================================================================
  // Custom Mass Matrix Tests
  // ===========================================================================

  "dynamic HMC with diagonal mass matrix"_test = [] {
    // Anisotropic target: variances are 1 and 4 (sigma = 1, 2)
    // Optimal mass matrix is inverse of target covariance
    auto log_target = [](std::span<const double> x) {
      return -(x[0] * x[0] / 2.0 + x[1] * x[1] / 8.0);  // Var[x0]=1, Var[x1]=4
    };
    auto grad_log = [](std::span<const double> x) {
      return std::vector<double>{-x[0], -x[1] / 4.0};
    };

    // Mass matrix = [1, 0.25] rescales to make effective target isotropic
    std::vector<double> mass{1.0, 0.25};
    auto kernel = makeHMCDynamic<double>(log_target, grad_log, 0.12, 20, mass);

    expectNear(kernel.mass()[0], 1.0, 1e-10);
    expectNear(kernel.mass()[1], 0.25, 1e-10);

    std::mt19937_64 gen{42};
    Chain chain{kernel, gen, std::vector{0.0, 0.0}};

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

  // ===========================================================================
  // Accessor Tests
  // ===========================================================================

  "dynamic HMC accessors"_test = [] {
    auto log_target = [](std::span<const double> x) {
      double sum = 0.0;
      for (double xi : x) sum += xi * xi;
      return -sum / 2.0;
    };
    auto grad_log = [](std::span<const double> x) {
      std::vector<double> g(x.size());
      for (std::size_t i = 0; i < x.size(); ++i) g[i] = -x[i];
      return g;
    };

    std::vector<double> mass{1.0, 2.0, 3.0};
    auto kernel = makeHMCDynamic<double>(log_target, grad_log, 0.05, 30, mass);

    expectNear(kernel.epsilon(), 0.05, 1e-10);
    expectEq(kernel.nSteps(), std::size_t{30});
    expectEq(kernel.dim(), std::size_t{3});
    expectNear(kernel.mass()[0], 1.0, 1e-10);
    expectNear(kernel.mass()[1], 2.0, 1e-10);
    expectNear(kernel.mass()[2], 3.0, 1e-10);
  };

  "dynamic HMC dim accessor with unit mass"_test = [] {
    auto log_target = [](std::span<const double> x) {
      double sum = 0.0;
      for (double xi : x) sum += xi * xi;
      return -sum / 2.0;
    };
    auto grad_log = [](std::span<const double> x) {
      std::vector<double> g(x.size());
      for (std::size_t i = 0; i < x.size(); ++i) g[i] = -x[i];
      return g;
    };

    auto kernel = makeHMCDynamic<double>(log_target, grad_log, 0.1, 20, 7);

    expectEq(kernel.dim(), std::size_t{7});
    expectEq(kernel.mass().size(), std::size_t{7});
    for (std::size_t i = 0; i < 7; ++i) {
      expectNear(kernel.mass()[i], 1.0, 1e-10);
    }
  };

  return TestRegistry::result();
}
