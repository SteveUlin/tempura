// HMC Example using Symbolic4
//
// This example demonstrates Hamiltonian Monte Carlo sampling with a model
// defined using symbolic4's probabilistic programming DSL.
//
// Model:
//   mu ~ Normal(0, 5)           # prior on mean
//   sigma ~ HalfNormal(2)       # prior on std dev (positive)
//   y_i ~ Normal(mu, sigma)     # observed data (N=20)
//
// We sample from the posterior p(mu, sigma | y) using HMC.

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <print>
#include <random>
#include <vector>

#include "bayes/estimation/hmc.h"
#include "bayes/estimation/metropolis_hastings.h"
#include "bayes/normal.h"
#include "plot.h"
#include "symbolic4/infer.h"

using namespace tempura;
using namespace tempura::symbolic4;

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
  std::ranges::sort(samples);
  double idx = q * static_cast<double>(samples.size() - 1);
  auto lo = static_cast<size_t>(std::floor(idx));
  auto hi = static_cast<size_t>(std::ceil(idx));
  if (lo == hi) return samples[lo];
  double frac = idx - static_cast<double>(lo);
  return samples[lo] * (1.0 - frac) + samples[hi] * frac;
}

void printHeader(std::string_view title) {
  std::println("\n{}", std::string(72, '='));
  std::println("{:^72}", title);
  std::println("{}\n", std::string(72, '='));
}

// =============================================================================
// Main
// =============================================================================

auto main() -> int {
  std::println("======================================================================");
  std::println("        HMC Sampling with Symbolic4 Probabilistic Model              ");
  std::println("======================================================================");

  // =========================================================================
  // Generate synthetic data
  // =========================================================================

  printHeader("Generating Data");

  constexpr double kTrueMu = 3.0;
  constexpr double kTrueSigma = 1.5;
  constexpr size_t kNumObs = 20;

  std::mt19937 data_gen{12345};
  bayes::Normal<double> data_dist{kTrueMu, kTrueSigma};

  std::vector<double> observed_data;
  observed_data.reserve(kNumObs);
  for (size_t i = 0; i < kNumObs; ++i) {
    observed_data.push_back(data_dist.sample(data_gen));
  }

  double data_mean = computeMean(observed_data);
  double data_std = computeStdDev(observed_data, data_mean);

  std::println("True parameters:");
  std::println("  mu = {:.2f}", kTrueMu);
  std::println("  sigma = {:.2f}", kTrueSigma);
  std::println("");
  std::println("Generated {} observations:", kNumObs);
  std::println("  Sample mean = {:.2f}", data_mean);
  std::println("  Sample std = {:.2f}", data_std);

  // =========================================================================
  // Define model using symbolic4
  // =========================================================================

  printHeader("Symbolic Model Definition");

  // Define random variables
  auto mu = normal(0.0, 5.0);       // mu ~ Normal(0, 5)
  auto sigma = halfNormal(2.0);     // sigma ~ HalfNormal(2)

  // For likelihood, we need to sum over observations
  // Since we're doing manual HMC, we'll construct the log-posterior directly

  std::println("Prior:");
  std::println("  mu ~ Normal(0, 5)");
  std::println("  sigma ~ HalfNormal(2)");
  std::println("Likelihood:");
  std::println("  y_i ~ Normal(mu, sigma) for i = 1..{}", kNumObs);

  // Create posterior for prior log-prob and gradients
  // infer() returns HMC-ready posterior with automatic transforms and Jacobians
  auto prior_posterior = infer(mu, sigma).build();

  // =========================================================================
  // HMC Setup
  // =========================================================================

  printHeader("HMC Configuration");

  // State: [mu, log_sigma] (use log transform for sigma > 0)
  constexpr size_t kStateDim = 2;
  using State = std::array<double, kStateDim>;

  // Log-posterior function
  // infer() operates in unconstrained space: state = [mu, z_sigma] where sigma = exp(z_sigma)
  auto log_posterior = [&](const State& state) -> double {
    // Prior log-prob from symbolic model (includes Jacobian automatically)
    double log_prior = prior_posterior.logProb(state);

    // Get constrained values for likelihood computation
    auto constrained = prior_posterior.transform(state);
    double mu_val = constrained[0];
    double sigma_val = constrained[1];

    // Handle boundary
    if (sigma_val < 1e-8) return -std::numeric_limits<double>::infinity();

    // Likelihood: sum of log N(y_i | mu, sigma)
    double log_lik = 0.0;
    for (double y : observed_data) {
      double z = (y - mu_val) / sigma_val;
      log_lik += -0.5 * z * z - std::log(sigma_val);
    }
    // Constant term: -N/2 * log(2*pi)
    log_lik -= static_cast<double>(kNumObs) * 0.5 * std::log(2.0 * std::numbers::pi);

    return log_prior + log_lik;
  };

  // Gradient of log-posterior
  // The gradient is in unconstrained space, with chain rule applied automatically
  auto grad_log_posterior = [&](const State& state) -> State {
    // Get constrained values for likelihood gradient
    auto constrained = prior_posterior.transform(state);
    double mu_val = constrained[0];
    double sigma_val = constrained[1];

    if (sigma_val < 1e-8) return State{0.0, 0.0};

    // Prior gradient from symbolic model (already in unconstrained space with chain rule)
    auto prior_grad = prior_posterior.gradient(state);

    // Likelihood gradient (compute in constrained space, then apply chain rule)
    double sum_residual = 0.0;
    double sum_residual_sq = 0.0;
    for (double y : observed_data) {
      double residual = y - mu_val;
      sum_residual += residual;
      sum_residual_sq += residual * residual;
    }

    // d/d_mu of likelihood: sum((y_i - mu) / sigma^2)
    double grad_mu_lik = sum_residual / (sigma_val * sigma_val);

    // d/d_sigma of likelihood: sum((y-mu)^2/sigma^3 - 1/sigma)
    double grad_sigma_lik = sum_residual_sq / (sigma_val * sigma_val * sigma_val) -
                            static_cast<double>(kNumObs) / sigma_val;

    // Chain rule for sigma: d/dz = d/d_sigma * d_sigma/dz = d/d_sigma * sigma
    // (since sigma = exp(z), d_sigma/dz = sigma)
    double grad_z_sigma_lik = grad_sigma_lik * sigma_val;

    return State{prior_grad[0] + grad_mu_lik, prior_grad[1] + grad_z_sigma_lik};
  };

  // HMC parameters
  constexpr double kEpsilon = 0.05;
  constexpr size_t kLeapfrogSteps = 20;
  constexpr size_t kBurnIn = 500;
  constexpr size_t kSamples = 2000;

  std::println("HMC parameters:");
  std::println("  Step size (epsilon): {:.3f}", kEpsilon);
  std::println("  Leapfrog steps: {}", kLeapfrogSteps);
  std::println("  Burn-in: {}", kBurnIn);
  std::println("  Samples: {}", kSamples);

  auto hmc = bayes::makeHMC<double, kStateDim>(
      log_posterior, grad_log_posterior, kEpsilon, kLeapfrogSteps);

  // Initial state
  State initial{0.0, 0.0};  // mu=0, log_sigma=0 (sigma=1)

  std::mt19937 gen{42};
  bayes::Chain chain{hmc, gen, initial};

  // =========================================================================
  // Run HMC
  // =========================================================================

  printHeader("Running HMC");

  std::println("Burning in {} samples...", kBurnIn);
  chain.advance(kBurnIn);
  chain.resetStats();

  std::println("Collecting {} samples...", kSamples);
  auto [raw_samples, acceptance_rate] = chain.takeWithStats(kSamples);

  std::println("  Acceptance rate: {:.1f}%", acceptance_rate * 100);

  // Extract mu and sigma samples
  std::vector<double> mu_samples, sigma_samples;
  for (const auto& s : raw_samples) {
    mu_samples.push_back(s[0]);
    sigma_samples.push_back(std::exp(s[1]));
  }

  // =========================================================================
  // Results
  // =========================================================================

  printHeader("Posterior Summary");

  double mu_mean = computeMean(mu_samples);
  double mu_std = computeStdDev(mu_samples, mu_mean);
  double mu_lo = computeQuantile(mu_samples, 0.025);
  double mu_hi = computeQuantile(mu_samples, 0.975);

  double sigma_mean = computeMean(sigma_samples);
  double sigma_std = computeStdDev(sigma_samples, sigma_mean);
  double sigma_lo = computeQuantile(sigma_samples, 0.025);
  double sigma_hi = computeQuantile(sigma_samples, 0.975);

  std::println("  {:>10}  {:>8}  {:>8}  {:>10}  {:>10}  {:>8}",
               "Parameter", "Mean", "Std", "2.5%", "97.5%", "True");
  std::println("  {:>10}  {:>8}  {:>8}  {:>10}  {:>10}  {:>8}",
               "----------", "------", "------", "--------", "--------", "------");
  std::println("  {:>10}  {:>8.3f}  {:>8.3f}  {:>10.3f}  {:>10.3f}  {:>8.2f}",
               "mu", mu_mean, mu_std, mu_lo, mu_hi, kTrueMu);
  std::println("  {:>10}  {:>8.3f}  {:>8.3f}  {:>10.3f}  {:>10.3f}  {:>8.2f}",
               "sigma", sigma_mean, sigma_std, sigma_lo, sigma_hi, kTrueSigma);

  // =========================================================================
  // Histograms
  // =========================================================================

  printHeader("Posterior Distributions");

  std::println("mu:");
  TextHistogramOptions hist_opts{
      .bins = 25,
      .width = 50,
      .color = colors::kCyan,
  };
  std::print("{}", generateTextHistogram(mu_samples, hist_opts));
  std::println("  True mu = {:.2f} | Posterior mean = {:.2f}", kTrueMu, mu_mean);

  std::println("\nsigma:");
  hist_opts.color = colors::kGreen;
  hist_opts.min_x = 0.0;
  std::print("{}", generateTextHistogram(sigma_samples, hist_opts));
  std::println("  True sigma = {:.2f} | Posterior mean = {:.2f}", kTrueSigma, sigma_mean);

  // =========================================================================
  // Joint posterior
  // =========================================================================

  printHeader("Joint Posterior (mu vs sigma)");

  std::vector<Point> joint_points;
  for (size_t i = 0; i < mu_samples.size(); ++i) {
    joint_points.push_back({mu_samples[i], sigma_samples[i]});
  }

  DensityPlotOptions density_opts{
      .width = 60,
      .height = 15,
      .title = " mu vs sigma ",
      .low_color = {20, 30, 60},
      .high_color = {100, 200, 255},
  };
  std::print("{}", smoothHeatmap(joint_points, density_opts));

  std::println("\n======================================================================");
  std::println("                            Complete                                  ");
  std::println("======================================================================\n");

  return 0;
}
