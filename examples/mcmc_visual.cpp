#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <print>
#include <random>
#include <ranges>
#include <vector>

#include "bayes/estimation/mcmc.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::bayes;

auto main() -> int {
  std::println("╔══════════════════════════════════════════════════════════════════╗");
  std::println("║            MCMC Visualization Examples with Text Plots           ║");
  std::println("╚══════════════════════════════════════════════════════════════════╝\n");

  // Query terminal background color for seamless heatmap blending
  auto terminal_bg = queryTerminalBackground();
  if (terminal_bg) {
    std::println("Detected terminal background: RGB({}, {}, {})\n",
                 terminal_bg->r, terminal_bg->g, terminal_bg->b);
  } else {
    std::println("Could not detect terminal background (using defaults)\n");
  }

  // ============================================================================
  // Example 1: Sampling from a Normal Distribution
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Example 1: Sampling from N(μ=3, σ=1.5)");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  constexpr double kMu = 3.0;
  constexpr double kSigma = 1.5;

  auto normal_log_prob = [](double x) {
    return -0.5 * std::pow((x - kMu) / kSigma, 2);
  };

  auto mcmc_chain = chain<double>(normal_log_prob, GaussianWalk{1.0},
                                  std::mt19937{42}, 0.0);

  // Collect samples with burn-in
  std::vector<double> samples;
  std::vector<double> trace;
  constexpr int kBurnIn = 500;
  constexpr int kSamples = 3000;

  int i = 0;
  for (const auto& sample : mcmc_chain | std::views::take(kBurnIn + kSamples)) {
    trace.push_back(sample.value);
    if (i >= kBurnIn) {
      samples.push_back(sample.value);
    }
    ++i;
  }

  // 1a. Trace plot - shows chain exploration over time
  std::println("1a. Trace Plot (first 200 iterations, including burn-in)");
  std::println("    Shows how the chain explores the parameter space\n");

  std::vector<Point> trace_points;
  for (size_t j = 0; j < std::min(size_t{200}, trace.size()); ++j) {
    trace_points.push_back({static_cast<double>(j), trace[j]});
  }
  std::cout << scatterPlot(trace_points, 70, 12, colors::kCyan);
  std::println("");

  // 1b. Histogram of posterior samples
  std::println("1b. Posterior Histogram (after {} burn-in)", kBurnIn);
  std::println("    True mean = {:.1f}, True σ = {:.1f}\n", kMu, kSigma);

  TextHistogramOptions hist_opts{
      .bins = 20,
      .width = 50,
      .color = colors::kGreen,
      .normalize = false,
  };
  std::cout << generateTextHistogram(samples, hist_opts);

  // Compute sample statistics
  double sample_mean = 0;
  for (double x : samples) sample_mean += x;
  sample_mean /= static_cast<double>(samples.size());

  double sample_var = 0;
  for (double x : samples) sample_var += (x - sample_mean) * (x - sample_mean);
  sample_var /= static_cast<double>(samples.size() - 1);

  std::println("\n    Sample mean: {:.3f} (true: {:.1f})", sample_mean, kMu);
  std::println("    Sample std:  {:.3f} (true: {:.1f})", std::sqrt(sample_var), kSigma);
  std::println("");

  // ============================================================================
  // Example 2: Bimodal Distribution
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Example 2: Sampling from a Bimodal Distribution");
  std::println("             Mixture of N(-3, 0.8) and N(3, 0.8)");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  auto bimodal_log_prob = [](double x) {
    // log of mixture: log(0.5 * N(-3,0.8) + 0.5 * N(3,0.8))
    double mode1 = std::exp(-0.5 * std::pow((x + 3) / 0.8, 2));
    double mode2 = std::exp(-0.5 * std::pow((x - 3) / 0.8, 2));
    return std::log(0.5 * mode1 + 0.5 * mode2);
  };

  // Larger proposal helps jump between modes
  auto bimodal_chain = chain<double>(bimodal_log_prob, GaussianWalk{2.0},
                                     std::mt19937{123}, 0.0);

  std::vector<double> bimodal_samples;
  std::vector<double> bimodal_trace;

  i = 0;
  for (const auto& sample :
       bimodal_chain | std::views::take(1000 + 5000)) {
    bimodal_trace.push_back(sample.value);
    if (i >= 1000) {
      bimodal_samples.push_back(sample.value);
    }
    ++i;
  }

  std::println("2a. Trace Plot - Watch the chain jump between modes\n");

  std::vector<Point> bimodal_trace_pts;
  for (size_t j = 0; j < std::min(size_t{500}, bimodal_trace.size()); ++j) {
    bimodal_trace_pts.push_back({static_cast<double>(j), bimodal_trace[j]});
  }
  std::cout << scatterPlot(bimodal_trace_pts, 70, 12, colors::kMagenta);
  std::println("");

  std::println("2b. Bimodal Posterior Histogram\n");

  TextHistogramOptions bimodal_hist_opts{
      .bins = 25,
      .width = 50,
      .color = colors::kMagenta,
      .min_x = -6.0,
      .max_x = 6.0,
      .normalize = false,
  };
  std::cout << generateTextHistogram(bimodal_samples, bimodal_hist_opts);
  std::println("");

  // ============================================================================
  // Example 3: 2D Sampling - Correlated Bivariate Normal
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Example 3: 2D Sampling - Correlated Bivariate Normal");
  std::println("           μ = (1, 2), σ = (1, 1), ρ = 0.7");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // Bivariate normal with correlation
  constexpr double kMu1 = 1.0, kMu2 = 2.0;
  constexpr double kS1 = 1.0, kS2 = 1.0;
  constexpr double kRho = 0.7;  // Correlation

  auto bivariate_log_prob = [](const std::array<double, 2>& p) {
    double x = p[0], y = p[1];
    double z1 = (x - kMu1) / kS1;
    double z2 = (y - kMu2) / kS2;
    double q = (z1 * z1 - 2 * kRho * z1 * z2 + z2 * z2) / (1 - kRho * kRho);
    return -0.5 * q;
  };

  auto biv_chain = chain<std::array<double, 2>>(
      bivariate_log_prob, GaussianWalkND<double, 2>{0.5}, std::mt19937{456},
      std::array{0.0, 0.0});

  std::vector<Point> biv_samples;
  int biv_i = 0;
  for (const auto& sample : biv_chain | std::views::take(500 + 2000)) {
    if (biv_i >= 500) {
      biv_samples.push_back({sample.value[0], sample.value[1]});
    }
    ++biv_i;
  }

  std::println("2D Smooth Heatmap - Correlated samples (ρ = {})", kRho);
  std::println("   Uses ▀ half-blocks with fg+bg colors for 2x vertical resolution\n");

  DensityPlotOptions density_opts{
      .width = 50,
      .height = 20,
      .title = " Bivariate Normal ρ=0.7 ",
      .low_color = {30, 50, 90},       // Dark blue
      .high_color = {255, 220, 80},    // Bright gold
      .bg_color = terminal_bg,         // Use detected terminal background
  };
  std::cout << smoothHeatmap(biv_samples, density_opts);
  std::println("");

  // ============================================================================
  // Example 4: Comparing Proposal Distributions
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Example 4: Effect of Proposal Step Size on Mixing");
  std::println("           Same target N(0, 1), different σ_proposal");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  auto std_normal_log_prob = [](double x) { return -0.5 * x * x; };

  // Small step size (too small - slow mixing)
  auto chain_small = chain<double>(std_normal_log_prob, GaussianWalk{0.1},
                                   std::mt19937{789}, 0.0);

  // Good step size
  auto chain_good = chain<double>(std_normal_log_prob, GaussianWalk{1.0},
                                  std::mt19937{789}, 0.0);

  // Large step size (too large - low acceptance)
  auto chain_large = chain<double>(std_normal_log_prob, GaussianWalk{10.0},
                                   std::mt19937{789}, 0.0);

  std::vector<Point> trace_small, trace_good, trace_large;
  int accepted_small = 0, accepted_good = 0, accepted_large = 0;

  int idx = 0;
  for (const auto& s : chain_small | std::views::take(300)) {
    trace_small.push_back({static_cast<double>(idx), s.value});
    if (s.accepted) ++accepted_small;
    ++idx;
  }

  idx = 0;
  for (const auto& s : chain_good | std::views::take(300)) {
    trace_good.push_back({static_cast<double>(idx), s.value});
    if (s.accepted) ++accepted_good;
    ++idx;
  }

  idx = 0;
  for (const auto& s : chain_large | std::views::take(300)) {
    trace_large.push_back({static_cast<double>(idx), s.value});
    if (s.accepted) ++accepted_large;
    ++idx;
  }

  std::println("4a. {} σ_proposal = 0.1 (too small - random walk behavior)",
               colors::kRed.wrap("●"));
  std::println("    Acceptance rate: {:.1f}%\n",
               100.0 * accepted_small / 300.0);
  std::cout << scatterPlot(trace_small, 70, 8, colors::kRed);
  std::println("");

  std::println("4b. {} σ_proposal = 1.0 (good - efficient exploration)",
               colors::kGreen.wrap("●"));
  std::println("    Acceptance rate: {:.1f}%\n",
               100.0 * accepted_good / 300.0);
  std::cout << scatterPlot(trace_good, 70, 8, colors::kGreen);
  std::println("");

  std::println("4c. {} σ_proposal = 10.0 (too large - many rejections)",
               colors::kBlue.wrap("●"));
  std::println("    Acceptance rate: {:.1f}%\n",
               100.0 * accepted_large / 300.0);
  std::cout << scatterPlot(trace_large, 70, 8, colors::kBlue);
  std::println("");

  // ============================================================================
  // Example 5: Log-Probability Landscape with Samples
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Example 5: Target Density vs Samples");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  // Show the target PDF alongside the histogram
  std::println("Target: Gamma-like distribution (x > 0)\n");

  auto gamma_log_prob = [](double x) {
    if (x <= 0) return -std::numeric_limits<double>::infinity();
    // Gamma(shape=2, rate=0.5) ∝ x * exp(-0.5*x)
    return std::log(x) - 0.5 * x;
  };

  auto gamma_chain = chain<double>(gamma_log_prob, GaussianWalk{1.5},
                                   std::mt19937{999}, 1.0);

  std::vector<double> gamma_samples;
  for (const auto& s :
       gamma_chain | std::views::drop(500) | std::views::take(3000)) {
    gamma_samples.push_back(s.value);
  }

  // Plot the target PDF
  std::function<double(double)> gamma_pdf = [](double x) {
    if (x <= 0) return 0.0;
    return x * std::exp(-0.5 * x) / 4.0;  // Normalized
  };

  std::println("True PDF (Gamma(2, 0.5)):");
  PlotOptions pdf_opts{
      .width = 60,
      .height = 10,
      .color = colors::kCyan,
      .title = " Gamma PDF ",
      .show_border = true,
      .show_axes = true,
      .show_roots = false,
  };
  std::cout << plotFn(gamma_pdf, 0.0, 15.0, pdf_opts);
  std::println("");

  std::println("MCMC Samples Histogram:");
  TextHistogramOptions gamma_hist_opts{
      .bins = 20,
      .width = 50,
      .color = colors::kCyan,
      .min_x = 0.0,
      .max_x = 15.0,
      .normalize = false,
  };
  std::cout << generateTextHistogram(gamma_samples, gamma_hist_opts);
  std::println("");

  // ============================================================================
  // Example 6: Banana-shaped Distribution (challenging)
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Example 6: Banana-Shaped Distribution (challenging for MCMC)");
  std::println("           p(x,y) ∝ exp(-x²/200 - (y - x²/10)²/2)");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

  auto banana_log_prob = [](const std::array<double, 2>& p) {
    double x = p[0], y = p[1];
    return -x * x / 200.0 - std::pow(y - x * x / 10.0, 2) / 2.0;
  };

  auto banana_chain = chain<std::array<double, 2>>(
      banana_log_prob, GaussianWalkND<double, 2>{2.0}, std::mt19937{2024},
      std::array{0.0, 0.0});

  std::vector<Point> banana_samples;
  for (const auto& s :
       banana_chain | std::views::drop(2000) | std::views::take(5000)) {
    banana_samples.push_back({s.value[0], s.value[1]});
  }

  std::println("Banana Distribution - Smooth Heatmap:");

  DensityPlotOptions banana_opts{
      .width = 60,
      .height = 18,
      .title = " Banana Distribution ",
      .low_color = {10, 5, 25},        // Near-black purple
      .high_color = {255, 160, 40},    // Bright orange
      .bg_color = terminal_bg,         // Use detected terminal background
  };
  std::cout << smoothHeatmap(banana_samples, banana_opts);
  std::println("");

  // ============================================================================
  // Summary
  // ============================================================================
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("Summary: Key MCMC Concepts Illustrated");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  std::println("✓ Trace plots show chain exploration and convergence");
  std::println("✓ Histograms approximate the target posterior distribution");
  std::println("✓ Proposal step size affects mixing (aim for ~23-44%% acceptance)");
  std::println("✓ Multi-modal distributions need larger proposals to jump modes");
  std::println("✓ 2D sampling reveals correlations in the posterior");
  std::println("✓ Burn-in discards early samples before convergence");
  std::println("");

  return 0;
}
