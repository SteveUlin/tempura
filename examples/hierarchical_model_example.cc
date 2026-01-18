// Eight Schools Hierarchical Model - the classic Bayesian hierarchical example.
//
// The eight schools problem (Rubin, 1981): 8 schools each have an estimated
// treatment effect y_j with standard error sigma_j. We want to estimate the
// true effects theta_j while sharing information across schools.
//
// Model:
//   mu    ~ Normal(0, 5)           # population mean effect
//   tau   ~ HalfNormal(5)          # between-school std dev
//   theta_j ~ Normal(mu, tau)      # school-specific effects (j=1..8)
//   y_j   ~ Normal(theta_j, sigma_j)  # observed effects (sigma_j known)
//
// We use a non-centered parameterization for better sampling:
//   eta_j ~ Normal(0, 1)
//   theta_j = mu + tau * eta_j
//
// State: [mu, log(tau), eta_1, ..., eta_8] (10 parameters)

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <print>
#include <random>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "bayes/estimation/metropolis_hastings.h"
#include "plot.h"

using namespace tempura;

// =============================================================================
// Eight Schools Data (Rubin, 1981)
// =============================================================================

// Observed treatment effects (y_j) and standard errors (sigma_j) for 8 schools
constexpr std::array<double, 8> kY = {28, 8, -3, 7, -1, 1, 18, 12};
constexpr std::array<double, 8> kSigma = {15, 10, 16, 11, 9, 11, 10, 18};

// Number of schools
constexpr size_t kNumSchools = 8;

// State: [mu, log_tau, eta_1, ..., eta_8]
constexpr size_t kStateDim = 2 + kNumSchools;  // 10 parameters
using State = std::array<double, kStateDim>;

// =============================================================================
// Model Functions
// =============================================================================

// Log-prior for mu ~ Normal(0, 5)
constexpr auto logPriorMu(double mu) -> double {
  constexpr double sigma_mu = 5.0;
  return -0.5 * (mu * mu) / (sigma_mu * sigma_mu);
}

// Log-prior for tau ~ HalfNormal(5), parameterized via log_tau
// p(tau) propto exp(-tau^2 / (2 * 5^2)) for tau > 0
// With change of variables log_tau, Jacobian is tau = exp(log_tau)
constexpr auto logPriorLogTau(double log_tau) -> double {
  constexpr double sigma_tau = 5.0;
  double tau = std::exp(log_tau);
  // log p(tau) + log|d(tau)/d(log_tau)| = -tau^2/(2*sigma^2) + log_tau
  return -0.5 * (tau * tau) / (sigma_tau * sigma_tau) + log_tau;
}

// Log-prior for eta_j ~ Normal(0, 1)
constexpr auto logPriorEta(double eta) -> double {
  return -0.5 * eta * eta;
}

// Log-likelihood for school j: y_j ~ Normal(theta_j, sigma_j)
// where theta_j = mu + tau * eta_j
constexpr auto logLikelihoodSchool(double y, double sigma, double theta) -> double {
  double z = (y - theta) / sigma;
  return -0.5 * z * z - std::log(sigma);
}

// Full log-posterior (unnormalized)
auto logPosterior(const State& state) -> double {
  double mu = state[0];
  double log_tau = state[1];
  double tau = std::exp(log_tau);

  // Handle edge case: tau too small causes numerical issues
  if (tau < 1e-8) {
    return -std::numeric_limits<double>::infinity();
  }

  double log_p = 0.0;

  // Priors
  log_p += logPriorMu(mu);
  log_p += logPriorLogTau(log_tau);

  // Prior on eta_j and likelihood for each school
  for (size_t j = 0; j < kNumSchools; ++j) {
    double eta_j = state[2 + j];
    double theta_j = mu + tau * eta_j;

    log_p += logPriorEta(eta_j);
    log_p += logLikelihoodSchool(kY[j], kSigma[j], theta_j);
  }

  return log_p;
}

// Gradient of log-posterior
auto gradLogPosterior(const State& state) -> State {
  double mu = state[0];
  double log_tau = state[1];
  double tau = std::exp(log_tau);

  State grad{};

  if (tau < 1e-8) {
    return grad;  // Return zeros
  }

  constexpr double sigma_mu = 5.0;
  constexpr double sigma_tau = 5.0;

  // Gradient of log prior for mu
  grad[0] = -mu / (sigma_mu * sigma_mu);

  // Gradient of log prior for log_tau (including Jacobian)
  // d/d(log_tau) [-tau^2/(2*sigma^2) + log_tau]
  // = -tau * d(tau)/d(log_tau) / sigma^2 + 1
  // = -tau^2 / sigma^2 + 1
  grad[1] = 1.0 - (tau * tau) / (sigma_tau * sigma_tau);

  // Gradients from likelihood and eta priors
  for (size_t j = 0; j < kNumSchools; ++j) {
    double eta_j = state[2 + j];
    double theta_j = mu + tau * eta_j;
    double residual = kY[j] - theta_j;
    double inv_var = 1.0 / (kSigma[j] * kSigma[j]);

    // d/d(mu) from likelihood: residual / sigma_j^2
    grad[0] += residual * inv_var;

    // d/d(log_tau) from likelihood: d/d(theta_j) * d(theta_j)/d(tau) * d(tau)/d(log_tau)
    //   = (residual / sigma_j^2) * eta_j * tau
    grad[1] += residual * inv_var * eta_j * tau;

    // d/d(eta_j): prior term (-eta_j) + likelihood term (residual * tau / sigma_j^2)
    grad[2 + j] = -eta_j + residual * inv_var * tau;
  }

  return grad;
}

// =============================================================================
// Utility Functions
// =============================================================================

auto computeMean(const std::vector<double>& samples) -> double {
  if (samples.empty()) return 0.0;
  return std::accumulate(samples.begin(), samples.end(), 0.0) /
         static_cast<double>(samples.size());
}

auto computeStdDev(const std::vector<double>& samples, double mean) -> double {
  if (samples.size() < 2) return 0.0;
  double sum_sq = 0.0;
  for (double x : samples) {
    sum_sq += (x - mean) * (x - mean);
  }
  return std::sqrt(sum_sq / static_cast<double>(samples.size() - 1));
}

auto computeQuantile(std::vector<double> samples, double q) -> double {
  if (samples.empty()) return 0.0;
  std::sort(samples.begin(), samples.end());
  double idx = q * static_cast<double>(samples.size() - 1);
  auto lo = static_cast<size_t>(std::floor(idx));
  auto hi = static_cast<size_t>(std::ceil(idx));
  if (lo == hi) return samples[lo];
  double frac = idx - static_cast<double>(lo);
  return samples[lo] * (1.0 - frac) + samples[hi] * frac;
}

void printSectionHeader(std::string_view title) {
  std::println("\n{}", std::string(72, '='));
  std::println("{:^72}", title);
  std::println("{}\n", std::string(72, '='));
}

void printSubsectionHeader(std::string_view title) {
  std::println("\n--- {} ---\n", title);
}

// =============================================================================
// Main
// =============================================================================

auto main() -> int {
  std::println("======================================================================");
  std::println("     Eight Schools Hierarchical Model - Bayesian Inference with HMC  ");
  std::println("======================================================================");

  printSectionHeader("Model Description");

  std::println("The Eight Schools Problem (Rubin, 1981):");
  std::println("");
  std::println("  8 schools each report an estimated treatment effect y_j with");
  std::println("  known standard error sigma_j. We want to estimate the true");
  std::println("  school-specific effects theta_j while sharing information.");
  std::println("");
  std::println("Hierarchical Model:");
  std::println("  mu      ~ Normal(0, 5)           (population mean)");
  std::println("  tau     ~ HalfNormal(5)          (between-school std dev)");
  std::println("  theta_j ~ Normal(mu, tau)        (school effects)");
  std::println("  y_j     ~ Normal(theta_j, sigma_j) (observed, sigma_j known)");
  std::println("");
  std::println("Non-centered parameterization (for better sampling):");
  std::println("  eta_j   ~ Normal(0, 1)");
  std::println("  theta_j = mu + tau * eta_j");
  std::println("");
  std::println("State vector: [mu, log(tau), eta_1, ..., eta_8]  ({} parameters)", kStateDim);

  printSubsectionHeader("Data");

  std::println("  {:>8}  {:>6}  {:>8}", "School", "y_j", "sigma_j");
  std::println("  {:>8}  {:>6}  {:>8}", "------", "----", "-------");
  for (size_t j = 0; j < kNumSchools; ++j) {
    std::println("  {:>8}  {:>6.0f}  {:>8.0f}", j + 1, kY[j], kSigma[j]);
  }

  // =========================================================================
  // Run HMC
  // =========================================================================

  printSectionHeader("HMC Sampling");

  // HMC parameters
  constexpr double kEpsilon = 0.05;       // Step size (smaller for stability)
  constexpr size_t kLeapfrogSteps = 20;   // Number of leapfrog steps
  constexpr size_t kBurnIn = 1000;
  constexpr size_t kSamples = 4000;

  std::println("HMC parameters:");
  std::println("  Step size (epsilon):   {:.3f}", kEpsilon);
  std::println("  Leapfrog steps:        {}", kLeapfrogSteps);
  std::println("  Burn-in:               {} iterations", kBurnIn);
  std::println("  Samples:               {}", kSamples);

  auto hmc = bayes::makeHMC<double, kStateDim>(
      logPosterior, gradLogPosterior, kEpsilon, kLeapfrogSteps);

  // Initial state: mu=0, log_tau=1 (tau~2.7), eta_j=0
  State initial{};
  initial[0] = 0.0;    // mu
  initial[1] = 1.0;    // log_tau
  for (size_t j = 0; j < kNumSchools; ++j) {
    initial[2 + j] = 0.0;  // eta_j
  }

  std::mt19937 gen{42};
  bayes::Chain chain{hmc, gen, initial};

  std::println("\nRunning HMC...");

  // Burn-in
  chain.advance(kBurnIn);
  chain.resetStats();

  // Sample
  auto [raw_samples, acceptance_rate] = chain.takeWithStats(kSamples);

  std::println("  Acceptance rate: {:.1f}%", acceptance_rate * 100);

  // =========================================================================
  // Extract posterior samples
  // =========================================================================

  std::vector<double> mu_samples, tau_samples;
  std::array<std::vector<double>, kNumSchools> theta_samples;

  for (const auto& s : raw_samples) {
    double mu = s[0];
    double tau = std::exp(s[1]);
    mu_samples.push_back(mu);
    tau_samples.push_back(tau);

    for (size_t j = 0; j < kNumSchools; ++j) {
      double theta_j = mu + tau * s[2 + j];
      theta_samples[j].push_back(theta_j);
    }
  }

  // =========================================================================
  // Trace plots
  // =========================================================================

  printSectionHeader("Trace Plots");

  std::println("mu trace (first 300 samples):");
  std::vector<Point> mu_trace;
  for (size_t i = 0; i < std::min(size_t{300}, mu_samples.size()); ++i) {
    mu_trace.push_back({static_cast<double>(i), mu_samples[i]});
  }
  std::print("{}", scatterPlot(mu_trace, 70, 8, colors::kCyan));

  std::println("\ntau trace (first 300 samples):");
  std::vector<Point> tau_trace;
  for (size_t i = 0; i < std::min(size_t{300}, tau_samples.size()); ++i) {
    tau_trace.push_back({static_cast<double>(i), tau_samples[i]});
  }
  std::print("{}", scatterPlot(tau_trace, 70, 8, colors::kOrange));

  // =========================================================================
  // Posterior histograms
  // =========================================================================

  printSectionHeader("Posterior Histograms");

  printSubsectionHeader("Population Mean (mu)");

  double mu_mean = computeMean(mu_samples);
  double mu_std = computeStdDev(mu_samples, mu_mean);
  double mu_lo = computeQuantile(mu_samples, 0.025);
  double mu_hi = computeQuantile(mu_samples, 0.975);

  TextHistogramOptions hist_opts{
      .bins = 25,
      .width = 50,
      .color = colors::kCyan,
      .normalize = false,
  };
  std::print("{}", generateTextHistogram(mu_samples, hist_opts));
  std::println("\n  Mean: {:.2f}, Std: {:.2f}, 95%% CI: [{:.2f}, {:.2f}]",
               mu_mean, mu_std, mu_lo, mu_hi);

  printSubsectionHeader("Between-School Std Dev (tau)");

  double tau_mean = computeMean(tau_samples);
  double tau_std = computeStdDev(tau_samples, tau_mean);
  double tau_lo = computeQuantile(tau_samples, 0.025);
  double tau_hi = computeQuantile(tau_samples, 0.975);

  hist_opts.color = colors::kOrange;
  hist_opts.min_x = 0.0;
  hist_opts.max_x = std::nullopt;
  std::print("{}", generateTextHistogram(tau_samples, hist_opts));
  std::println("\n  Mean: {:.2f}, Std: {:.2f}, 95%% CI: [{:.2f}, {:.2f}]",
               tau_mean, tau_std, tau_lo, tau_hi);

  // =========================================================================
  // Shrinkage plot
  // =========================================================================

  printSectionHeader("Shrinkage Plot");

  std::println("Shows how the hierarchical model \"shrinks\" extreme observations");
  std::println("toward the population mean. The diagonal line is y = x (no pooling).\n");

  // Compute posterior means for each school
  std::array<double, kNumSchools> theta_means;
  for (size_t j = 0; j < kNumSchools; ++j) {
    theta_means[j] = computeMean(theta_samples[j]);
  }

  // Create scatter plot: observed y_j vs posterior mean theta_j
  std::vector<Point> shrinkage_points;
  for (size_t j = 0; j < kNumSchools; ++j) {
    shrinkage_points.push_back({kY[j], theta_means[j]});
  }

  std::println("Observed y_j (x-axis) vs Posterior mean theta_j (y-axis):");
  std::print("{}", scatterPlot(shrinkage_points, 60, 15, colors::kGreen));

  std::println("\n  {:>8}  {:>10}  {:>14}  {:>10}", "School", "y_j", "theta_j (post)", "Shrinkage");
  std::println("  {:>8}  {:>10}  {:>14}  {:>10}", "------", "----", "--------------", "---------");
  for (size_t j = 0; j < kNumSchools; ++j) {
    double shrinkage = kY[j] - theta_means[j];
    std::println("  {:>8}  {:>10.1f}  {:>14.2f}  {:>+10.2f}",
                 j + 1, kY[j], theta_means[j], shrinkage);
  }

  // =========================================================================
  // Forest plot
  // =========================================================================

  printSectionHeader("Forest Plot");

  std::println("School effects with 95%% credible intervals:\n");

  // Find the overall range for consistent scaling
  double forest_min = std::numeric_limits<double>::max();
  double forest_max = std::numeric_limits<double>::lowest();

  std::array<double, kNumSchools> theta_lo, theta_hi;
  for (size_t j = 0; j < kNumSchools; ++j) {
    theta_lo[j] = computeQuantile(theta_samples[j], 0.025);
    theta_hi[j] = computeQuantile(theta_samples[j], 0.975);
    forest_min = std::min(forest_min, theta_lo[j]);
    forest_max = std::max(forest_max, theta_hi[j]);
  }

  // Add some padding
  double forest_range = forest_max - forest_min;
  forest_min -= forest_range * 0.1;
  forest_max += forest_range * 0.1;
  forest_range = forest_max - forest_min;

  // ASCII forest plot
  constexpr int kPlotWidth = 50;
  constexpr int kHalfWidth = kPlotWidth / 2;
  std::println("  {:>8}  {:>6}  {}", "School", "Mean", "95% Credible Interval");
  std::println("  {:>8}  {:>6}  {:>25}|{:<25}", "", "", "", "");
  std::println("  {:>8}  {:>6}  {:<.1f}{:^44}{:.1f}", "", "",
               forest_min, "|", forest_max);
  std::println("");

  for (size_t j = 0; j < kNumSchools; ++j) {
    // Scale positions to plot width
    auto toPos = [&](double val) -> int {
      return static_cast<int>((val - forest_min) / forest_range * kPlotWidth);
    };

    int lo_pos = toPos(theta_lo[j]);
    int mean_pos = toPos(theta_means[j]);
    int hi_pos = toPos(theta_hi[j]);

    // Clamp to valid range
    lo_pos = std::clamp(lo_pos, 0, kPlotWidth - 1);
    mean_pos = std::clamp(mean_pos, 0, kPlotWidth - 1);
    hi_pos = std::clamp(hi_pos, 0, kPlotWidth - 1);

    // Build the line
    std::string line(static_cast<size_t>(kPlotWidth), ' ');

    // Draw the interval line
    for (int k = lo_pos; k <= hi_pos; ++k) {
      line[static_cast<size_t>(k)] = '-';
    }

    // Mark endpoints
    line[static_cast<size_t>(lo_pos)] = '[';
    line[static_cast<size_t>(hi_pos)] = ']';

    // Mark mean
    line[static_cast<size_t>(mean_pos)] = '*';

    std::println("  {:>8}  {:>6.2f}  {}",
                 j + 1, theta_means[j],
                 colors::kGreen.wrap(line));
  }

  // Add population mean
  std::println("");
  std::string pop_line(static_cast<size_t>(kPlotWidth), ' ');
  int pop_lo_pos = static_cast<int>((mu_lo - forest_min) / forest_range * kPlotWidth);
  int pop_mean_pos = static_cast<int>((mu_mean - forest_min) / forest_range * kPlotWidth);
  int pop_hi_pos = static_cast<int>((mu_hi - forest_min) / forest_range * kPlotWidth);
  pop_lo_pos = std::clamp(pop_lo_pos, 0, kPlotWidth - 1);
  pop_mean_pos = std::clamp(pop_mean_pos, 0, kPlotWidth - 1);
  pop_hi_pos = std::clamp(pop_hi_pos, 0, kPlotWidth - 1);

  for (int k = pop_lo_pos; k <= pop_hi_pos; ++k) {
    pop_line[static_cast<size_t>(k)] = '=';
  }
  pop_line[static_cast<size_t>(pop_lo_pos)] = '[';
  pop_line[static_cast<size_t>(pop_hi_pos)] = ']';
  pop_line[static_cast<size_t>(pop_mean_pos)] = '#';

  std::println("  {:>8}  {:>6.2f}  {}",
               "Pop(mu)", mu_mean,
               colors::kCyan.wrap(pop_line));

  // =========================================================================
  // Joint posterior (mu vs tau)
  // =========================================================================

  printSectionHeader("Joint Posterior: mu vs tau");

  std::vector<Point> joint_points;
  for (size_t i = 0; i < mu_samples.size(); ++i) {
    joint_points.push_back({mu_samples[i], tau_samples[i]});
  }

  DensityPlotOptions density_opts{
      .width = 60,
      .height = 18,
      .title = " mu vs tau ",
      .low_color = {10, 20, 50},
      .high_color = {100, 200, 255},
  };
  std::print("{}", smoothHeatmap(joint_points, density_opts));

  // =========================================================================
  // Summary table
  // =========================================================================

  printSectionHeader("Posterior Summary");

  std::println("  {:>14}  {:>8}  {:>8}  {:>10}  {:>10}",
               "Parameter", "Mean", "Std", "2.5%", "97.5%");
  std::println("  {:>14}  {:>8}  {:>8}  {:>10}  {:>10}",
               "--------------", "------", "------", "--------", "--------");

  std::println("  {:>14}  {:>8.2f}  {:>8.2f}  {:>10.2f}  {:>10.2f}",
               "mu", mu_mean, mu_std, mu_lo, mu_hi);
  std::println("  {:>14}  {:>8.2f}  {:>8.2f}  {:>10.2f}  {:>10.2f}",
               "tau", tau_mean, tau_std, tau_lo, tau_hi);

  for (size_t j = 0; j < kNumSchools; ++j) {
    double mean = computeMean(theta_samples[j]);
    double std_dev = computeStdDev(theta_samples[j], mean);
    double lo = computeQuantile(theta_samples[j], 0.025);
    double hi = computeQuantile(theta_samples[j], 0.975);

    std::println("  {:>10}[{}]  {:>8.2f}  {:>8.2f}  {:>10.2f}  {:>10.2f}",
                 "theta", j + 1, mean, std_dev, lo, hi);
  }

  // =========================================================================
  // Key insights
  // =========================================================================

  printSectionHeader("Key Insights");

  std::println("1. Hierarchical Shrinkage:");
  std::println("   - Schools with extreme observed effects (e.g., School 1: y=28)");
  std::println("     are \"shrunk\" toward the population mean.");
  std::println("   - This reflects the model's belief that extreme observations");
  std::println("     are more likely due to noise than true extreme effects.");
  std::println("");

  std::println("2. Uncertainty in tau:");
  std::println("   - The posterior for tau is right-skewed, with substantial mass");
  std::println("     near zero. This means the data are consistent with both:");
  std::println("     * Nearly identical effects (complete pooling)");
  std::println("     * Moderately varying effects (partial pooling)");
  std::println("");

  std::println("3. Non-centered Parameterization:");
  std::println("   - The model uses eta_j ~ N(0,1), theta_j = mu + tau*eta_j");
  std::println("   - This avoids the \"funnel\" geometry that makes sampling difficult");
  std::println("     when tau is small (eta becomes increasingly constrained).");
  std::println("");

  std::println("4. Partial Pooling:");
  std::println("   - The hierarchical model automatically finds a balance between:");
  std::println("     * No pooling (each school estimated independently)");
  std::println("     * Complete pooling (all schools have the same effect)");

  std::println("\n======================================================================");
  std::println("                            Report Complete                           ");
  std::println("======================================================================\n");

  return 0;
}
