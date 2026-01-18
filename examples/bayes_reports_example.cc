// Comprehensive Bayesian inference examples with text report generation.
//
// Demonstrates:
// 1. Coin flip (Beta-Binomial) model with MH MCMC
// 2. Linear regression with priors on slope, intercept, and noise
// 3. Normal-Normal conjugate model
//
// Each model generates a text-based report with histograms, heatmaps,
// trace plots, and summary statistics.

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <numeric>
#include <print>
#include <random>
#include <ranges>
#include <vector>
#include "bayes/estimation/metropolis_hastings.h"
#include "bayes/normal.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::bayes;

// =============================================================================
// Utility functions for statistics
// =============================================================================

auto computeMean(const std::vector<double>& samples) -> double {
  if (samples.empty()) return 0.0;
  double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  return sum / static_cast<double>(samples.size());
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

auto computeCredibleInterval(const std::vector<double>& samples, double alpha)
    -> std::pair<double, double> {
  // (1 - alpha) credible interval
  double lower = computeQuantile(samples, alpha / 2);
  double upper = computeQuantile(samples, 1.0 - alpha / 2);
  return {lower, upper};
}

void printSectionHeader(std::string_view title) {
  std::println("\n{}", std::string(72, '='));
  std::println("{:^72}", title);
  std::println("{}\n", std::string(72, '='));
}

void printSubsectionHeader(std::string_view title) {
  std::println("\n--- {} ---\n", title);
}

void printSummaryRow(std::string_view name, double mean, double std,
                     double ci_lo, double ci_hi) {
  std::println("  {:12}  {:>10.4f}  {:>10.4f}  [{:>8.4f}, {:>8.4f}]",
               name, mean, std, ci_lo, ci_hi);
}

// =============================================================================
// Example 1: Coin Flip Model (Beta-Binomial)
// =============================================================================
//
// Model:
//   p ~ Beta(alpha, beta)           (prior on probability of heads)
//   y_i | p ~ Bernoulli(p)          (likelihood)
//
// We observe some coin flips and infer the posterior p(p | y).

void runCoinFlipModel() {
  printSectionHeader("Example 1: Coin Flip Model (Beta-Binomial)");

  // Prior: Beta(2, 2) - weakly informative, slight preference for p=0.5
  constexpr double kPriorAlpha = 2.0;
  constexpr double kPriorBeta = 2.0;

  // Observed data: 7 heads out of 10 flips
  constexpr int kHeads = 7;
  constexpr int kTails = 3;
  constexpr int kTotal = kHeads + kTails;

  std::println("Prior: Beta({:.0f}, {:.0f})", kPriorAlpha, kPriorBeta);
  std::println("Data:  {} heads, {} tails ({} flips)\n", kHeads, kTails, kTotal);

  // Log-posterior: log(Beta(p; alpha + heads, beta + tails))
  // Using unnormalized log-posterior (proportional to posterior)
  auto log_posterior = [](double p) {
    if (p <= 0.0 || p >= 1.0) {
      return -std::numeric_limits<double>::infinity();
    }
    // log p^(alpha + heads - 1) * (1-p)^(beta + tails - 1)
    constexpr double alpha_post = kPriorAlpha + kHeads;
    constexpr double beta_post = kPriorBeta + kTails;
    return (alpha_post - 1.0) * std::log(p) +
           (beta_post - 1.0) * std::log(1.0 - p);
  };

  // MH sampler with Gaussian walk proposal (constrained to [0,1] by log_posterior)
  auto kernel = makeMetropolisHastings<double>(log_posterior, GaussianWalk{0.1});
  std::mt19937 gen{42};
  Chain chain{kernel, gen, 0.5};

  // Burn-in and sampling
  constexpr size_t kBurnIn = 1000;
  constexpr size_t kSamples = 5000;
  constexpr size_t kThin = 2;

  std::println("Running MH MCMC...");
  std::println("  Burn-in:  {} iterations", kBurnIn);
  std::println("  Samples:  {}", kSamples);
  std::println("  Thinning: {}", kThin);

  chain.advance(kBurnIn);
  auto [samples, acceptance_rate] = chain.takeWithStats(kSamples, kThin);

  std::println("  Acceptance rate: {:.1f}%\n", acceptance_rate * 100);

  // Collect trace for visualization (re-run a shorter chain)
  Chain trace_chain{kernel, std::mt19937{123}, 0.5};
  std::vector<Point> trace_points;
  for (size_t i = 0; i < 500; ++i) {
    double val = trace_chain.next();
    trace_points.push_back({static_cast<double>(i), val});
  }

  printSubsectionHeader("Trace Plot (first 500 iterations)");
  std::print("{}", scatterPlot(trace_points, 65, 10, colors::kCyan));

  printSubsectionHeader("Posterior Histogram of p");
  TextHistogramOptions hist_opts{
      .bins = 20,
      .width = 50,
      .color = colors::kGreen,
      .min_x = 0.0,
      .max_x = 1.0,
      .normalize = false,
  };
  std::print("{}", generateTextHistogram(samples, hist_opts));

  // Summary statistics
  printSubsectionHeader("Summary Statistics");
  double mean = computeMean(samples);
  double std_dev = computeStdDev(samples, mean);
  auto [ci_lo, ci_hi] = computeCredibleInterval(samples, 0.05);

  std::println("  {:12}  {:>10}  {:>10}  {:>20}", "Parameter", "Mean", "Std",
               "95% CI");
  std::println("  {:12}  {:>10}  {:>10}  {:>20}", "---------", "----", "---",
               "------");
  printSummaryRow("p", mean, std_dev, ci_lo, ci_hi);

  // Compare to analytic posterior
  double analytic_mean = (kPriorAlpha + kHeads) /
                         (kPriorAlpha + kPriorBeta + kTotal);
  std::println("\n  Analytic posterior mean (Beta({}, {})): {:.4f}",
               kPriorAlpha + kHeads, kPriorBeta + kTails, analytic_mean);
  std::println("  MCMC estimate matches analytic: {}",
               std::abs(mean - analytic_mean) < 0.02 ? "YES" : "NO");
}

// =============================================================================
// Example 2: Linear Regression Model
// =============================================================================
//
// Model:
//   slope ~ Normal(0, 5)
//   intercept ~ Normal(0, 5)
//   sigma ~ HalfNormal(2)
//   y_i | x_i, slope, intercept, sigma ~ Normal(slope * x_i + intercept, sigma)
//
// True parameters: slope = 2.0, intercept = 1.0, sigma = 0.5

void runLinearRegressionModel() {
  printSectionHeader("Example 2: Linear Regression Model");

  // Generate synthetic data: y = 2x + 1 + noise
  constexpr double kTrueSlope = 2.0;
  constexpr double kTrueIntercept = 1.0;
  constexpr double kTrueSigma = 0.5;
  constexpr size_t kDataPoints = 30;

  std::println("True parameters:");
  std::println("  slope     = {:.1f}", kTrueSlope);
  std::println("  intercept = {:.1f}", kTrueIntercept);
  std::println("  sigma     = {:.1f}\n", kTrueSigma);

  std::mt19937 data_gen{123};
  Normal<double> noise{0.0, kTrueSigma};

  std::vector<double> x_data, y_data;
  for (size_t i = 0; i < kDataPoints; ++i) {
    double x = static_cast<double>(i) / static_cast<double>(kDataPoints - 1) * 4.0 - 2.0;
    double y = kTrueSlope * x + kTrueIntercept + noise.sample(data_gen);
    x_data.push_back(x);
    y_data.push_back(y);
  }

  std::println("Generated {} data points in x = [-2, 2]\n", kDataPoints);

  // Priors
  std::println("Priors:");
  std::println("  slope     ~ Normal(0, 5)");
  std::println("  intercept ~ Normal(0, 5)");
  std::println("  sigma     ~ HalfNormal(2)\n");

  // Log-posterior: log p(slope, intercept, sigma | data)
  // State: [slope, intercept, log_sigma]
  // We sample log_sigma to enforce positivity

  auto log_posterior = [&x_data, &y_data](const std::array<double, 3>& state) {
    double slope = state[0];
    double intercept = state[1];
    double log_sigma = state[2];
    double sigma = std::exp(log_sigma);

    if (sigma < 1e-6) return -std::numeric_limits<double>::infinity();

    double log_prior = 0.0;

    // slope ~ Normal(0, 5): log p = -0.5 * (slope/5)^2 + const
    log_prior += -0.5 * (slope / 5.0) * (slope / 5.0);

    // intercept ~ Normal(0, 5)
    log_prior += -0.5 * (intercept / 5.0) * (intercept / 5.0);

    // sigma ~ HalfNormal(2): log p = -0.5 * (sigma/2)^2 + const
    // Plus Jacobian for log-transform: add log_sigma
    log_prior += -0.5 * (sigma / 2.0) * (sigma / 2.0) + log_sigma;

    // Likelihood: product of Normal(y_i | slope*x_i + intercept, sigma)
    double log_lik = 0.0;
    for (size_t i = 0; i < x_data.size(); ++i) {
      double mu = slope * x_data[i] + intercept;
      double z = (y_data[i] - mu) / sigma;
      log_lik += -0.5 * z * z - std::log(sigma);
    }

    return log_prior + log_lik;
  };

  // MH sampler
  auto kernel = makeMetropolisHastings<std::array<double, 3>>(
      log_posterior, GaussianWalkND<double, 3>{0.15});
  std::mt19937 gen{456};
  Chain chain{kernel, gen, std::array{0.0, 0.0, 0.0}};

  constexpr size_t kBurnIn = 2000;
  constexpr size_t kSamples = 8000;
  constexpr size_t kThin = 3;

  std::println("Running MH MCMC...");
  std::println("  Burn-in:  {} iterations", kBurnIn);
  std::println("  Samples:  {}", kSamples);
  std::println("  Thinning: {}", kThin);

  chain.advance(kBurnIn);
  auto [raw_samples, acceptance_rate] = chain.takeWithStats(kSamples, kThin);

  std::println("  Acceptance rate: {:.1f}%\n", acceptance_rate * 100);

  // Extract individual parameter samples
  std::vector<double> slope_samples, intercept_samples, sigma_samples;
  std::vector<Point> joint_points;

  for (const auto& s : raw_samples) {
    slope_samples.push_back(s[0]);
    intercept_samples.push_back(s[1]);
    sigma_samples.push_back(std::exp(s[2]));
    joint_points.push_back({s[0], s[1]});  // slope vs intercept
  }

  // Trace plots
  printSubsectionHeader("Trace Plots (first 300 iterations after burn-in)");

  std::vector<Point> slope_trace, intercept_trace, sigma_trace;
  for (size_t i = 0; i < std::min(size_t{300}, slope_samples.size()); ++i) {
    slope_trace.push_back({static_cast<double>(i), slope_samples[i]});
    intercept_trace.push_back({static_cast<double>(i), intercept_samples[i]});
    sigma_trace.push_back({static_cast<double>(i), sigma_samples[i]});
  }

  std::println("Slope trace:");
  std::print("{}", scatterPlot(slope_trace, 60, 6, colors::kRed));

  std::println("\nIntercept trace:");
  std::print("{}", scatterPlot(intercept_trace, 60, 6, colors::kGreen));

  std::println("\nSigma trace:");
  std::print("{}", scatterPlot(sigma_trace, 60, 6, colors::kBlue));

  // Histograms
  printSubsectionHeader("Posterior Histograms");

  TextHistogramOptions reg_hist_opts{
      .bins = 18,
      .width = 45,
      .normalize = false,
  };

  std::println("Slope posterior (true = {:.1f}):", kTrueSlope);
  reg_hist_opts.color = colors::kRed;
  std::print("{}", generateTextHistogram(slope_samples, reg_hist_opts));

  std::println("\nIntercept posterior (true = {:.1f}):", kTrueIntercept);
  reg_hist_opts.color = colors::kGreen;
  std::print("{}", generateTextHistogram(intercept_samples, reg_hist_opts));

  std::println("\nSigma posterior (true = {:.1f}):", kTrueSigma);
  reg_hist_opts.color = colors::kBlue;
  reg_hist_opts.min_x = 0.0;
  reg_hist_opts.max_x = 2.0;
  std::print("{}", generateTextHistogram(sigma_samples, reg_hist_opts));

  // Joint posterior heatmap (slope vs intercept)
  printSubsectionHeader("Joint Posterior: Slope vs Intercept");

  DensityPlotOptions density_opts{
      .width = 55,
      .height = 18,
      .title = " Slope vs Intercept ",
      .low_color = {20, 20, 60},
      .high_color = {255, 200, 50},
  };
  std::print("{}", smoothHeatmap(joint_points, density_opts));

  // Summary table
  printSubsectionHeader("Summary Statistics");

  std::println("  {:12}  {:>10}  {:>10}  {:>20}  {:>10}",
               "Parameter", "Mean", "Std", "95% CI", "True");
  std::println("  {:12}  {:>10}  {:>10}  {:>20}  {:>10}",
               "---------", "----", "---", "------", "----");

  double slope_mean = computeMean(slope_samples);
  double slope_std = computeStdDev(slope_samples, slope_mean);
  auto [slope_lo, slope_hi] = computeCredibleInterval(slope_samples, 0.05);
  std::println("  {:12}  {:>10.4f}  {:>10.4f}  [{:>8.4f}, {:>8.4f}]  {:>10.1f}",
               "slope", slope_mean, slope_std, slope_lo, slope_hi, kTrueSlope);

  double int_mean = computeMean(intercept_samples);
  double int_std = computeStdDev(intercept_samples, int_mean);
  auto [int_lo, int_hi] = computeCredibleInterval(intercept_samples, 0.05);
  std::println("  {:12}  {:>10.4f}  {:>10.4f}  [{:>8.4f}, {:>8.4f}]  {:>10.1f}",
               "intercept", int_mean, int_std, int_lo, int_hi, kTrueIntercept);

  double sig_mean = computeMean(sigma_samples);
  double sig_std = computeStdDev(sigma_samples, sig_mean);
  auto [sig_lo, sig_hi] = computeCredibleInterval(sigma_samples, 0.05);
  std::println("  {:12}  {:>10.4f}  {:>10.4f}  [{:>8.4f}, {:>8.4f}]  {:>10.1f}",
               "sigma", sig_mean, sig_std, sig_lo, sig_hi, kTrueSigma);

  // Check if true values are within CIs
  std::println("\n  True values within 95%% CI:");
  std::println("    slope:     {} ({})",
               (kTrueSlope >= slope_lo && kTrueSlope <= slope_hi) ? "YES" : "NO",
               std::format("[{:.2f}, {:.2f}]", slope_lo, slope_hi));
  std::println("    intercept: {} ({})",
               (kTrueIntercept >= int_lo && kTrueIntercept <= int_hi) ? "YES" : "NO",
               std::format("[{:.2f}, {:.2f}]", int_lo, int_hi));
  std::println("    sigma:     {} ({})",
               (kTrueSigma >= sig_lo && kTrueSigma <= sig_hi) ? "YES" : "NO",
               std::format("[{:.2f}, {:.2f}]", sig_lo, sig_hi));
}

// =============================================================================
// Example 3: Normal-Normal Conjugate Model
// =============================================================================
//
// Model (known variance):
//   mu ~ Normal(mu_0, sigma_0)       (prior)
//   y_i | mu ~ Normal(mu, sigma)     (likelihood, sigma known)
//
// Posterior: mu | y ~ Normal(mu_n, sigma_n) where
//   precision_n = precision_0 + n * precision_lik
//   mu_n = (precision_0 * mu_0 + precision_lik * sum(y)) / precision_n

void runNormalNormalModel() {
  printSectionHeader("Example 3: Normal-Normal Conjugate Model");

  // Prior: mu ~ Normal(0, 10)
  constexpr double kPriorMu = 0.0;
  constexpr double kPriorSigma = 10.0;

  // Known likelihood variance
  constexpr double kKnownSigma = 2.0;

  // True mean
  constexpr double kTrueMu = 5.0;

  std::println("Prior: mu ~ Normal({:.0f}, {:.0f})", kPriorMu, kPriorSigma);
  std::println("Likelihood: y_i | mu ~ Normal(mu, {:.0f}) (known sigma)", kKnownSigma);
  std::println("True mu = {:.1f}\n", kTrueMu);

  // Generate observations
  constexpr size_t kNumObs = 15;
  std::mt19937 data_gen{789};
  Normal<double> likelihood{kTrueMu, kKnownSigma};

  std::vector<double> observations;
  double sum_y = 0.0;
  for (size_t i = 0; i < kNumObs; ++i) {
    double y = likelihood.sample(data_gen);
    observations.push_back(y);
    sum_y += y;
  }

  std::print("Observations (n={}): [", kNumObs);
  for (size_t i = 0; i < std::min(size_t{8}, observations.size()); ++i) {
    std::print("{:.2f}", observations[i]);
    if (i < 7) std::print(", ");
  }
  std::println(", ...]");
  std::println("Sample mean: {:.3f}\n", sum_y / static_cast<double>(kNumObs));

  // Analytic posterior
  double prior_prec = 1.0 / (kPriorSigma * kPriorSigma);
  double lik_prec = 1.0 / (kKnownSigma * kKnownSigma);
  double post_prec = prior_prec + static_cast<double>(kNumObs) * lik_prec;
  double post_sigma = std::sqrt(1.0 / post_prec);
  double post_mu = (prior_prec * kPriorMu + lik_prec * sum_y) / post_prec;

  std::println("Analytic posterior: mu | y ~ Normal({:.4f}, {:.4f})",
               post_mu, post_sigma);

  // Log-posterior for MCMC
  auto log_posterior = [&](double mu) {
    // Prior: -0.5 * ((mu - mu_0) / sigma_0)^2
    double log_prior = -0.5 * std::pow((mu - kPriorMu) / kPriorSigma, 2);

    // Likelihood: sum of -0.5 * ((y_i - mu) / sigma)^2
    double log_lik = 0.0;
    for (double y : observations) {
      log_lik += -0.5 * std::pow((y - mu) / kKnownSigma, 2);
    }

    return log_prior + log_lik;
  };

  // MH sampler
  auto kernel = makeMetropolisHastings<double>(log_posterior, GaussianWalk{0.5});
  std::mt19937 gen{321};
  Chain chain{kernel, gen, 0.0};

  constexpr size_t kBurnIn = 500;
  constexpr size_t kSamples = 4000;

  std::println("\nRunning MH MCMC...");
  std::println("  Burn-in:  {} iterations", kBurnIn);
  std::println("  Samples:  {}", kSamples);

  chain.advance(kBurnIn);
  auto [samples, acceptance_rate] = chain.takeWithStats(kSamples);

  std::println("  Acceptance rate: {:.1f}%\n", acceptance_rate * 100);

  // Trace plot
  printSubsectionHeader("Trace Plot");
  std::vector<Point> trace_points;
  for (size_t i = 0; i < std::min(size_t{400}, samples.size()); ++i) {
    trace_points.push_back({static_cast<double>(i), samples[i]});
  }
  std::print("{}", scatterPlot(trace_points, 60, 8, colors::kOrange));

  // Histogram
  printSubsectionHeader("Posterior Distribution of mu");
  TextHistogramOptions hist_opts{
      .bins = 20,
      .width = 50,
      .color = colors::kOrange,
      .normalize = false,
  };
  std::print("{}", generateTextHistogram(samples, hist_opts));

  // Overlay analytic posterior PDF
  std::println("\nAnalytic posterior PDF:");
  std::function<double(double)> analytic_pdf = [post_mu, post_sigma](double x) {
    double z = (x - post_mu) / post_sigma;
    return std::exp(-0.5 * z * z) / (post_sigma * std::sqrt(2.0 * std::numbers::pi));
  };

  double plot_min = post_mu - 4.0 * post_sigma;
  double plot_max = post_mu + 4.0 * post_sigma;
  PlotOptions plot_opts{
      .width = 55,
      .height = 10,
      .color = colors::kOrange,
      .title = " Analytic Posterior ",
      .show_border = true,
      .show_axes = true,
      .show_roots = false,
  };
  std::print("{}", plotFn(analytic_pdf, plot_min, plot_max, plot_opts));

  // Summary
  printSubsectionHeader("Comparison: MCMC vs Analytic");

  double mcmc_mean = computeMean(samples);
  double mcmc_std = computeStdDev(samples, mcmc_mean);
  auto [mcmc_lo, mcmc_hi] = computeCredibleInterval(samples, 0.05);

  // Analytic 95% CI: mu +/- 1.96 * sigma
  double analytic_lo = post_mu - 1.96 * post_sigma;
  double analytic_hi = post_mu + 1.96 * post_sigma;

  std::println("  {:15}  {:>12}  {:>12}", "", "MCMC", "Analytic");
  std::println("  {:15}  {:>12}  {:>12}", "---------------", "----", "--------");
  std::println("  {:15}  {:>12.4f}  {:>12.4f}", "Mean", mcmc_mean, post_mu);
  std::println("  {:15}  {:>12.4f}  {:>12.4f}", "Std Dev", mcmc_std, post_sigma);
  std::println("  {:15}  {:>12.4f}  {:>12.4f}", "95% CI Lower", mcmc_lo, analytic_lo);
  std::println("  {:15}  {:>12.4f}  {:>12.4f}", "95% CI Upper", mcmc_hi, analytic_hi);

  std::println("\n  Mean error: {:.4f} ({:.2f}%% of posterior std)",
               std::abs(mcmc_mean - post_mu),
               100.0 * std::abs(mcmc_mean - post_mu) / post_sigma);
}

// =============================================================================
// Main
// =============================================================================

auto main() -> int {
  std::println("======================================================================");
  std::println("         Bayesian Inference Examples with Text Report Generation      ");
  std::println("======================================================================");

  // Query terminal background for better heatmap rendering
  auto terminal_bg = queryTerminalBackground();
  if (terminal_bg) {
    std::println("\nDetected terminal background: RGB({}, {}, {})",
                 terminal_bg->r, terminal_bg->g, terminal_bg->b);
  }

  runCoinFlipModel();
  runLinearRegressionModel();
  runNormalNormalModel();

  std::println("\n======================================================================");
  std::println("                            Report Complete                           ");
  std::println("======================================================================\n");

  return 0;
}
