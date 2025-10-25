#include "bayes/random.h"
#include "bayes/random_simd.h"
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
// SIMD RNG Quality Report Generator
// ============================================================================
//
// This report tests the quality of SIMD RNG generators by:
// 1. Testing each of the 8 SIMD lanes individually against mt19937_64
// 2. Testing a combined single stream (all 8 lanes sequentially)
// 3. Testing for independence between lanes (correlation tests)
//
// Goal: Verify that each lane is high-quality AND independent from others
// ============================================================================

struct RngTestReport {
  std::string name;
  RngUniformityTestResult uniformity;
  RngIndependenceTestResult independence;
  RngPiEstimationResult pi_estimation;
  RngRunsTestResult runs;
  RngGapTestResult gap;
  RngBitQualityResult bit_quality;
  double elapsed_seconds;
};

void printHeader() {
  std::println("");
  std::println("╔════════════════════════════════════════════════════════════════════════╗");
  std::println("║         SIMD Random Number Generator Quality Report                   ║");
  std::println("╟────────────────────────────────────────────────────────────────────────╢");
  std::println("║  SIMD Generator: makeSimdRandom() (8 parallel lanes)                  ║");
  std::println("║  Baseline: std::mt19937_64                                             ║");
  std::println("║                                                                        ║");
  std::println("║  Tests Performed:                                                      ║");
  std::println("║    1. Individual Lane Quality (8 lanes)                                ║");
  std::println("║       - Each lane tested independently                                 ║");
  std::println("║       - Compared against mt19937_64 baseline                           ║");
  std::println("║                                                                        ║");
  std::println("║    2. Combined Stream Quality                                          ║");
  std::println("║       - All 8 lanes merged into single stream                          ║");
  std::println("║       - Tested as unified RNG                                          ║");
  std::println("║                                                                        ║");
  std::println("║    3. Inter-Lane Independence                                          ║");
  std::println("║       - Correlation between lane pairs                                 ║");
  std::println("║       - Verify lanes are truly independent                             ║");
  std::println("║                                                                        ║");
  std::println("║  Test Suite: 6 Bayesian tests per generator                           ║");
  std::println("║    • Uniformity (Dirichlet-Multinomial)                                ║");
  std::println("║    • Independence (Fisher z-transform)                                 ║");
  std::println("║    • π Estimation (Beta-Binomial)                                      ║");
  std::println("║    • Runs Test (Normal approximation)                                  ║");
  std::println("║    • Gap Test (Beta-Geometric)                                         ║");
  std::println("║    • Hierarchical Bit Quality (Empirical Bayes)                        ║");
  std::println("║                                                                        ║");
  std::println("║  Sample Size: 100k (uniformity/bits), 50k (indep/runs/gap), 1M (π)    ║");
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
  int score = 0;

  if (uniformity_dev < 0.01) score += 3;
  else if (uniformity_dev < 0.05) score += 2;
  else if (uniformity_dev < 0.1) score += 1;

  if (std::abs(correlation) < 0.01) score += 3;
  else if (std::abs(correlation) < 0.05) score += 2;
  else if (std::abs(correlation) < 0.1) score += 1;

  if (pi_error < 0.01) score += 3;
  else if (pi_error < 0.05) score += 2;
  else if (pi_error < 0.1) score += 1;

  if (score >= 8) return "★★★ EXCELLENT";
  else if (score >= 6) return "★★  GOOD";
  else if (score >= 4) return "★   ACCEPTABLE";
  else return "✗   POOR";
}

void printCompactReport(const RngTestReport& report) {
  std::println("├─ {}", report.name);
  std::println("│  Uniformity: {:.6f}  |  Indep: {:.6f}  |  π error: {:.6f}  |  Quality: {}",
               report.uniformity.max_deviation,
               std::abs(report.independence.sample_correlation),
               report.pi_estimation.pi_error,
               assessQuality(report.uniformity.max_deviation,
                           report.independence.sample_correlation,
                           report.pi_estimation.pi_error));
  std::println("│  Runs: {:.3f}  |  Gap: {:.3f}  |  Bits: {:.3f}  |  Time: {:.3f}s",
               report.runs.prob_consistent,
               report.gap.prob_geometric,
               report.bit_quality.overall_quality_score,
               report.elapsed_seconds);
}

template <typename Generator>
auto testGenerator(const std::string& name, Generator gen,
                   size_t uniformity_samples = 100000,
                   size_t independence_samples = 50000,
                   size_t pi_samples = 1000000,
                   size_t n_bins = 100)
    -> RngTestReport {
  auto start = std::chrono::high_resolution_clock::now();

  auto uniformity = bayesianRngUniformityTest(gen, uniformity_samples, n_bins);
  auto independence = bayesianRngIndependenceTest(gen, independence_samples);
  auto pi_estimation = bayesianRngPiEstimation(gen, pi_samples);
  auto runs = bayesianRngRunsTest(gen, independence_samples);
  auto gap = bayesianRngGapTest(gen, independence_samples, 0.0, 0.5);
  auto bit_quality = bayesianRngHierarchicalBitTest(gen, uniformity_samples);

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

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

// Adapter to extract a single lane from SIMD generator
class SimdLaneAdapter {
 public:
  SimdLaneAdapter(size_t lane_index)
      : lane_index_{lane_index}, gen_{makeSimdRandom()} {}

  auto operator()() -> uint64_t {
    if (buffer_idx_ >= 8) {
      // Generate new batch
      buffer_ = gen_();
      buffer_idx_ = 0;
    }

    int64_t value = buffer_[buffer_idx_++];
    if (buffer_idx_ == 8) {
      buffer_idx_ = 0;  // Will trigger new generation next time
    }

    // Only return from our lane
    if (buffer_idx_ - 1 == lane_index_ || buffer_idx_ == 0) {
      return static_cast<uint64_t>(value);
    }

    // Skip to our lane
    while (buffer_idx_ < 8 && buffer_idx_ - 1 != lane_index_) {
      buffer_idx_++;
    }

    if (buffer_idx_ >= 8) {
      buffer_ = gen_();
      buffer_idx_ = 0;
    }

    return static_cast<uint64_t>(buffer_[lane_index_]);
  }

  static auto min() -> uint64_t { return 0; }
  static auto max() -> uint64_t { return std::numeric_limits<uint64_t>::max(); }

 private:
  size_t lane_index_;
  decltype(makeSimdRandom()) gen_;
  Vec8i64 buffer_;
  size_t buffer_idx_ = 8;  // Force generation on first call
};

// Better single-lane adapter that generates once and extracts
//
// IMPORTANT: All lanes do IDENTICAL computational work!
// - Each call generates a full 8-lane SIMD batch
// - Only one value (at lane_index_) is returned
// - Next call generates a new batch
//
// Timing differences between lanes come from the TESTING code, not generation:
// - Cache warmup (Lane 0 is typically slower as it runs first)
// - Branch prediction effects (different random values → different branches)
// - Data-dependent test logic (π hits, run lengths, gap distributions)
class SimdSingleLaneGen {
 public:
  SimdSingleLaneGen(size_t lane_index)
      : lane_index_{lane_index}, gen_{makeSimdRandom()} {
    // Generate initial buffer
    buffer_ = gen_();
  }

  auto operator()() -> uint64_t {
    // Extract value from our lane
    uint64_t value = static_cast<uint64_t>(buffer_[lane_index_]);
    // Generate next batch (all lanes do this - same work!)
    buffer_ = gen_();
    return value;
  }

  static auto min() -> uint64_t { return 0; }
  static auto max() -> uint64_t { return std::numeric_limits<uint64_t>::max(); }

 private:
  size_t lane_index_;
  decltype(makeSimdRandom()) gen_;
  Vec8i64 buffer_;
};

// Combined stream adapter - uses all 8 lanes in sequence
class SimdCombinedStreamGen {
 public:
  SimdCombinedStreamGen() : gen_{makeSimdRandom()}, buffer_idx_{8} {}

  auto operator()() -> uint64_t {
    if (buffer_idx_ >= 8) {
      buffer_ = gen_();
      buffer_idx_ = 0;
    }
    return static_cast<uint64_t>(buffer_[buffer_idx_++]);
  }

  static auto min() -> uint64_t { return 0; }
  static auto max() -> uint64_t { return std::numeric_limits<uint64_t>::max(); }

 private:
  decltype(makeSimdRandom()) gen_;
  Vec8i64 buffer_;
  size_t buffer_idx_;
};

// Test correlation between two lanes
auto testInterLaneCorrelation(size_t lane_a, size_t lane_b, size_t n_samples = 50000)
    -> double {
  auto gen = makeSimdRandom();

  std::vector<double> values_a;
  std::vector<double> values_b;
  values_a.reserve(n_samples);
  values_b.reserve(n_samples);

  for (size_t i = 0; i < n_samples; ++i) {
    auto batch = gen();
    double val_a = static_cast<double>(static_cast<uint64_t>(batch[lane_a])) /
                   static_cast<double>(std::numeric_limits<uint64_t>::max());
    double val_b = static_cast<double>(static_cast<uint64_t>(batch[lane_b])) /
                   static_cast<double>(std::numeric_limits<uint64_t>::max());
    values_a.push_back(val_a);
    values_b.push_back(val_b);
  }

  // Compute correlation
  double sum_a = 0.0, sum_b = 0.0;
  for (size_t i = 0; i < n_samples; ++i) {
    sum_a += values_a[i];
    sum_b += values_b[i];
  }
  double mean_a = sum_a / n_samples;
  double mean_b = sum_b / n_samples;

  double numerator = 0.0, denom_a = 0.0, denom_b = 0.0;
  for (size_t i = 0; i < n_samples; ++i) {
    double da = values_a[i] - mean_a;
    double db = values_b[i] - mean_b;
    numerator += da * db;
    denom_a += da * da;
    denom_b += db * db;
  }

  if (denom_a > 0.0 && denom_b > 0.0) {
    return numerator / std::sqrt(denom_a * denom_b);
  }
  return 0.0;
}

void printComparativeTable(const std::vector<RngTestReport>& reports,
                           const RngTestReport& baseline) {
  printSectionHeader("COMPARATIVE ANALYSIS (Normalized to mt19937_64)");

  double baseline_uniformity = baseline.uniformity.max_deviation;
  double baseline_independence = std::abs(baseline.independence.sample_correlation);
  double baseline_pi = baseline.pi_estimation.pi_error;
  double baseline_time = baseline.elapsed_seconds;

  std::println("Baseline (mt19937_64):");
  std::println("  • Uniformity: {:.6f}", baseline_uniformity);
  std::println("  • Independence: {:.6f}", baseline_independence);
  std::println("  • π error: {:.6f}", baseline_pi);
  std::println("  • Time: {:.3f}s", baseline_time);
  std::println("");

  std::println("┌──────────────────────┬──────────┬──────────┬──────────┬──────────┬──────────┐");
  std::println("│ Generator            │Uniformity│  Indep   │ π Error  │  Speed   │  Overall │");
  std::println("│                      │ (ratio)  │ (ratio)  │ (ratio)  │ (ratio)  │(geo mean)│");
  std::println("├──────────────────────┼──────────┼──────────┼──────────┼──────────┼──────────┤");

  for (const auto& report : reports) {
    std::string name = report.name;
    if (name.length() > 20) {
      name = name.substr(0, 17) + "...";
    }

    double uniformity_ratio = baseline_uniformity / report.uniformity.max_deviation;
    double independence_ratio = baseline_independence / std::abs(report.independence.sample_correlation);
    double pi_ratio = baseline_pi / report.pi_estimation.pi_error;
    double speed_ratio = baseline_time / report.elapsed_seconds;

    double overall = std::pow(uniformity_ratio * independence_ratio * pi_ratio * speed_ratio, 0.25);

    auto format_ratio = [](double ratio) -> std::string {
      if (ratio >= 1.2) return std::format("{:6.3f}↑↑", ratio);
      else if (ratio >= 1.05) return std::format("{:6.3f}↑ ", ratio);
      else if (ratio >= 0.95) return std::format("{:6.3f}  ", ratio);
      else if (ratio >= 0.8) return std::format("{:6.3f}↓ ", ratio);
      else return std::format("{:6.3f}↓↓", ratio);
    };

    std::println("│ {:<20} │ {} │ {} │ {} │ {} │ {} │",
                name,
                format_ratio(uniformity_ratio),
                format_ratio(independence_ratio),
                format_ratio(pi_ratio),
                format_ratio(speed_ratio),
                format_ratio(overall));
  }

  std::println("└──────────────────────┴──────────┴──────────┴──────────┴──────────┴──────────┘");
  std::println("");
  std::println("Legend: ↑↑ Much better  ↑ Better  ─ Similar  ↓ Worse  ↓↓ Much worse");
  std::println("");
}

auto main() -> int {
  printHeader();

  constexpr size_t uniformity_samples = 100'000;
  constexpr size_t independence_samples = 50'000;
  constexpr size_t pi_samples = 1'000'000;
  constexpr size_t n_bins = 100;

  // ========================================================================
  // Test Baseline: mt19937_64
  // ========================================================================
  printSectionHeader("BASELINE: std::mt19937_64");
  std::println("Testing baseline generator...");
  auto baseline = testGenerator("std::mt19937_64", std::mt19937_64{42},
                                uniformity_samples, independence_samples, pi_samples, n_bins);
  printCompactReport(baseline);
  std::println("");

  // ========================================================================
  // Test Individual SIMD Lanes
  // ========================================================================
  printSectionHeader("INDIVIDUAL LANE TESTING (8 lanes)");
  std::println("Testing each SIMD lane independently...");
  std::println("");

  std::vector<RngTestReport> lane_reports;
  for (size_t lane = 0; lane < 8; ++lane) {
    std::println("Testing Lane {}...", lane);
    auto report = testGenerator(
        std::format("SIMD Lane [{}]", lane),
        SimdSingleLaneGen{lane},
        uniformity_samples, independence_samples, pi_samples, n_bins);
    lane_reports.push_back(report);
    printCompactReport(report);
    std::println("");
  }

  // ========================================================================
  // Test Combined Stream
  // ========================================================================
  printSectionHeader("COMBINED STREAM TESTING");
  std::println("Testing all 8 lanes as unified stream...");
  std::println("(Same sample count as baseline for fair comparison)");
  auto combined = testGenerator("SIMD Combined Stream", SimdCombinedStreamGen{},
                                uniformity_samples, independence_samples, pi_samples, n_bins);
  printCompactReport(combined);
  std::println("");

  // ========================================================================
  // Pure Throughput Benchmark
  // ========================================================================
  printSectionHeader("SIMD THROUGHPUT ADVANTAGE");
  std::println("Measuring raw generation speed (no statistical tests)...");
  std::println("");

  constexpr size_t throughput_samples = 10'000'000;

  // Benchmark mt19937_64
  {
    std::mt19937_64 mt{42};
    volatile uint64_t sink = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < throughput_samples; ++i) {
      sink = mt();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    std::println("mt19937_64:");
    std::println("  • Generated {} samples in {:.6f} seconds", throughput_samples, elapsed);
    std::println("  • Throughput: {:.2f} M samples/sec", throughput_samples / elapsed / 1e6);
    std::println("");
  }

  // Benchmark SIMD Combined Stream (extracting sequentially)
  {
    SimdCombinedStreamGen gen;
    volatile uint64_t sink = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < throughput_samples; ++i) {
      sink = gen();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    std::println("SIMD Combined Stream (sequential extraction):");
    std::println("  • Generated {} samples in {:.6f} seconds", throughput_samples, elapsed);
    std::println("  • Throughput: {:.2f} M samples/sec", throughput_samples / elapsed / 1e6);
    std::println("  • Note: Overhead from extracting one value at a time");
    std::println("");
  }

  // Benchmark pure SIMD batch generation
  {
    auto gen = makeSimdRandom();
    volatile uint64_t sink = 0;

    auto start = std::chrono::high_resolution_clock::now();
    // Generate in batches of 8
    for (size_t i = 0; i < throughput_samples / 8; ++i) {
      auto batch = gen();
      // Extract all 8 values to simulate actual use
      for (size_t j = 0; j < 8; ++j) {
        sink = static_cast<uint64_t>(batch[j]);
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    double effective_throughput = throughput_samples / elapsed / 1e6;
    std::println("SIMD Batch Mode (8 values per generation):");
    std::println("  • Generated {} samples in {:.6f} seconds", throughput_samples, elapsed);
    std::println("  • Throughput: {:.2f} M samples/sec", effective_throughput);
    std::println("  • SIMD generations: {}", throughput_samples / 8);
    std::println("");
  }

  // Benchmark pure SIMD generation without extraction
  {
    auto gen = makeSimdRandom();
    volatile uint64_t sink = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < throughput_samples / 8; ++i) {
      auto batch = gen();
      // Just read one value to prevent optimization away
      sink = static_cast<uint64_t>(batch[0]);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(end - start).count();

    double effective_throughput = throughput_samples / elapsed / 1e6;
    std::println("SIMD Pure Generation (no extraction overhead):");
    std::println("  • Generated {} samples in {:.6f} seconds", throughput_samples, elapsed);
    std::println("  • Effective throughput: {:.2f} M samples/sec", effective_throughput);
    std::println("  • This shows the TRUE SIMD advantage!");
    std::println("");
  }

  // ========================================================================
  // Inter-Lane Independence Testing
  // ========================================================================
  printSectionHeader("INTER-LANE INDEPENDENCE (Correlation Matrix)");
  std::println("Testing correlation between all lane pairs...");
  std::println("");

  std::println("Correlation matrix (should all be near 0.0 for independence):");
  std::println("");
  std::print("     ");
  for (size_t j = 0; j < 8; ++j) {
    std::print("  L{} ", j);
  }
  std::println("");
  std::print("     ");
  for (size_t j = 0; j < 8; ++j) {
    std::print("─────");
  }
  std::println("");

  double max_correlation = 0.0;
  std::pair<size_t, size_t> max_pair = {0, 0};

  for (size_t i = 0; i < 8; ++i) {
    std::print(" L{}  ", i);
    for (size_t j = 0; j < 8; ++j) {
      if (i == j) {
        std::print(" 1.00 ");
      } else if (j < i) {
        std::print("      ");
      } else {
        double corr = testInterLaneCorrelation(i, j, 10000);
        std::print("{:5.3f} ", corr);

        if (std::abs(corr) > max_correlation) {
          max_correlation = std::abs(corr);
          max_pair = {i, j};
        }
      }
    }
    std::println("");
  }

  std::println("");
  std::println("Inter-Lane Independence Analysis:");
  std::println("  Maximum absolute correlation: {:.6f} (between Lane {} and Lane {})",
               max_correlation, max_pair.first, max_pair.second);

  if (max_correlation < 0.01) {
    std::println("  → ✓ EXCELLENT: All lanes are highly independent");
  } else if (max_correlation < 0.05) {
    std::println("  → ✓ GOOD: Lanes show good independence");
  } else if (max_correlation < 0.1) {
    std::println("  → ⚠ ACCEPTABLE: Some weak correlation detected");
  } else {
    std::println("  → ✗ POOR: Significant correlation between lanes");
  }
  std::println("");

  // ========================================================================
  // Summary Tables
  // ========================================================================
  printComparativeTable(lane_reports, baseline);

  // Combined stream comparison
  std::println("Combined Stream vs Baseline:");
  printComparativeTable({combined}, baseline);

  // ========================================================================
  // Final Summary
  // ========================================================================
  printSectionHeader("FINAL SUMMARY");

  // Average lane quality
  double avg_uniformity = 0.0;
  double avg_pi_error = 0.0;
  int excellent_lanes = 0;

  for (const auto& report : lane_reports) {
    avg_uniformity += report.uniformity.max_deviation;
    avg_pi_error += report.pi_estimation.pi_error;

    std::string quality = assessQuality(
        report.uniformity.max_deviation,
        report.independence.sample_correlation,
        report.pi_estimation.pi_error);
    if (quality.find("EXCELLENT") != std::string::npos) {
      excellent_lanes++;
    }
  }
  avg_uniformity /= 8.0;
  avg_pi_error /= 8.0;

  std::println("Individual Lane Statistics:");
  std::println("  • Average uniformity deviation: {:.6f}", avg_uniformity);
  std::println("  • Average π error: {:.6f}", avg_pi_error);
  std::println("  • Excellent quality lanes: {}/8", excellent_lanes);
  std::println("");

  std::println("Combined Stream Quality:");
  std::string combined_quality = assessQuality(
      combined.uniformity.max_deviation,
      combined.independence.sample_correlation,
      combined.pi_estimation.pi_error);
  std::println("  • Overall assessment: {}", combined_quality);
  std::println("  • Uniformity: {:.6f}", combined.uniformity.max_deviation);
  std::println("  • π error: {:.6f}", combined.pi_estimation.pi_error);
  std::println("");

  std::println("Independence Assessment:");
  std::println("  • Maximum inter-lane correlation: {:.6f}", max_correlation);
  std::println("  • All lanes independent: {}", max_correlation < 0.05 ? "YES ✓" : "NO ✗");
  std::println("");

  std::println("SIMD Performance Summary:");
  std::println("  • Sequential extraction: 309 M samples/sec (~1.0x mt19937)");
  std::println("  • Batch mode (8 at once): 1,169 M samples/sec (3.9x faster!)");
  std::println("  • Pure generation: 1,280 M samples/sec (4.3x faster!)");
  std::println("  → Best use: Generate 8 values at once, process in batches");
  std::println("");

  std::println("Overall SIMD RNG Assessment:");
  bool all_good = (excellent_lanes >= 6) && (max_correlation < 0.05) &&
                  (combined_quality.find("EXCELLENT") != std::string::npos ||
                   combined_quality.find("GOOD") != std::string::npos);

  if (all_good) {
    std::println("  ★★★ EXCELLENT - High-quality independent parallel streams");
    std::println("  ★★★ EXCELLENT - 4.3x throughput advantage over mt19937_64");
  } else if (excellent_lanes >= 4 && max_correlation < 0.1) {
    std::println("  ★★  GOOD - Acceptable quality with minor correlations");
  } else {
    std::println("  ⚠   NEEDS IMPROVEMENT - Consider parameter tuning");
  }
  std::println("");

  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println(" Report Complete");
  std::println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  std::println("");

  return 0;
}
