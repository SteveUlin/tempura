// Bayesian Predictive Checks Example
//
// Demonstrates prior and posterior predictive checking for model validation.
//
// Model (Normal-Normal):
//   Prior:      mu ~ Normal(0, 10)
//   Likelihood: y_i ~ Normal(mu, sigma=2) for i=1..N
//
// Prior predictive:  "What data would my model generate before seeing observations?"
// Posterior predictive: "What data would my model generate after fitting?"
//
// Comparing these to actual data helps diagnose model misspecification.

#include <algorithm>
#include <cmath>
#include <numeric>
#include <print>
#include <random>
#include <vector>

#include "bayes/estimation/metropolis_hastings.h"
#include "bayes/estimation/predictive.h"
#include "bayes/normal.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::bayes;

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
  auto lo = static_cast<std::size_t>(std::floor(idx));
  auto hi = static_cast<std::size_t>(std::ceil(idx));
  if (lo == hi) return samples[lo];
  double frac = idx - static_cast<double>(lo);
  return samples[lo] * (1.0 - frac) + samples[hi] * frac;
}

void printHeader(std::string_view title) {
  std::println("\n{}", std::string(72, '='));
  std::println("{:^72}", title);
  std::println("{}\n", std::string(72, '='));
}

void printSubheader(std::string_view title) {
  std::println("\n--- {} ---\n", title);
}

// =============================================================================
// Main Example
// =============================================================================

auto main() -> int {
  std::println("======================================================================");
  std::println("       Bayesian Predictive Checks: Prior and Posterior               ");
  std::println("======================================================================");

  // =========================================================================
  // Model Setup
  // =========================================================================

  printHeader("Model Specification");

  // Model parameters
  constexpr double kPriorMu = 0.0;       // Prior mean for mu
  constexpr double kPriorSigma = 10.0;   // Prior std for mu
  constexpr double kLikelihoodSigma = 2.0;  // Known observation noise
  constexpr std::size_t kNumObs = 20;       // Number of observations

  std::println("Prior:      mu ~ Normal({:.1f}, {:.1f})", kPriorMu, kPriorSigma);
  std::println("Likelihood: y_i ~ Normal(mu, {:.1f}) for i=1..{}", kLikelihoodSigma, kNumObs);

  // =========================================================================
  // Generate Synthetic Data
  // =========================================================================

  printSubheader("Generating Synthetic Data");

  constexpr double kTrueMu = 3.5;  // True value of mu (unknown to inference)
  std::mt19937 data_gen{12345};
  Normal<double> data_dist{kTrueMu, kLikelihoodSigma};

  std::vector<double> observed_data;
  observed_data.reserve(kNumObs);
  for (std::size_t i = 0; i < kNumObs; ++i) {
    observed_data.push_back(data_dist.sample(data_gen));
  }

  double data_mean = computeMean(observed_data);
  double data_std = computeStdDev(observed_data, data_mean);

  std::println("True mu:        {:.2f}", kTrueMu);
  std::println("Observed mean:  {:.2f}", data_mean);
  std::println("Observed std:   {:.2f}", data_std);
  std::println("N observations: {}", kNumObs);

  // =========================================================================
  // Prior Predictive Check
  // =========================================================================

  printHeader("Prior Predictive Check");

  std::println("Question: Does my prior generate sensible data?");
  std::println("Sampling from prior predictive: p(y) = integral p(y|mu) p(mu) d(mu)\n");

  // Create prior sampler: mu ~ Normal(0, 10)
  auto prior_sampler = [](auto& g) {
    return Normal<double>{kPriorMu, kPriorSigma}.sample(g);
  };

  // Create likelihood sampler: y ~ Normal(mu, 2)
  auto likelihood_sampler = [](double mu, auto& g) {
    return Normal<double>{mu, kLikelihoodSigma}.sample(g);
  };

  // Build prior predictive
  auto prior_predictive = makePriorPredictive(prior_sampler, likelihood_sampler);

  // Sample from prior predictive
  std::mt19937 prior_gen{42};
  constexpr std::size_t kNumPriorSamples = 10000;
  auto prior_pred_samples = prior_predictive.sampleN(prior_gen, kNumPriorSamples);

  // Compute statistics
  double prior_pred_mean = computeMean(prior_pred_samples);
  double prior_pred_std = computeStdDev(prior_pred_samples, prior_pred_mean);
  double prior_pred_2_5 = computeQuantile(prior_pred_samples, 0.025);
  double prior_pred_97_5 = computeQuantile(prior_pred_samples, 0.975);

  std::println("Prior Predictive Statistics ({} samples):", kNumPriorSamples);
  std::println("  Mean:  {:.2f}", prior_pred_mean);
  std::println("  Std:   {:.2f}", prior_pred_std);
  std::println("  95%% CI: [{:.2f}, {:.2f}]", prior_pred_2_5, prior_pred_97_5);

  // Compare to observed data
  auto prior_check = computePredictiveCheck(data_mean, prior_pred_samples);
  std::println("\nPrior Predictive Check (comparing observed mean to prior predictive):");
  std::println("  Observed mean:     {:.2f}", prior_check.observed);
  std::println("  Predictive mean:   {:.2f}", prior_check.predictive_mean);
  std::println("  Predictive std:    {:.2f}", prior_check.predictive_std);
  std::println("  p-value:           {:.3f}", prior_check.p_value);

  if (prior_check.p_value > 0.05 && prior_check.p_value < 0.95) {
    std::println("  Interpretation: Observed data is consistent with prior (p in [0.05, 0.95])");
  } else {
    std::println("  Interpretation: Observed data is somewhat extreme under prior");
  }

  // Histogram of prior predictive
  printSubheader("Prior Predictive Distribution");

  TextHistogramOptions hist_opts{
      .bins = 25,
      .width = 50,
      .color = colors::kCyan,
      .min_x = -30.0,
      .max_x = 30.0,
  };
  std::print("{}", generateTextHistogram(prior_pred_samples, hist_opts));

  std::println("\n  Observed data mean marked at: {:.2f}", data_mean);

  // =========================================================================
  // Fit Posterior via Metropolis-Hastings
  // =========================================================================

  printHeader("Posterior Inference via MCMC");

  // Log-posterior: log p(mu | y) propto log p(y | mu) + log p(mu)
  auto log_posterior = [&](double mu) {
    // Prior: mu ~ Normal(0, 10)
    double log_prior = Normal<double>{kPriorMu, kPriorSigma}.logProb(mu);

    // Likelihood: y_i ~ Normal(mu, 2) for all observations
    double log_lik = 0.0;
    for (double y : observed_data) {
      log_lik += Normal<double>{mu, kLikelihoodSigma}.logProb(y);
    }

    return log_prior + log_lik;
  };

  // Create MH sampler
  auto mh_kernel = makeMetropolisHastings<double>(
      log_posterior,
      GaussianWalk<double>{0.5}  // Proposal: random walk with sigma=0.5
  );

  std::mt19937 mh_gen{123};
  Chain mh_chain{mh_kernel, mh_gen, 0.0};  // Start at mu=0

  // Burn-in
  constexpr std::size_t kBurnIn = 1000;
  constexpr std::size_t kPosteriorSamples = 5000;
  constexpr std::size_t kThin = 2;

  std::println("MCMC Settings:");
  std::println("  Burn-in:   {}", kBurnIn);
  std::println("  Samples:   {}", kPosteriorSamples);
  std::println("  Thinning:  {}", kThin);

  mh_chain.advance(kBurnIn);
  auto [posterior_samples, acceptance_rate] = mh_chain.takeWithStats(kPosteriorSamples, kThin);

  double post_mean = computeMean(posterior_samples);
  double post_std = computeStdDev(posterior_samples, post_mean);
  double post_2_5 = computeQuantile(posterior_samples, 0.025);
  double post_97_5 = computeQuantile(posterior_samples, 0.975);

  std::println("\nPosterior Statistics:");
  std::println("  Mean:      {:.3f}", post_mean);
  std::println("  Std:       {:.3f}", post_std);
  std::println("  95%% CI:    [{:.3f}, {:.3f}]", post_2_5, post_97_5);
  std::println("  Acceptance rate: {:.1f}%%", acceptance_rate * 100);

  // Analytic posterior for comparison (conjugate Normal-Normal)
  // Posterior: mu | y ~ Normal(mu_n, sigma_n)
  // sigma_n^2 = 1 / (1/sigma_0^2 + n/sigma^2)
  // mu_n = sigma_n^2 * (mu_0/sigma_0^2 + n*ybar/sigma^2)
  double prior_precision = 1.0 / (kPriorSigma * kPriorSigma);
  double lik_precision = 1.0 / (kLikelihoodSigma * kLikelihoodSigma);
  double post_precision = prior_precision + static_cast<double>(kNumObs) * lik_precision;
  double post_var_analytic = 1.0 / post_precision;
  double post_std_analytic = std::sqrt(post_var_analytic);
  double post_mean_analytic = post_var_analytic *
      (kPriorMu * prior_precision + static_cast<double>(kNumObs) * data_mean * lik_precision);

  std::println("\nAnalytic Posterior (for comparison):");
  std::println("  Mean: {:.3f}", post_mean_analytic);
  std::println("  Std:  {:.3f}", post_std_analytic);

  // Histogram of posterior
  printSubheader("Posterior Distribution of mu");

  hist_opts.min_x = post_mean - 4 * post_std;
  hist_opts.max_x = post_mean + 4 * post_std;
  hist_opts.color = colors::kGreen;
  std::print("{}", generateTextHistogram(posterior_samples, hist_opts));

  std::println("\n  True mu = {:.2f} (vertical line)", kTrueMu);

  // =========================================================================
  // Posterior Predictive Check
  // =========================================================================

  printHeader("Posterior Predictive Check");

  std::println("Question: Does my fitted model reproduce the observed data?");
  std::println("Sampling from posterior predictive: p(y*|y) = integral p(y*|mu) p(mu|y) d(mu)\n");

  // Build posterior predictive
  auto post_predictive = makePosteriorPredictive(
      posterior_samples,
      likelihood_sampler
  );

  std::println("Posterior predictive uses {} posterior samples", post_predictive.numSamples());

  // Sample from posterior predictive
  std::mt19937 post_pred_gen{456};
  constexpr std::size_t kNumPostPredSamples = 10000;
  auto post_pred_samples = post_predictive.sampleN(post_pred_gen, kNumPostPredSamples);

  // Compute statistics
  double post_pred_mean = computeMean(post_pred_samples);
  double post_pred_std = computeStdDev(post_pred_samples, post_pred_mean);
  double post_pred_2_5 = computeQuantile(post_pred_samples, 0.025);
  double post_pred_97_5 = computeQuantile(post_pred_samples, 0.975);

  std::println("\nPosterior Predictive Statistics ({} samples):", kNumPostPredSamples);
  std::println("  Mean:  {:.2f}", post_pred_mean);
  std::println("  Std:   {:.2f}", post_pred_std);
  std::println("  95%% CI: [{:.2f}, {:.2f}]", post_pred_2_5, post_pred_97_5);

  // Compare to observed data
  auto post_check = computePredictiveCheck(data_mean, post_pred_samples);
  std::println("\nPosterior Predictive Check (comparing observed mean to posterior predictive):");
  std::println("  Observed mean:     {:.2f}", post_check.observed);
  std::println("  Predictive mean:   {:.2f}", post_check.predictive_mean);
  std::println("  Predictive std:    {:.2f}", post_check.predictive_std);
  std::println("  p-value:           {:.3f}", post_check.p_value);

  if (post_check.p_value > 0.05 && post_check.p_value < 0.95) {
    std::println("  Interpretation: Model fits data well (p in [0.05, 0.95])");
  } else {
    std::println("  Interpretation: Possible model misspecification (extreme p-value)");
  }

  // =========================================================================
  // Visual Comparison: Histograms
  // =========================================================================

  printHeader("Visual Comparison");

  printSubheader("Prior Predictive vs Posterior Predictive");

  // Determine shared x-axis range
  double x_min = std::min({
      computeQuantile(prior_pred_samples, 0.001),
      computeQuantile(post_pred_samples, 0.001),
      *std::min_element(observed_data.begin(), observed_data.end())
  });
  double x_max = std::max({
      computeQuantile(prior_pred_samples, 0.999),
      computeQuantile(post_pred_samples, 0.999),
      *std::max_element(observed_data.begin(), observed_data.end())
  });

  // Add some padding
  double range = x_max - x_min;
  x_min -= range * 0.05;
  x_max += range * 0.05;

  std::println("Prior Predictive (before seeing data):");
  hist_opts.min_x = x_min;
  hist_opts.max_x = x_max;
  hist_opts.color = colors::kCyan;
  hist_opts.bins = 30;
  std::print("{}", generateTextHistogram(prior_pred_samples, hist_opts));

  std::println("\nPosterior Predictive (after fitting):");
  hist_opts.color = colors::kGreen;
  std::print("{}", generateTextHistogram(post_pred_samples, hist_opts));

  std::println("\nObserved Data (N={}):", kNumObs);
  hist_opts.color = colors::kOrange;
  hist_opts.bins = 10;
  std::print("{}", generateTextHistogram(observed_data, hist_opts));

  // =========================================================================
  // Individual Observation Checks
  // =========================================================================

  printSubheader("Individual Observation p-values");

  std::println("Checking each observed data point against posterior predictive:\n");
  std::println("  {:>4}  {:>8}  {:>8}  {:>10}", "Obs", "y_i", "p-value", "Status");
  std::println("  {:>4}  {:>8}  {:>8}  {:>10}", "----", "--------", "--------", "----------");

  int extreme_count = 0;
  for (std::size_t i = 0; i < observed_data.size(); ++i) {
    auto check = computePredictiveCheckTwoSided(observed_data[i], post_pred_samples);
    std::string status = (check.p_value < 0.05) ? "EXTREME" : "OK";
    if (check.p_value < 0.05) ++extreme_count;
    std::println("  {:>4}  {:>8.2f}  {:>8.3f}  {:>10}", i + 1, observed_data[i],
                 check.p_value, status);
  }

  std::println("\n  {} of {} observations flagged as extreme (expected ~1 by chance)",
               extreme_count, kNumObs);

  // =========================================================================
  // Summary
  // =========================================================================

  printHeader("Summary");

  std::println("Model: Normal-Normal conjugate model");
  std::println("  Prior:      mu ~ Normal({:.1f}, {:.1f})", kPriorMu, kPriorSigma);
  std::println("  Likelihood: y_i ~ Normal(mu, {:.1f})", kLikelihoodSigma);
  std::println("  True mu:    {:.2f}", kTrueMu);
  std::println("");
  std::println("Posterior Inference:");
  std::println("  Estimated mu: {:.3f} +/- {:.3f}", post_mean, post_std);
  std::println("  95%% CI:       [{:.3f}, {:.3f}]", post_2_5, post_97_5);
  std::println("  True mu in CI: {}", (kTrueMu >= post_2_5 && kTrueMu <= post_97_5) ? "Yes" : "No");
  std::println("");
  std::println("Predictive Checks:");
  std::println("  Prior predictive p-value:     {:.3f} (data vs prior)", prior_check.p_value);
  std::println("  Posterior predictive p-value: {:.3f} (data vs fitted model)", post_check.p_value);
  std::println("");
  std::println("Interpretation:");
  std::println("  - Prior predictive check: Is the data plausible under my prior beliefs?");
  std::println("  - Posterior predictive check: Does the fitted model capture the data?");
  std::println("  - p-values near 0.5 suggest good model fit");
  std::println("  - p-values near 0 or 1 suggest model misspecification");

  std::println("\n======================================================================");
  std::println("                          Report Complete                             ");
  std::println("======================================================================\n");

  return 0;
}
