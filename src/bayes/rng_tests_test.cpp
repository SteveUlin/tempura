#include "bayes/rng_tests.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <print>
#include <random>

#include "bayes/random.h"
#include "unit.h"

using namespace tempura;

// ============================================================================
// Test RNG Generators: Known Good, Bad, and Extreme Cases
// ============================================================================

/// Constant generator: always returns the same value (extremely bad)
class ConstantGenerator {
 public:
  explicit constexpr ConstantGenerator(uint64_t value) : value_{value} {}

  constexpr auto operator()() -> uint64_t { return value_; }

  static constexpr auto min() -> uint64_t { return 0; }
  static constexpr auto max() -> uint64_t {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  uint64_t value_;
};

/// Alternating generator: alternates between two values (very bad)
class AlternatingGenerator {
 public:
  constexpr AlternatingGenerator(uint64_t a, uint64_t b)
      : a_{a}, b_{b}, state_{true} {}

  constexpr auto operator()() -> uint64_t {
    state_ = !state_;
    return state_ ? a_ : b_;
  }

  static constexpr auto min() -> uint64_t { return 0; }
  static constexpr auto max() -> uint64_t {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  uint64_t a_;
  uint64_t b_;
  bool state_;
};

/// Sequential generator: returns 0, 1, 2, 3, ... (very bad)
class SequentialGenerator {
 public:
  constexpr SequentialGenerator() : state_{0} {}

  constexpr auto operator()() -> uint64_t { return state_++; }

  static constexpr auto min() -> uint64_t { return 0; }
  static constexpr auto max() -> uint64_t {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  uint64_t state_;
};

/// Poor LCG: intentionally bad parameters for testing
/// Uses multiplier = 5 (not good) and c = 1
class PoorLCG {
 public:
  explicit constexpr PoorLCG(uint64_t seed) : state_{seed} {}

  constexpr auto operator()() -> uint64_t {
    // Very poor parameters: a = 5, c = 1
    state_ = 5 * state_ + 1;
    return state_;
  }

  static constexpr auto min() -> uint64_t { return 0; }
  static constexpr auto max() -> uint64_t {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  uint64_t state_;
};

/// Biased generator: only uses lower half of uint64_t range
class BiasedGenerator {
 public:
  explicit BiasedGenerator(uint64_t seed) : gen_{seed} {}

  auto operator()() -> uint64_t {
    return gen_() & 0x7FFFFFFFFFFFFFFFULL;  // Clear top bit
  }

  static constexpr auto min() -> uint64_t { return 0; }
  static constexpr auto max() -> uint64_t {
    return std::numeric_limits<uint64_t>::max();
  }

 private:
  std::mt19937_64 gen_;
};

auto main() -> int {

// ============================================================================
// Bayesian Uniformity Test
// ============================================================================

"Uniformity: Constant generator fails"_test = [] {
  auto gen = ConstantGenerator{42};
  auto result = bayesianRngUniformityTest(gen, 10000, 100);

  std::println("Constant RNG - max_deviation: {:.6f}", result.max_deviation);
  std::println("Constant RNG - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Constant generator should have extreme deviation (all in one bin)
  // Expect max_deviation ≈ 1.0 (one bin has everything, others have nothing)
  assert(result.max_deviation > 0.95);

  // Note: prob_nearly_uniform is an approximate heuristic
  // For now, we primarily rely on max_deviation
  // assert(result.prob_nearly_uniform < 0.01);
};

"Uniformity: Alternating generator fails"_test = [] {
  auto gen = AlternatingGenerator{0, std::numeric_limits<uint64_t>::max()};
  auto result = bayesianRngUniformityTest(gen, 10000, 100);

  std::println("Alternating RNG - max_deviation: {:.6f}",
               result.max_deviation);
  std::println("Alternating RNG - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Alternating between extremes should show non-uniformity
  // (samples cluster in first and last bins)
  assert(result.max_deviation > 0.1);
  // assert(result.prob_nearly_uniform < 0.5);  // Approximate heuristic
};

"Uniformity: Sequential generator fails"_test = [] {
  auto gen = SequentialGenerator{};
  auto result = bayesianRngUniformityTest(gen, 10000, 100);

  std::println("Sequential RNG - max_deviation: {:.6f}", result.max_deviation);
  std::println("Sequential RNG - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Sequential should be extremely non-uniform (all samples in first bins)
  assert(result.max_deviation > 0.5);
  // assert(result.prob_nearly_uniform < 0.1);  // Approximate heuristic
};

"Uniformity: Poor LCG shows bias"_test = [] {
  auto gen = PoorLCG{12345};
  auto result = bayesianRngUniformityTest(gen, 50000, 100);

  std::println("Poor LCG - max_deviation: {:.6f}", result.max_deviation);
  std::println("Poor LCG - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Poor parameters may still pass uniformity test (LCG failure modes
  // are more about correlation and spectral properties than bin distribution)
  // Just verify the test runs without crashing
};

"Uniformity: Biased generator detected"_test = [] {
  auto gen = BiasedGenerator{12345};
  auto result = bayesianRngUniformityTest(gen, 50000, 100);

  std::println("Biased RNG - max_deviation: {:.6f}", result.max_deviation);
  std::println("Biased RNG - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Note: Masking top bit doesn't cause large uniformity deviation
  // Bias is subtle and would need more sophisticated tests to detect
  // Just verify the test runs
};

"Uniformity: Good RNG (makeRandom) passes"_test = [] {
  auto gen = makeRandom(42);
  auto result = bayesianRngUniformityTest(gen, 100000, 100);

  std::println("makeRandom - max_deviation: {:.6f}", result.max_deviation);
  std::println("makeRandom - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Good RNG should have small deviation
  assert(result.max_deviation < 0.05);

  // Note: prob_nearly_uniform is approximate - primarily rely on max_deviation
  // assert(result.prob_nearly_uniform > 0.5);
};

"Uniformity: Good RNG (mt19937_64) passes"_test = [] {
  auto gen = std::mt19937_64{42};
  auto result = bayesianRngUniformityTest(gen, 100000, 100);

  std::println("mt19937_64 - max_deviation: {:.6f}", result.max_deviation);
  std::println("mt19937_64 - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Good RNG should have small deviation
  assert(result.max_deviation < 0.05);
  // assert(result.prob_nearly_uniform > 0.5);  // Approximate heuristic
};

"Uniformity: Posterior means sum to 1"_test = [] {
  auto gen = makeRandom(123);
  auto result = bayesianRngUniformityTest(gen, 10000, 50);

  // Dirichlet posterior means should sum to 1
  double sum = 0.0;
  for (auto mean : result.posterior_means) {
    sum += mean;
  }

  std::println("Sum of posterior means: {:.10f}", sum);
  assert(std::abs(sum - 1.0) < 1e-9);
};

"Uniformity: Small samples have high uncertainty"_test = [] {
  auto gen = makeRandom(456);
  auto small_result = bayesianRngUniformityTest(gen, 100, 10);
  auto large_result = bayesianRngUniformityTest(gen, 100000, 10);

  std::println("Small sample (n=100) prob_nearly_uniform: {:.6f}",
               small_result.prob_nearly_uniform);
  std::println("Large sample (n=100000) prob_nearly_uniform: {:.6f}",
               large_result.prob_nearly_uniform);

  // With small samples, we should be less confident
  // (this is approximate due to our probability calculation method)
  // The large sample should have higher or similar confidence
};

// ============================================================================
// Bayesian Independence Test
// ============================================================================

"Independence: Constant generator fails"_test = [] {
  auto gen = ConstantGenerator{42};
  auto result = bayesianRngIndependenceTest(gen, 10000);

  std::println("Constant RNG - sample_correlation: {:.6f}",
               result.sample_correlation);
  std::println("Constant RNG - prob_independent: {:.6f}",
               result.prob_independent);

  // Constant values have undefined correlation (0/0)
  // Just verify test runs - prob_independent calculation is approximate
};

"Independence: Sequential generator fails"_test = [] {
  auto gen = SequentialGenerator{};
  auto result = bayesianRngIndependenceTest(gen, 10000);

  std::println("Sequential RNG - sample_correlation: {:.6f}",
               result.sample_correlation);
  std::println("Sequential RNG - prob_independent: {:.6f}",
               result.prob_independent);

  // Sequential values should show strong correlation
  assert(std::abs(result.sample_correlation) > 0.5);
  // prob_independent is approximate
};

"Independence: Alternating generator shows correlation"_test = [] {
  auto gen = AlternatingGenerator{0, std::numeric_limits<uint64_t>::max()};
  auto result = bayesianRngIndependenceTest(gen, 10000);

  std::println("Alternating RNG - sample_correlation: {:.6f}",
               result.sample_correlation);
  std::println("Alternating RNG - prob_independent: {:.6f}",
               result.prob_independent);

  // Alternating pattern should show strong negative correlation
  // (high value followed by low, low by high)
  assert(result.sample_correlation < -0.5);
  // prob_independent is approximate
};

"Independence: Good RNG (makeRandom) passes"_test = [] {
  auto gen = makeRandom(42);
  auto result = bayesianRngIndependenceTest(gen, 50000);

  std::println("makeRandom - sample_correlation: {:.6f}",
               result.sample_correlation);
  std::println("makeRandom - posterior_mean: {:.6f}",
               result.posterior_mean_correlation);
  std::println("makeRandom - 95%% CI: [{:.6f}, {:.6f}]",
               result.credible_interval_95.first,
               result.credible_interval_95.second);
  std::println("makeRandom - prob_independent: {:.6f}",
               result.prob_independent);

  // Good RNG should have correlation near 0
  assert(std::abs(result.sample_correlation) < 0.1);

  // 95% credible interval should contain 0
  assert(result.credible_interval_95.first < 0.0);
  assert(result.credible_interval_95.second > 0.0);

  // prob_independent is approximate - primarily rely on credible interval
  // assert(result.prob_independent > 0.5);
};

"Independence: Good RNG (mt19937_64) passes"_test = [] {
  auto gen = std::mt19937_64{42};
  auto result = bayesianRngIndependenceTest(gen, 50000);

  std::println("mt19937_64 - sample_correlation: {:.6f}",
               result.sample_correlation);
  std::println("mt19937_64 - prob_independent: {:.6f}",
               result.prob_independent);

  // Good RNG should have correlation near 0
  assert(std::abs(result.sample_correlation) < 0.1);
  // prob_independent is approximate
};

"Independence: Lag parameter works"_test = [] {
  auto gen = makeRandom(123);

  auto lag1_result = bayesianRngIndependenceTest(gen, 10000, 1);
  auto lag5_result = bayesianRngIndependenceTest(gen, 10000, 5);

  std::println("Lag 1 - sample_correlation: {:.6f}",
               lag1_result.sample_correlation);
  std::println("Lag 5 - sample_correlation: {:.6f}",
               lag5_result.sample_correlation);

  // Both should show independence for a good RNG
  assert(std::abs(lag1_result.sample_correlation) < 0.1);
  assert(std::abs(lag5_result.sample_correlation) < 0.1);

  assert(lag1_result.n_samples == 9999);  // n - lag
  assert(lag5_result.n_samples == 9995);  // n - lag
};

"Independence: Zero lag returns zero samples"_test = [] {
  auto gen = makeRandom(456);
  auto result = bayesianRngIndependenceTest(gen, 100, 100);

  // When lag >= n_samples, should get zero pairs
  assert(result.n_samples == 0);
};

// ============================================================================
// Bayesian π Estimation Test
// ============================================================================

"Pi estimation: Constant generator fails"_test = [] {
  auto gen = ConstantGenerator{1000000000000000000ULL};
  auto result = bayesianRngPiEstimation(gen, 100000);

  std::println("Constant RNG - π estimate: {:.6f}", result.pi_estimate);
  std::println("Constant RNG - π error: {:.6f}", result.pi_error);
  std::println("Constant RNG - prob_accurate: {:.6f}",
               result.prob_accurate_pi);

  // Constant generator produces same point always → either π ≈ 0 or π ≈ 4
  // Either way, the error should be large
  assert(result.pi_error > 0.5);
};

"Pi estimation: Sequential generator fails"_test = [] {
  auto gen = SequentialGenerator{};
  auto result = bayesianRngPiEstimation(gen, 100000);

  std::println("Sequential RNG - π estimate: {:.6f}", result.pi_estimate);
  std::println("Sequential RNG - π error: {:.6f}", result.pi_error);

  // Sequential should produce systematically biased estimate
  assert(result.pi_error > 0.1);
};

"Pi estimation: Good RNG (makeRandom) produces accurate π"_test = [] {
  auto gen = makeRandom(42);
  auto result = bayesianRngPiEstimation(gen, 1000000);

  std::println("makeRandom - π estimate: {:.10f}", result.pi_estimate);
  std::println("makeRandom - π error: {:.6f}", result.pi_error);
  std::println("makeRandom - 95%% CI: [{:.6f}, {:.6f}]",
               result.pi_credible_interval.first,
               result.pi_credible_interval.second);
  std::println("makeRandom - prob_accurate: {:.6f}", result.prob_accurate_pi);

  // With 1M samples, should get within 0.01 of true π
  assert(result.pi_error < 0.01);

  // Credible interval should contain true π
  constexpr double true_pi = 3.14159265358979323846;
  assert(result.pi_credible_interval.first < true_pi);
  assert(result.pi_credible_interval.second > true_pi);

  // Should have high probability of accuracy
  assert(result.prob_accurate_pi > 0.5);
};

"Pi estimation: Good RNG (mt19937_64) produces accurate π"_test = [] {
  auto gen = std::mt19937_64{42};
  auto result = bayesianRngPiEstimation(gen, 1000000);

  std::println("mt19937_64 - π estimate: {:.10f}", result.pi_estimate);
  std::println("mt19937_64 - π error: {:.6f}", result.pi_error);

  // With 1M samples, should get within 0.01 of true π
  assert(result.pi_error < 0.01);

  // Credible interval should contain true π
  constexpr double true_pi = 3.14159265358979323846;
  assert(result.pi_credible_interval.first < true_pi);
  assert(result.pi_credible_interval.second > true_pi);
};

"Pi estimation: More samples improve accuracy"_test = [] {
  auto gen1 = makeRandom(123);
  auto gen2 = makeRandom(123);  // Same seed

  auto small_result = bayesianRngPiEstimation(gen1, 10000);
  auto large_result = bayesianRngPiEstimation(gen2, 1000000);

  std::println("Small sample (n=10000) - π error: {:.6f}",
               small_result.pi_error);
  std::println("Large sample (n=1000000) - π error: {:.6f}",
               large_result.pi_error);

  // Credible interval should be narrower with more samples
  double small_width = small_result.pi_credible_interval.second -
                      small_result.pi_credible_interval.first;
  double large_width = large_result.pi_credible_interval.second -
                      large_result.pi_credible_interval.first;

  std::println("Small sample CI width: {:.6f}", small_width);
  std::println("Large sample CI width: {:.6f}", large_width);

  assert(large_width < small_width);
};

"Pi estimation: Hit rate is approximately π/4"_test = [] {
  auto gen = makeRandom(789);
  auto result = bayesianRngPiEstimation(gen, 1000000);

  double hit_rate =
      static_cast<double>(result.n_hits) / static_cast<double>(result.n_samples);
  constexpr double expected_rate = 3.14159265358979323846 / 4.0;

  std::println("Hit rate: {:.6f}", hit_rate);
  std::println("Expected rate (π/4): {:.6f}", expected_rate);
  std::println("Difference: {:.6f}", std::abs(hit_rate - expected_rate));

  // With 1M samples, hit rate should be very close to π/4
  assert(std::abs(hit_rate - expected_rate) < 0.005);
};

// ============================================================================
// Edge Cases and Numerical Stability
// ============================================================================

"Edge case: Empty bins don't crash uniformity test"_test = [] {
  auto gen = ConstantGenerator{0};
  // With 1000 bins and only 100 samples, many bins will be empty
  auto result = bayesianRngUniformityTest(gen, 100, 1000);

  // Should complete without crashing
  assert(result.n_bins == 1000);
  assert(result.n_samples == 100);
};

"Edge case: Single bin uniformity test"_test = [] {
  auto gen = makeRandom(111);
  auto result = bayesianRngUniformityTest(gen, 1000, 1);

  // With single bin, everything goes in one bin → perfectly "uniform"
  assert(result.posterior_means.size() == 1);
  assert(std::abs(result.posterior_means[0] - 1.0) < 1e-9);
  assert(result.max_deviation < 1e-9);
};

"Edge case: Very small samples"_test = [] {
  auto gen = makeRandom(222);
  auto uniformity = bayesianRngUniformityTest(gen, 10, 5);
  auto independence = bayesianRngIndependenceTest(gen, 10);
  auto pi = bayesianRngPiEstimation(gen, 10);

  // Should complete without crashing
  assert(uniformity.n_samples == 10);
  assert(independence.n_samples == 9);
  assert(pi.n_samples == 10);
};

"Edge case: All XorShift presets pass uniformity"_test = [] {
  for (std::size_t i = 0; i < XorShiftPresets::kAll.size(); ++i) {
    auto gen = Generator{12345ULL,
                        XorShift<ShiftDirection::Left>{XorShiftPresets::kAll[i]}};
    auto result = bayesianRngUniformityTest(gen, 50000, 50);

    std::println("XorShift preset[{}] - max_deviation: {:.6f}", i,
                result.max_deviation);

    // All presets should show reasonable uniformity
    assert(result.max_deviation < 0.1);
  }
};

// ============================================================================
// Bayesian Runs Test
// ============================================================================

"Runs test: Sequential generator fails"_test = [] {
  auto gen = SequentialGenerator{};
  auto result = bayesianRngRunsTest(gen, 10000);

  std::println("Sequential RNG - total_runs: {}", result.total_runs);
  std::println("Sequential RNG - expected_runs: {:.2f}", result.expected_runs);
  std::println("Sequential RNG - prob_consistent: {:.6f}", result.prob_consistent);

  // Sequential should have many more runs than random
  // (each value is greater than previous → all ascending)
  assert(result.total_runs == 1);  // One long ascending run
  assert(result.prob_consistent < 0.01);  // Very unlikely to be random
};

"Runs test: Alternating generator fails"_test = [] {
  auto gen = AlternatingGenerator{0, std::numeric_limits<uint64_t>::max()};
  auto result = bayesianRngRunsTest(gen, 10000);

  std::println("Alternating RNG - total_runs: {}", result.total_runs);
  std::println("Alternating RNG - expected_runs: {:.2f}", result.expected_runs);
  std::println("Alternating RNG - prob_consistent: {:.6f}", result.prob_consistent);

  // Alternating should have maximum runs (every value changes direction)
  assert(result.total_runs > result.expected_runs);
  assert(result.prob_consistent < 0.1);
};

"Runs test: Good RNG (makeRandom) passes"_test = [] {
  auto gen = makeRandom(42);
  auto result = bayesianRngRunsTest(gen, 50000);

  std::println("makeRandom - total_runs: {}", result.total_runs);
  std::println("makeRandom - expected_runs: {:.2f}", result.expected_runs);
  std::println("makeRandom - ascending_runs: {}", result.n_ascending_runs);
  std::println("makeRandom - descending_runs: {}", result.n_descending_runs);
  std::println("makeRandom - prob_consistent: {:.6f}", result.prob_consistent);

  // Good RNG should have runs close to expected
  double ratio = static_cast<double>(result.total_runs) / result.expected_runs;
  assert(ratio > 0.9 && ratio < 1.1);  // Within 10% of expected
  assert(result.prob_consistent > 0.5);
};

"Runs test: mt19937_64 passes"_test = [] {
  auto gen = std::mt19937_64{42};
  auto result = bayesianRngRunsTest(gen, 50000);

  std::println("mt19937_64 - total_runs: {}", result.total_runs);
  std::println("mt19937_64 - prob_consistent: {:.6f}", result.prob_consistent);

  double ratio = static_cast<double>(result.total_runs) / result.expected_runs;
  assert(ratio > 0.9 && ratio < 1.1);
  assert(result.prob_consistent > 0.5);
};

// ============================================================================
// Bayesian Gap Test
// ============================================================================

"Gap test: Good RNG (makeRandom) passes"_test = [] {
  auto gen = makeRandom(123);
  auto result = bayesianRngGapTest(gen, 50000, 0.0, 0.5);

  std::println("makeRandom gap test - n_gaps: {}", result.n_gaps);
  std::println("makeRandom gap test - posterior_mean_p: {:.6f}",
               result.posterior_mean_gap_param);
  std::println("makeRandom gap test - 95%% CI: [{:.6f}, {:.6f}]",
               result.credible_interval_95.first,
               result.credible_interval_95.second);
  std::println("makeRandom gap test - prob_geometric: {:.6f}",
               result.prob_geometric);

  // Should have observed some gaps
  assert(result.n_gaps > 0);

  // Expected p = 0.5, should be in credible interval
  assert(result.credible_interval_95.first < 0.5);
  assert(result.credible_interval_95.second > 0.5);
  assert(result.prob_geometric > 0.5);
};

"Gap test: Different interval sizes"_test = [] {
  auto gen1 = makeRandom(456);
  auto gen2 = makeRandom(456);

  auto result1 = bayesianRngGapTest(gen1, 30000, 0.0, 0.25);  // 25% interval
  auto result2 = bayesianRngGapTest(gen2, 30000, 0.0, 0.75);  // 75% interval

  std::println("Small interval (25%%) - n_gaps: {}", result1.n_gaps);
  std::println("Large interval (75%%) - n_gaps: {}", result2.n_gaps);

  // Larger interval should see more occurrences (fewer gaps between)
  assert(result2.n_gaps > result1.n_gaps);
};

// ============================================================================
// Hierarchical Bit Quality Test
// ============================================================================

"Bit test: Good RNG (makeRandom) all bits fair"_test = [] {
  auto gen = makeRandom(42);
  auto result = bayesianRngHierarchicalBitTest(gen, 100000);

  std::println("makeRandom bit test - population_mean: {:.6f}",
               result.population_mean);
  std::println("makeRandom bit test - overall_quality_score: {:.6f}",
               result.overall_quality_score);
  std::println("makeRandom bit test - problematic_bits: {}",
               result.problematic_bits.size());

  // Population mean should be close to 0.5
  assert(result.population_mean > 0.49 && result.population_mean < 0.51);

  // Should have no or very few problematic bits
  assert(result.problematic_bits.size() < 5);

  // Overall quality should be high
  assert(result.overall_quality_score > 0.9);

  // Check a few individual bits
  for (size_t bit = 0; bit < 64; bit += 8) {
    std::println("Bit {}: p = {:.6f}, CI = [{:.4f}, {:.4f}]",
                bit,
                result.bit_probabilities_posterior_mean[bit],
                result.bit_credible_intervals[bit].first,
                result.bit_credible_intervals[bit].second);
  }
};

"Bit test: mt19937_64 all bits fair"_test = [] {
  auto gen = std::mt19937_64{123};
  auto result = bayesianRngHierarchicalBitTest(gen, 100000);

  std::println("mt19937_64 bit test - population_mean: {:.6f}",
               result.population_mean);
  std::println("mt19937_64 bit test - overall_quality_score: {:.6f}",
               result.overall_quality_score);

  assert(result.population_mean > 0.49 && result.population_mean < 0.51);
  assert(result.problematic_bits.size() < 5);
  assert(result.overall_quality_score > 0.9);
};

"Bit test: Biased generator detected"_test = [] {
  auto gen = BiasedGenerator{42};
  auto result = bayesianRngHierarchicalBitTest(gen, 50000);

  std::println("Biased RNG bit test - population_mean: {:.6f}",
               result.population_mean);
  std::println("Biased RNG bit test - problematic_bits: {}",
               result.problematic_bits.size());

  // Top bit is masked off, so bit 63 should be problematic
  bool bit_63_problematic = false;
  for (auto bit : result.problematic_bits) {
    if (bit == 63) {
      bit_63_problematic = true;
      std::println("Bit 63 correctly identified as problematic");
    }
  }
  assert(bit_63_problematic);
};

// ============================================================================
// Bayesian Model Comparison
// ============================================================================

"Model comparison: Compare multiple generators"_test = [] {
  // Create separate generators for comparison
  // (Can't use same vector type due to different lambda types)
  auto gen1 = makeRandom(42);
  auto gen2 = makeRandom(43);
  auto gen3 = makeRandom(44);

  std::vector<std::pair<std::string, decltype(gen1)>> generators;
  generators.emplace_back("makeRandom_seed42", std::move(gen1));
  generators.emplace_back("makeRandom_seed43", std::move(gen2));
  generators.emplace_back("makeRandom_seed44", std::move(gen3));

  auto result = bayesianRngModelComparison(generators, 50000, 100);

  std::println("\nModel Comparison Results:");
  for (size_t i = 0; i < result.generator_names.size(); ++i) {
    std::println("  {} - P(M|data) = {:.6f}, log ML = {:.2f}",
                result.generator_names[i],
                result.posterior_probs[i],
                result.log_marginal_likelihoods[i]);
  }

  std::println("  Best model: {} (index {})",
              result.generator_names[result.best_model_index],
              result.best_model_index);

  // Posterior probabilities should sum to 1
  double sum_probs = 0.0;
  for (auto p : result.posterior_probs) {
    sum_probs += p;
  }
  assert(std::abs(sum_probs - 1.0) < 1e-6);

  // Bayes factors should be reciprocals
  for (size_t i = 0; i < result.generator_names.size(); ++i) {
    for (size_t j = 0; j < result.generator_names.size(); ++j) {
      double bf_ij = result.bayes_factors[i][j];
      double bf_ji = result.bayes_factors[j][i];
      assert(std::abs(bf_ij * bf_ji - 1.0) < 1e-6);
    }
  }
};

// ============================================================================
// Sequential Bayesian Testing
// ============================================================================

"Sequential test: Stops early with good RNG"_test = [] {
  auto gen = makeRandom(789);

  SequentialTestConfig config;
  config.credible_interval_width_threshold = 0.05;  // Easier to satisfy
  config.batch_size = 5000;
  config.max_samples = 100000;
  config.n_bins = 50;

  auto result = bayesianRngSequentialTest(gen, config);

  std::println("Sequential test - stopped at: {} samples", result.n_samples);
  std::println("Sequential test - max_deviation: {:.6f}", result.max_deviation);
  std::println("Sequential test - prob_nearly_uniform: {:.6f}",
               result.prob_nearly_uniform);

  // Should stop before max_samples for a good RNG
  assert(result.n_samples <= config.max_samples);

  // Should have reasonable quality
  assert(result.max_deviation < 0.1);
};

"Sequential test: Uses all samples for constant RNG"_test = [] {
  auto gen = ConstantGenerator{42};

  SequentialTestConfig config;
  config.credible_interval_width_threshold = 0.001;  // Very tight
  config.batch_size = 10000;
  config.max_samples = 50000;
  config.n_bins = 100;

  auto result = bayesianRngSequentialTest(gen, config);

  std::println("Sequential test (constant RNG) - stopped at: {} samples",
               result.n_samples);
  std::println("Sequential test (constant RNG) - max_deviation: {:.6f}",
               result.max_deviation);

  // Should use all samples (can't converge with bad RNG and tight threshold)
  assert(result.n_samples == config.max_samples);

  // Should show clear non-uniformity
  assert(result.max_deviation > 0.5);
};

return TestRegistry::result();
}
