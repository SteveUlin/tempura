#include "bayes/random.h"
#include "bayes/rng_tests.h"

#include <chrono>
#include <format>
#include <iomanip>
#include <iostream>
#include <print>
#include <random>
#include <string>
#include <vector>

using namespace tempura;

// ============================================================================
// Report Generation for RNG Quality Assessment
// ============================================================================

struct RngTestReport {
  std::string name;
  RngUniformityTestResult uniformity;
  RngIndependenceTestResult independence;
  RngPiEstimationResult pi_estimation;
  RngRunsTestResult runs;
  RngGapTestResult gap;
  RngBitQualityResult bit_quality;
  double elapsed_seconds;  // Time to run all tests
};

void printHeader() {
  std::println("");
  std::println("╔════════════════════════════════════════════════════════════════════════╗");
  std::println("║          Bayesian Random Number Generator Quality Report              ║");
  std::println("╟────────────────────────────────────────────────────────────────────────╢");
  std::println("║  Framework: Full Bayesian Test Suite (6 tests)                        ║");
  std::println("║    • Uniformity (Dirichlet-Multinomial)                                ║");
  std::println("║    • Independence (Fisher z-transform)                                 ║");
  std::println("║    • π Estimation (Beta-Binomial)                                      ║");
  std::println("║    • Runs Test (Normal approximation)                                  ║");
  std::println("║    • Gap Test (Beta-Geometric)                                         ║");
  std::println("║    • Hierarchical Bit Quality (Empirical Bayes)                        ║");
  std::println("║                                                                        ║");
  std::println("║  Generators Tested: 24 total                                           ║");
  std::println("║    • 1 Combined (makeRandom)                                           ║");
  std::println("║    • 5 XorShift-Left presets                                           ║");
  std::println("║    • 5 XorShift-Right presets                                          ║");
  std::println("║    • 5 MultiplyWithCarry presets                                       ║");
  std::println("║    • 3 LinearCongruential presets                                      ║");
  std::println("║    • 5 C++ stdlib (mt19937, minstd, ranlux)                            ║");
  std::println("║                                                                        ║");
  std::println("║  Sample Size: 100k (uniformity/bits), 50k (indep/runs/gap), 1M (π)    ║");
  std::println("║  STRESS TEST: 1000 bins (challenging uniformity requirements)          ║");
  std::println("╚════════════════════════════════════════════════════════════════════════╝");
  std::println("");
}

void printSectionHeader(const std::string& title) {
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println(" {}", title);
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("");
}

auto assessQuality(double uniformity_dev, double correlation, double pi_error)
    -> std::string {
  // Heuristic quality assessment
  int score = 0;

  // Uniformity: max_deviation < 0.01 is excellent
  if (uniformity_dev < 0.01)
    score += 3;
  else if (uniformity_dev < 0.05)
    score += 2;
  else if (uniformity_dev < 0.1)
    score += 1;

  // Independence: |correlation| < 0.01 is excellent
  if (std::abs(correlation) < 0.01)
    score += 3;
  else if (std::abs(correlation) < 0.05)
    score += 2;
  else if (std::abs(correlation) < 0.1)
    score += 1;

  // π estimation: error < 0.01 is excellent
  if (pi_error < 0.01)
    score += 3;
  else if (pi_error < 0.05)
    score += 2;
  else if (pi_error < 0.1)
    score += 1;

  if (score >= 8)
    return "★★★ EXCELLENT";
  else if (score >= 6)
    return "★★  GOOD";
  else if (score >= 4)
    return "★   ACCEPTABLE";
  else
    return "✗   POOR";
}

void printRngReport(const RngTestReport& report) {
  size_t padding = 70 - report.name.length();
  std::print("┌─ {} ", report.name);
  for (size_t i = 0; i < padding; ++i) std::print("─");
  std::println("");
  std::println("│");

  // Uniformity Test Results
  std::println("│ 📊 Uniformity Test (Dirichlet-Multinomial):");
  std::println("│   • Max deviation from uniform: {:.6f}",
               report.uniformity.max_deviation);
  std::println("│   • Posterior probability nearly uniform: {:.3f}",
               report.uniformity.prob_nearly_uniform);
  std::println("│   • Log marginal likelihood: {:.2f}",
               report.uniformity.log_marginal_likelihood);

  std::string uniformity_verdict =
      report.uniformity.max_deviation < 0.05 ? "✓ PASS" : "✗ FAIL";
  std::println("│   → Verdict: {}", uniformity_verdict);
  std::println("│");

  // Independence Test Results
  std::println("│ 🔗 Independence Test (Serial Correlation):");
  std::println("│   • Sample correlation ρ: {:.6f}",
               report.independence.sample_correlation);
  std::println("│   • 95%% credible interval: [{:.4f}, {:.4f}]",
               report.independence.credible_interval_95.first,
               report.independence.credible_interval_95.second);
  std::println("│   • P(|ρ| < 0.01 | data): {:.3f}",
               report.independence.prob_independent);

  bool ci_contains_zero = report.independence.credible_interval_95.first < 0.0 &&
                         report.independence.credible_interval_95.second > 0.0;
  std::string indep_verdict =
      (std::abs(report.independence.sample_correlation) < 0.1 && ci_contains_zero)
          ? "✓ PASS"
          : "✗ FAIL";
  std::println("│   → Verdict: {}", indep_verdict);
  std::println("│");

  // π Estimation Test Results
  std::println("│ 🎯 Monte Carlo π Estimation:");
  std::println("│   • Estimated π: {:.10f}", report.pi_estimation.pi_estimate);
  std::println("│   • Absolute error: {:.6f}", report.pi_estimation.pi_error);
  std::println("│   • 95%% credible interval: [{:.6f}, {:.6f}]",
               report.pi_estimation.pi_credible_interval.first,
               report.pi_estimation.pi_credible_interval.second);
  std::println("│   • Hit rate: {}/{} = {:.6f}",
               report.pi_estimation.n_hits,
               report.pi_estimation.n_samples,
               static_cast<double>(report.pi_estimation.n_hits) /
                   static_cast<double>(report.pi_estimation.n_samples));

  std::string pi_verdict = report.pi_estimation.pi_error < 0.01 ? "✓ PASS" : "✗ FAIL";
  std::println("│   → Verdict: {}", pi_verdict);
  std::println("│");

  // Runs Test Results
  std::println("│ 🔄 Runs Test (Monotone Sequences):");
  std::println("│   • Total runs: {} (expected: {:.0f})",
               report.runs.total_runs, report.runs.expected_runs);
  std::println("│   • Ascending runs: {}", report.runs.n_ascending_runs);
  std::println("│   • Descending runs: {}", report.runs.n_descending_runs);
  std::println("│   • P(consistent | data): {:.3f}", report.runs.prob_consistent);

  double runs_ratio = static_cast<double>(report.runs.total_runs) / report.runs.expected_runs;
  std::string runs_verdict = (runs_ratio > 0.9 && runs_ratio < 1.1 && report.runs.prob_consistent > 0.5)
                              ? "✓ PASS" : "✗ FAIL";
  std::println("│   → Verdict: {}", runs_verdict);
  std::println("│");

  // Gap Test Results
  std::println("│ 📏 Gap Test (Spacing Distribution):");
  std::println("│   • Gaps observed: {}", report.gap.n_gaps);
  std::println("│   • Posterior mean p: {:.4f}", report.gap.posterior_mean_gap_param);
  std::println("│   • 95%% CI: [{:.4f}, {:.4f}]",
               report.gap.credible_interval_95.first,
               report.gap.credible_interval_95.second);
  std::println("│   • P(geometric | data): {:.3f}", report.gap.prob_geometric);

  std::string gap_verdict = report.gap.prob_geometric > 0.5 ? "✓ PASS" : "✗ FAIL";
  std::println("│   → Verdict: {}", gap_verdict);
  std::println("│");

  // Bit Quality Test Results
  std::println("│ 🎲 Hierarchical Bit Quality (64 bits):");
  std::println("│   • Population mean: {:.6f}", report.bit_quality.population_mean);
  std::println("│   • Population precision: {:.2f}", report.bit_quality.population_precision);
  std::println("│   • Problematic bits: {}", report.bit_quality.problematic_bits.size());
  std::println("│   • Overall quality score: {:.3f}", report.bit_quality.overall_quality_score);

  std::string bit_verdict = report.bit_quality.overall_quality_score > 0.9 ? "✓ PASS" : "✗ FAIL";
  std::println("│   → Verdict: {}", bit_verdict);
  std::println("│");

  // Overall Assessment
  std::string overall = assessQuality(
      report.uniformity.max_deviation,
      report.independence.sample_correlation,
      report.pi_estimation.pi_error);
  std::println("│ Overall Quality: {}", overall);
  std::println("│");
  std::println("│ ⏱️  Test Duration: {:.3f} seconds", report.elapsed_seconds);
  std::print("└");
  for (int i = 0; i < 75; ++i) std::print("─");
  std::println("");
  std::println("");
}

void printComparativeTable(const std::vector<RngTestReport>& reports) {
  printSectionHeader("COMPARATIVE SUMMARY (Absolute Values)");

  // Table header - expanded to include all 6 tests
  std::println("┌──────────────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐");
  std::println("│ Generator        │Uniformity│  Indep   │ π Error  │  Runs    │   Gap    │   Bits   │  Quality │ Time (s) │");
  std::println("│                  │ (maxdev) │  (|ρ|)   │          │  (P>0.5) │ (P>0.5)  │  (score) │          │          │");
  std::println("├──────────────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤");

  for (const auto& report : reports) {
    std::string name = report.name;
    if (name.length() > 16) {
      name = name.substr(0, 13) + "...";
    }

    std::string quality = assessQuality(
        report.uniformity.max_deviation,
        report.independence.sample_correlation,
        report.pi_estimation.pi_error);

    // Shorten quality for table
    if (quality.find("EXCELLENT") != std::string::npos) quality = "★★★";
    else if (quality.find("GOOD") != std::string::npos) quality = "★★ ";
    else if (quality.find("ACCEPTABLE") != std::string::npos) quality = "★  ";
    else quality = "✗  ";

    std::println("│ {:<16} │ {:.6f} │ {:.6f} │ {:.6f} │   {:.3f}  │  {:.3f}  │  {:.3f}  │   {:<5}  │  {:6.3f}  │",
                name,
                report.uniformity.max_deviation,
                std::abs(report.independence.sample_correlation),
                report.pi_estimation.pi_error,
                report.runs.prob_consistent,
                report.gap.prob_geometric,
                report.bit_quality.overall_quality_score,
                quality,
                report.elapsed_seconds);
  }

  std::println("└──────────────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘");
  std::println("");
  std::println("Note: All probability values are posterior probabilities from Bayesian inference");
  std::println("      Higher is better for Runs, Gap, and Bits (>0.5 or >0.9 indicates good quality)");
  std::println("");
}

void printNormalizedTable(const std::vector<RngTestReport>& reports) {
  printSectionHeader("COMPARATIVE SUMMARY (Normalized to std::mt19937_64)");

  // Find mt19937_64 baseline
  const RngTestReport* baseline = nullptr;
  for (const auto& report : reports) {
    if (report.name == "std::mt19937_64") {
      baseline = &report;
      break;
    }
  }

  if (!baseline) {
    std::println("Warning: mt19937_64 baseline not found!");
    return;
  }

  double baseline_uniformity = baseline->uniformity.max_deviation;
  double baseline_independence = std::abs(baseline->independence.sample_correlation);
  double baseline_pi = baseline->pi_estimation.pi_error;
  double baseline_time = baseline->elapsed_seconds;

  double baseline_runs = baseline->runs.prob_consistent;
  double baseline_gap = baseline->gap.prob_geometric;
  double baseline_bits = baseline->bit_quality.overall_quality_score;

  std::println("Baseline (mt19937_64):");
  std::println("  • Uniformity deviation: {:.6f}", baseline_uniformity);
  std::println("  • Independence |ρ|: {:.6f}", baseline_independence);
  std::println("  • π error: {:.6f}", baseline_pi);
  std::println("  • Runs P(consistent): {:.3f}", baseline_runs);
  std::println("  • Gap P(geometric): {:.3f}", baseline_gap);
  std::println("  • Bit quality score: {:.3f}", baseline_bits);
  std::println("  • Time: {:.3f} seconds", baseline_time);
  std::println("");
  std::println("Relative Performance (>1.0 = better than mt19937_64, <1.0 = worse):");
  std::println("  Quality ratios: For errors (uniform/indep/π): baseline / current (lower error = better)");
  std::println("                  For probabilities (runs/gap/bits): current / baseline (higher prob = better)");
  std::println("  Speed ratio: baseline / current (lower time = faster)");
  std::println("");

  // Table header - expanded
  std::println("┌──────────────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐");
  std::println("│ Generator        │Uniformity│  Indep   │ π Error  │  Runs    │   Gap    │   Bits   │  Speed   │ Overall  │");
  std::println("│                  │ (ratio)  │ (ratio)  │ (ratio)  │ (ratio)  │ (ratio)  │ (ratio)  │ (ratio)  │(geo mean)│");
  std::println("├──────────────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤");

  for (const auto& report : reports) {
    std::string name = report.name;
    if (name.length() > 16) {
      name = name.substr(0, 13) + "...";
    }

    // For error metrics (uniformity, independence, pi), lower is better
    // ratio = baseline / current, so >1.0 means current is better
    double uniformity_ratio = baseline_uniformity / report.uniformity.max_deviation;
    double independence_ratio = baseline_independence / std::abs(report.independence.sample_correlation);
    double pi_ratio = baseline_pi / report.pi_estimation.pi_error;

    // For probability metrics (runs, gap, bits), higher is better
    // ratio = current / baseline, so >1.0 means current is better
    double runs_ratio = report.runs.prob_consistent / baseline_runs;
    double gap_ratio = report.gap.prob_geometric / baseline_gap;
    double bits_ratio = report.bit_quality.overall_quality_score / baseline_bits;

    // For speed, lower time is better
    // ratio = baseline / current, so >1.0 means current is faster
    double speed_ratio = baseline_time / report.elapsed_seconds;

    // Geometric mean of all 7 ratios for overall score
    double overall = std::pow(uniformity_ratio * independence_ratio * pi_ratio *
                              runs_ratio * gap_ratio * bits_ratio * speed_ratio, 1.0 / 7.0);

    // Color coding via symbols
    auto format_ratio = [](double ratio) -> std::string {
      if (ratio >= 1.2) return std::format("{:6.3f}↑↑", ratio);  // Much better
      else if (ratio >= 1.05) return std::format("{:6.3f}↑ ", ratio);  // Better
      else if (ratio >= 0.95) return std::format("{:6.3f}  ", ratio);  // Similar
      else if (ratio >= 0.8) return std::format("{:6.3f}↓ ", ratio);  // Worse
      else return std::format("{:6.3f}↓↓", ratio);  // Much worse
    };

    std::println("│ {:<16} │ {} │ {} │ {} │ {} │ {} │ {} │ {} │ {} │",
                name,
                format_ratio(uniformity_ratio),
                format_ratio(independence_ratio),
                format_ratio(pi_ratio),
                format_ratio(runs_ratio),
                format_ratio(gap_ratio),
                format_ratio(bits_ratio),
                format_ratio(speed_ratio),
                format_ratio(overall));
  }

  std::println("└──────────────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘");
  std::println("");
  std::println("Legend:");
  std::println("  ↑↑  Much better than mt19937_64 (≥20% improvement)");
  std::println("  ↑   Better than mt19937_64 (5-20% improvement)");
  std::println("      Similar to mt19937_64 (within ±5%)");
  std::println("  ↓   Worse than mt19937_64 (5-20% degradation)");
  std::println("  ↓↓  Much worse than mt19937_64 (≥20% degradation)");
  std::println("");
  std::println("Note: Overall score is geometric mean of all 7 ratios (6 quality tests + speed)");
  std::println("");
}

void printMethodology() {
  printSectionHeader("METHODOLOGY");

  std::println("Statistical Framework:");
  std::println("  • Bayesian inference with conjugate priors");
  std::println("  • Direct probability statements (no p-values)");
  std::println("  • Full posterior distributions over quality parameters");
  std::println("  • Hierarchical modeling with information sharing");
  std::println("");

  std::println("Tests Performed:");
  std::println("  1. Uniformity: Dirichlet-Multinomial model (1000 bins, 100k samples)");
  std::println("     - Prior: Uniform Dirichlet(1,...,1)");
  std::println("     - Metric: Maximum absolute deviation from 1/k");
  std::println("     - Threshold: <0.05 for pass");
  std::println("");
  std::println("  2. Independence: Serial correlation with Fisher z-transform (50k samples)");
  std::println("     - Metric: Correlation coefficient ρ between consecutive values");
  std::println("     - Threshold: |ρ| < 0.1 and 95% CI contains 0");
  std::println("");
  std::println("  3. π Estimation: Monte Carlo integration (1M samples)");
  std::println("     - Method: Points in unit circle / points in unit square");
  std::println("     - Metric: |π_estimated - π_true|");
  std::println("     - Threshold: <0.01 for pass");
  std::println("");
  std::println("  4. Runs Test: Monotone sequence analysis (50k samples)");
  std::println("     - Theory: Expected runs ≈ (2n-1)/3 for random sequences");
  std::println("     - Metric: P(consistent | data) via Bayes factor");
  std::println("     - Threshold: Ratio 0.9-1.1 and P > 0.5");
  std::println("");
  std::println("  5. Gap Test: Geometric distribution of spacings (50k samples)");
  std::println("     - Interval: [0.0, 0.5] for p = 0.5");
  std::println("     - Prior: Beta(1,1) on geometric parameter");
  std::println("     - Threshold: P(geometric | data) > 0.5");
  std::println("");
  std::println("  6. Hierarchical Bit Quality: 64 bits with shared prior (100k samples)");
  std::println("     - Model: Empirical Bayes with Beta-Binomial conjugacy");
  std::println("     - Metric: Overall quality score, problematic bit detection");
  std::println("     - Threshold: Quality score > 0.9, < 5 problematic bits");
  std::println("");

  std::println("Quality Scoring:");
  std::println("  ★★★ EXCELLENT:  All metrics pass with margin (9/9 points)");
  std::println("  ★★  GOOD:       Most metrics pass (6-8 points)");
  std::println("  ★   ACCEPTABLE: Some metrics marginal (4-5 points)");
  std::println("  ✗   POOR:       Multiple failures (<4 points)");
  std::println("");
}

template <typename Generator>
auto testGenerator(const std::string& name, Generator gen, size_t uniformity_samples = 100000,
                   size_t independence_samples = 50000, size_t pi_samples = 1000000,
                   size_t n_bins = 100)
    -> RngTestReport {
  std::println("Testing {}...", name);

  auto start = std::chrono::high_resolution_clock::now();

  auto uniformity = bayesianRngUniformityTest(gen, uniformity_samples, n_bins);
  auto independence = bayesianRngIndependenceTest(gen, independence_samples);
  auto pi_estimation = bayesianRngPiEstimation(gen, pi_samples);
  auto runs = bayesianRngRunsTest(gen, independence_samples);
  auto gap = bayesianRngGapTest(gen, independence_samples, 0.0, 0.5);
  auto bit_quality = bayesianRngHierarchicalBitTest(gen, uniformity_samples);

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  std::println("  → Completed in {:.3f} seconds", elapsed.count());

  return RngTestReport{
      .name = name,
      .uniformity = uniformity,
      .independence = independence,
      .pi_estimation = pi_estimation,
      .runs = runs,
      .gap = gap,
      .bit_quality = bit_quality,
      .elapsed_seconds = elapsed.count(),
  };
}

auto main() -> int {
  printHeader();

  std::vector<RngTestReport> reports;

  // Test various RNG implementations
  printSectionHeader("TESTING GENERATORS (STRESS TEST MODE)");

  // STRESS TEST: Reduced samples + many bins = harder to pass
  // With 1000 bins and only 100k samples, we get ~100 samples per bin
  // This creates high variance and makes it harder to show uniformity
  constexpr size_t uniformity_samples = 100'000;
  constexpr size_t independence_samples = 50'000;
  constexpr size_t pi_samples = 1'000'000;
  constexpr size_t n_bins = 1000;  // 10x more bins than before!

  // ========================================================================
  // Tempura Combined Generator (Recommended)
  // ========================================================================
  reports.push_back(testGenerator(
      "makeRandom() [Combined]",
      makeRandom(42),
      uniformity_samples, independence_samples, pi_samples, n_bins));

  // ========================================================================
  // XorShift Generators - Left Direction (9 presets available, testing 5)
  // ========================================================================
  std::println("\n  Testing XorShift (Left) presets...");
  for (size_t i : {0, 2, 4, 6, 8}) {
    reports.push_back(testGenerator(
        std::format("XorShift-L[{}]", i),
        Generator{42ULL, XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[i]}},
        uniformity_samples, independence_samples, pi_samples, n_bins));
  }

  // ========================================================================
  // XorShift Generators - Right Direction (9 presets available, testing 5)
  // ========================================================================
  std::println("\n  Testing XorShift (Right) presets...");
  for (size_t i : {0, 2, 4, 6, 8}) {
    reports.push_back(testGenerator(
        std::format("XorShift-R[{}]", i),
        Generator{42ULL, XorShift<ShiftDirection::Right>{XorShiftPresets::kAll[i]}},
        uniformity_samples, independence_samples, pi_samples, n_bins));
  }

  // ========================================================================
  // MultiplyWithCarry Generators (9 presets available, testing 5)
  // ========================================================================
  std::println("\n  Testing MultiplyWithCarry presets...");
  for (size_t i : {0, 2, 4, 6, 8}) {
    reports.push_back(testGenerator(
        std::format("MWC[{}]", i),
        Generator{42ULL, MultiplyWithCarry{MultiplyWithCarryPresets::kAll[i]}},
        uniformity_samples, independence_samples, pi_samples, n_bins));
  }

  // ========================================================================
  // LinearCongruential Generators (3 presets available, testing all)
  // ========================================================================
  std::println("\n  Testing LinearCongruential presets...");
  for (size_t i = 0; i < LinearCongruentialPresets::kAll.size(); ++i) {
    reports.push_back(testGenerator(
        std::format("LCG[{}]", i),
        Generator{42ULL, LinearCongruential{LinearCongruentialPresets::kAll[i]}},
        uniformity_samples, independence_samples, pi_samples, n_bins));
  }

  // ========================================================================
  // C++ Standard Library Generators
  // ========================================================================
  std::println("\n  Testing C++ standard library generators...");

  reports.push_back(testGenerator(
      "std::mt19937_64",
      std::mt19937_64{42},
      uniformity_samples, independence_samples, pi_samples, n_bins));

  reports.push_back(testGenerator(
      "std::mt19937",
      std::mt19937{42},
      uniformity_samples, independence_samples, pi_samples, n_bins));

  reports.push_back(testGenerator(
      "std::minstd_rand",
      std::minstd_rand{42},
      uniformity_samples, independence_samples, pi_samples, n_bins));

  reports.push_back(testGenerator(
      "std::minstd_rand0",
      std::minstd_rand0{42},
      uniformity_samples, independence_samples, pi_samples, n_bins));

  reports.push_back(testGenerator(
      "std::ranlux24",
      std::ranlux24{42},
      uniformity_samples, independence_samples, pi_samples, n_bins));

  reports.push_back(testGenerator(
      "std::ranlux48",
      std::ranlux48{42},
      uniformity_samples, independence_samples, pi_samples, n_bins));

  std::println("");
  printSectionHeader("DETAILED RESULTS");

  for (const auto& report : reports) {
    printRngReport(report);
  }

  printComparativeTable(reports);
  printNormalizedTable(reports);
  printMethodology();

  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println(" Report Complete");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("");

  return 0;
}
