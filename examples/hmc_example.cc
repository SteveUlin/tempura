// HMC vs Metropolis-Hastings sampling efficiency comparison.
//
// Demonstrates using the symbolic DSL to define a Bayesian model and
// then using the Posterior's gradient() method with HMC for efficient sampling.
//
// Model:
//   mu ~ Normal(0, 5)        (prior on mean)
//   sigma ~ HalfNormal(2)    (prior on scale, constrained positive)
//   y ~ Normal(mu, sigma)    (likelihood)
//
// We observe y = 3.5 and infer the posterior p(mu, sigma | y).

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <print>
#include <random>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "bayes/estimation/metropolis_hastings.h"
#include "bayes/sym/dsl.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::bayes::sym;
using namespace tempura::symbolic3;

// Alias for the bayes::Normal class to avoid collision with DSL's Normal function
using NormalDist = tempura::bayes::Normal<double>;

// =============================================================================
// Utility functions
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

// Compute lag-k autocorrelation
auto computeAutocorrelation(const std::vector<double>& samples, size_t lag)
    -> double {
  if (samples.size() <= lag) return 0.0;
  double mean = computeMean(samples);
  double var = 0.0;
  for (double x : samples) {
    var += (x - mean) * (x - mean);
  }
  if (var == 0.0) return 0.0;

  double cov = 0.0;
  for (size_t i = 0; i + lag < samples.size(); ++i) {
    cov += (samples[i] - mean) * (samples[i + lag] - mean);
  }

  return cov / var;
}

// Effective sample size: N / (1 + 2*sum(autocorrelations))
auto computeEffectiveSampleSize(const std::vector<double>& samples) -> double {
  double n = static_cast<double>(samples.size());
  double sum_rho = 0.0;

  // Sum autocorrelations until they become negligible
  for (size_t k = 1; k < samples.size() / 2; ++k) {
    double rho = computeAutocorrelation(samples, k);
    if (rho < 0.05) break;  // Stop when autocorrelation is small
    sum_rho += rho;
  }

  return n / (1.0 + 2.0 * sum_rho);
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
// Main example
// =============================================================================

auto main() -> int {
  std::println("======================================================================");
  std::println("       HMC vs Metropolis-Hastings: Sampling Efficiency Comparison     ");
  std::println("======================================================================");

  // =========================================================================
  // Define the model using the symbolic DSL
  // =========================================================================

  printSectionHeader("Model Definition using Symbolic DSL");

  constexpr Symbol mu;
  constexpr Symbol sigma;
  constexpr Symbol y;

  auto joint = Joint{
      mu    | Normal(0.0, 5.0),     // mu ~ Normal(0, 5)
      sigma | HalfNormal(2.0),   // sigma ~ HalfNormal(2)
      y     | Normal(mu, sigma)      // y ~ Normal(mu, sigma)
  };

  // Observe y = 3.5 and infer mu, sigma
  constexpr double kObservedY = 3.5;
  auto posterior = joint.observe(y = kObservedY).infer(mu, sigma);

  std::println("Joint distribution:");
  std::println("  mu    ~ Normal(0, 5)");
  std::println("  sigma ~ HalfNormal(2)");
  std::println("  y     ~ Normal(mu, sigma)");
  std::println("\nObservation: y = {:.1f}", kObservedY);
  std::println("Inferring: p(mu, sigma | y = {:.1f})", kObservedY);

  // Test the posterior evaluation
  std::println("\nPosterior test at (mu=2.0, sigma=1.5):");
  double test_lp = posterior.logProb(2.0, 1.5);
  auto test_grad = posterior.gradient(2.0, 1.5);
  std::println("  logProb  = {:.6f}", test_lp);
  std::println("  gradient = [{:.6f}, {:.6f}]", test_grad[0], test_grad[1]);

  // =========================================================================
  // Setup samplers - work in transformed space for sigma
  // =========================================================================
  // We sample log(sigma) to enforce positivity, then transform back

  printSectionHeader("Sampler Configuration");

  // State: [mu, log_sigma]
  using State = std::array<double, 2>;

  // Log-target wrapping the symbolic posterior (with Jacobian for log-transform)
  auto log_target = [&posterior](const State& state) -> double {
    double mu_val = state[0];
    double log_sigma = state[1];
    double sigma_val = std::exp(log_sigma);

    if (sigma_val < 1e-6) return -std::numeric_limits<double>::infinity();

    // Evaluate symbolic log-posterior + Jacobian adjustment
    return posterior.logProb(mu_val, sigma_val) + log_sigma;
  };

  // Gradient of log-target
  auto grad_log_target = [&posterior](const State& state) -> State {
    double mu_val = state[0];
    double log_sigma = state[1];
    double sigma_val = std::exp(log_sigma);

    if (sigma_val < 1e-6) return State{0.0, 0.0};

    auto grad = posterior.gradient(mu_val, sigma_val);

    // grad[0] = d/d(mu) log_posterior - unchanged
    // grad[1] = d/d(log_sigma) = d/d(sigma) * d(sigma)/d(log_sigma) + d/d(log_sigma) Jacobian
    //         = grad[1] * sigma + 1
    return State{grad[0], grad[1] * sigma_val + 1.0};
  };

  // Initial state
  State initial_state{1.0, 0.0};  // mu=1.0, log_sigma=0 (sigma=1)

  std::println("State representation: [mu, log(sigma)]");
  std::println("Initial state: [{:.1f}, {:.1f}] (mu={:.1f}, sigma={:.1f})",
               initial_state[0], initial_state[1],
               initial_state[0], std::exp(initial_state[1]));

  // HMC parameters
  constexpr double kHmcEpsilon = 0.1;   // Step size
  constexpr size_t kHmcLeapfrogSteps = 10;

  // MH parameters
  constexpr double kMhStepSize = 0.5;

  std::println("\nHMC parameters:");
  std::println("  Step size (epsilon): {:.2f}", kHmcEpsilon);
  std::println("  Leapfrog steps:      {}", kHmcLeapfrogSteps);

  std::println("\nMH parameters:");
  std::println("  Gaussian walk sigma: {:.2f}", kMhStepSize);

  // =========================================================================
  // Run HMC sampler
  // =========================================================================

  printSectionHeader("HMC Sampling");

  auto hmc_kernel = tempura::bayes::makeHMC<double, 2>(
      log_target, grad_log_target, kHmcEpsilon, kHmcLeapfrogSteps);

  constexpr size_t kBurnIn = 500;
  constexpr size_t kSamples = 2000;

  std::println("Running HMC...");
  std::println("  Burn-in: {} iterations", kBurnIn);
  std::println("  Samples: {}", kSamples);

  std::mt19937 hmc_gen{42};
  tempura::bayes::Chain hmc_chain{hmc_kernel, hmc_gen, initial_state};

  hmc_chain.advance(kBurnIn);
  auto [hmc_raw_samples, hmc_acceptance] = hmc_chain.takeWithStats(kSamples);

  // Extract individual parameters and transform sigma back
  std::vector<double> hmc_mu_samples, hmc_sigma_samples;
  std::vector<Point> hmc_joint_points;
  for (const auto& s : hmc_raw_samples) {
    hmc_mu_samples.push_back(s[0]);
    hmc_sigma_samples.push_back(std::exp(s[1]));
    hmc_joint_points.push_back({s[0], std::exp(s[1])});
  }

  std::println("\nHMC Results:");
  std::println("  Acceptance rate: {:.1f}%", hmc_acceptance * 100);

  // =========================================================================
  // Run MH sampler
  // =========================================================================

  printSectionHeader("Metropolis-Hastings Sampling");

  auto mh_kernel = tempura::bayes::makeMetropolisHastings<State>(
      log_target, tempura::bayes::GaussianWalkND<double, 2>{kMhStepSize});

  std::println("Running MH...");
  std::println("  Burn-in: {} iterations", kBurnIn);
  std::println("  Samples: {}", kSamples);

  std::mt19937 mh_gen{42};
  tempura::bayes::Chain mh_chain{mh_kernel, mh_gen, initial_state};

  mh_chain.advance(kBurnIn);
  auto [mh_raw_samples, mh_acceptance] = mh_chain.takeWithStats(kSamples);

  // Extract individual parameters
  std::vector<double> mh_mu_samples, mh_sigma_samples;
  std::vector<Point> mh_joint_points;
  for (const auto& s : mh_raw_samples) {
    mh_mu_samples.push_back(s[0]);
    mh_sigma_samples.push_back(std::exp(s[1]));
    mh_joint_points.push_back({s[0], std::exp(s[1])});
  }

  std::println("\nMH Results:");
  std::println("  Acceptance rate: {:.1f}%", mh_acceptance * 100);

  // =========================================================================
  // Compare sampling efficiency
  // =========================================================================

  printSectionHeader("Sampling Efficiency Comparison");

  // Compute statistics for HMC
  double hmc_mu_mean = computeMean(hmc_mu_samples);
  double hmc_mu_std = computeStdDev(hmc_mu_samples, hmc_mu_mean);
  double hmc_sigma_mean = computeMean(hmc_sigma_samples);
  double hmc_sigma_std = computeStdDev(hmc_sigma_samples, hmc_sigma_mean);

  double hmc_mu_ess = computeEffectiveSampleSize(hmc_mu_samples);
  double hmc_sigma_ess = computeEffectiveSampleSize(hmc_sigma_samples);

  double hmc_mu_acf1 = computeAutocorrelation(hmc_mu_samples, 1);
  double hmc_sigma_acf1 = computeAutocorrelation(hmc_sigma_samples, 1);

  // Compute statistics for MH
  double mh_mu_mean = computeMean(mh_mu_samples);
  double mh_mu_std = computeStdDev(mh_mu_samples, mh_mu_mean);
  double mh_sigma_mean = computeMean(mh_sigma_samples);
  double mh_sigma_std = computeStdDev(mh_sigma_samples, mh_sigma_mean);

  double mh_mu_ess = computeEffectiveSampleSize(mh_mu_samples);
  double mh_sigma_ess = computeEffectiveSampleSize(mh_sigma_samples);

  double mh_mu_acf1 = computeAutocorrelation(mh_mu_samples, 1);
  double mh_sigma_acf1 = computeAutocorrelation(mh_sigma_samples, 1);

  // Summary table
  std::println("  {:20}  {:>12}  {:>12}", "Metric", "HMC", "MH");
  std::println("  {:20}  {:>12}  {:>12}", "--------------------", "----", "----");
  std::println("  {:20}  {:>12.1f}%  {:>12.1f}%", "Acceptance Rate",
               hmc_acceptance * 100, mh_acceptance * 100);
  std::println("");
  std::println("  {:20}  {:>12.4f}  {:>12.4f}", "mu Mean",
               hmc_mu_mean, mh_mu_mean);
  std::println("  {:20}  {:>12.4f}  {:>12.4f}", "mu Std Dev",
               hmc_mu_std, mh_mu_std);
  std::println("  {:20}  {:>12.3f}  {:>12.3f}", "mu Lag-1 Autocorr",
               hmc_mu_acf1, mh_mu_acf1);
  std::println("  {:20}  {:>12.1f}  {:>12.1f}", "mu ESS",
               hmc_mu_ess, mh_mu_ess);
  std::println("");
  std::println("  {:20}  {:>12.4f}  {:>12.4f}", "sigma Mean",
               hmc_sigma_mean, mh_sigma_mean);
  std::println("  {:20}  {:>12.4f}  {:>12.4f}", "sigma Std Dev",
               hmc_sigma_std, mh_sigma_std);
  std::println("  {:20}  {:>12.3f}  {:>12.3f}", "sigma Lag-1 Autocorr",
               hmc_sigma_acf1, mh_sigma_acf1);
  std::println("  {:20}  {:>12.1f}  {:>12.1f}", "sigma ESS",
               hmc_sigma_ess, mh_sigma_ess);

  // ESS per sample ratio
  double ess_ratio_mu = hmc_mu_ess / mh_mu_ess;
  double ess_ratio_sigma = hmc_sigma_ess / mh_sigma_ess;

  std::println("\n  ESS Ratio (HMC / MH):");
  std::println("    mu:    {:.2f}x", ess_ratio_mu);
  std::println("    sigma: {:.2f}x", ess_ratio_sigma);

  // =========================================================================
  // Trace plots
  // =========================================================================

  printSubsectionHeader("Trace Plots (first 200 samples)");

  std::vector<Point> hmc_mu_trace, mh_mu_trace;
  std::vector<Point> hmc_sigma_trace, mh_sigma_trace;
  size_t trace_len = std::min(size_t{200}, hmc_mu_samples.size());

  for (size_t i = 0; i < trace_len; ++i) {
    double x = static_cast<double>(i);
    hmc_mu_trace.push_back({x, hmc_mu_samples[i]});
    mh_mu_trace.push_back({x, mh_mu_samples[i]});
    hmc_sigma_trace.push_back({x, hmc_sigma_samples[i]});
    mh_sigma_trace.push_back({x, mh_sigma_samples[i]});
  }

  std::println("HMC mu trace:");
  std::print("{}", scatterPlot(hmc_mu_trace, 60, 6, colors::kCyan));

  std::println("\nMH mu trace:");
  std::print("{}", scatterPlot(mh_mu_trace, 60, 6, colors::kOrange));

  std::println("\nHMC sigma trace:");
  std::print("{}", scatterPlot(hmc_sigma_trace, 60, 6, colors::kCyan));

  std::println("\nMH sigma trace:");
  std::print("{}", scatterPlot(mh_sigma_trace, 60, 6, colors::kOrange));

  // =========================================================================
  // Joint posterior heatmaps
  // =========================================================================

  printSubsectionHeader("Joint Posterior Heatmaps (mu vs sigma)");

  DensityPlotOptions density_opts{
      .width = 50,
      .height = 15,
      .low_color = {20, 20, 60},
      .high_color = {50, 200, 255},
  };

  std::println("HMC samples:");
  density_opts.title = " HMC: mu vs sigma ";
  std::print("{}", smoothHeatmap(hmc_joint_points, density_opts));

  std::println("\nMH samples:");
  density_opts.title = " MH: mu vs sigma ";
  density_opts.high_color = {255, 180, 50};
  std::print("{}", smoothHeatmap(mh_joint_points, density_opts));

  // =========================================================================
  // Histograms
  // =========================================================================

  printSubsectionHeader("Posterior Histograms");

  TextHistogramOptions hist_opts{
      .bins = 20,
      .width = 45,
      .color = colors::kCyan,
      .normalize = false,
  };

  std::println("HMC mu posterior:");
  std::print("{}", generateTextHistogram(hmc_mu_samples, hist_opts));

  std::println("\nMH mu posterior:");
  hist_opts.color = colors::kOrange;
  std::print("{}", generateTextHistogram(mh_mu_samples, hist_opts));

  std::println("\nHMC sigma posterior:");
  hist_opts.color = colors::kCyan;
  hist_opts.min_x = 0.0;
  hist_opts.max_x = 6.0;
  std::print("{}", generateTextHistogram(hmc_sigma_samples, hist_opts));

  std::println("\nMH sigma posterior:");
  hist_opts.color = colors::kOrange;
  std::print("{}", generateTextHistogram(mh_sigma_samples, hist_opts));

  // =========================================================================
  // Summary
  // =========================================================================

  printSectionHeader("Summary");

  std::println("Model: Normal-Normal with HalfNormal prior on sigma");
  std::println("Observation: y = {:.1f}\n", kObservedY);

  std::println("Posterior estimates (HMC):");
  double hmc_mu_lo = computeQuantile(hmc_mu_samples, 0.025);
  double hmc_mu_hi = computeQuantile(hmc_mu_samples, 0.975);
  double hmc_sigma_lo = computeQuantile(hmc_sigma_samples, 0.025);
  double hmc_sigma_hi = computeQuantile(hmc_sigma_samples, 0.975);
  std::println("  mu:    {:.3f} +/- {:.3f}  (95%% CI: [{:.3f}, {:.3f}])",
               hmc_mu_mean, hmc_mu_std, hmc_mu_lo, hmc_mu_hi);
  std::println("  sigma: {:.3f} +/- {:.3f}  (95%% CI: [{:.3f}, {:.3f}])",
               hmc_sigma_mean, hmc_sigma_std, hmc_sigma_lo, hmc_sigma_hi);

  std::println("\nKey findings:");
  std::println("  - HMC acceptance rate: {:.0f}%% vs MH: {:.0f}%%",
               hmc_acceptance * 100, mh_acceptance * 100);
  std::println("  - HMC produces {:.1f}x more effective samples for mu",
               ess_ratio_mu);
  std::println("  - HMC produces {:.1f}x more effective samples for sigma",
               ess_ratio_sigma);
  std::println("  - HMC shows lower autocorrelation (faster mixing)");
  std::println("  - HMC leverages symbolic gradients from the DSL");

  std::println("\n======================================================================");
  std::println("                            Report Complete                           ");
  std::println("======================================================================\n");

  return 0;
}
