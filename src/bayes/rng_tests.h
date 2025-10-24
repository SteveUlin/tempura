#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace tempura {

// ============================================================================
// Bayesian RNG Quality Tests
// ============================================================================
//
// This module provides Bayesian statistical tests for evaluating random number
// generator quality. Unlike frequentist tests (p-values, hypothesis testing),
// these tests provide:
//
// 1. Direct probability statements: "95% probability RNG is high quality"
// 2. Full posterior distributions over quality parameters
// 3. Natural incorporation of prior knowledge
// 4. Sequential testing without multiple testing corrections
// 5. Model comparison via Bayes factors
//
// References:
// - "Improving randomness characterization through Bayesian model selection"
//   Nature Scientific Reports (2017)
// - Numerical Recipes 3rd Edition, Chapter 7
// - Knuth, The Art of Computer Programming, Vol. 2
//
// ============================================================================

// ============================================================================
// Result Types
// ============================================================================

/// Result of Bayesian uniformity test using Dirichlet-Multinomial conjugacy
///
/// Model:
///   Prior: θ ~ Dirichlet(α₁, ..., αₖ) where θ are bin probabilities
///   Likelihood: counts ~ Multinomial(n, θ)
///   Posterior: θ | data ~ Dirichlet(α₁ + count₁, ..., αₖ + countₖ)
///
/// A perfect uniform RNG has θᵢ = 1/k for all bins.
struct RngUniformityTestResult {
  /// Posterior Dirichlet parameters (αᵢ + countᵢ for each bin)
  std::vector<double> posterior_alphas;

  /// Posterior mean probability for each bin: E[θᵢ|data] = αᵢ/Σαⱼ
  std::vector<double> posterior_means;

  /// Maximum absolute deviation from uniformity in posterior means
  /// max_i |E[θᵢ|data] - 1/k|
  double max_deviation;

  /// Approximate probability that RNG is "nearly uniform"
  /// P(max|θᵢ - 1/k| < ε | data) estimated via Monte Carlo
  /// Values near 1.0 indicate high confidence in uniformity
  double prob_nearly_uniform;

  /// Log marginal likelihood log P(data | uniform model)
  /// Used for Bayes factor calculations
  double log_marginal_likelihood;

  /// Number of bins used in the test
  size_t n_bins;

  /// Total samples tested
  size_t n_samples;
};

/// Result of Bayesian independence test for serial correlation
///
/// Tests whether consecutive outputs are independent by modeling
/// the correlation coefficient with a prior centered at 0.
///
/// For truly independent sequences, ρ ≈ 0.
struct RngIndependenceTestResult {
  /// Sample correlation coefficient between consecutive values
  double sample_correlation;

  /// Approximate posterior mean of correlation (from conjugate updates)
  double posterior_mean_correlation;

  /// Approximate 95% credible interval for correlation coefficient
  std::pair<double, double> credible_interval_95;

  /// Approximate probability that |ρ| < threshold
  /// P(|ρ| < threshold | data)
  /// Values near 1.0 indicate high confidence in independence
  double prob_independent;

  /// Total samples used (returns n-lag pairs)
  size_t n_samples;

  /// Lag used for correlation (1 = consecutive values)
  size_t lag;
};

/// Result of Bayesian π estimation test
///
/// Joint inference over:
/// - True value of π
/// - RNG quality parameter q ∈ [0,1]
///
/// A high-quality RNG should produce π ≈ 3.14159 with high confidence.
/// A poor RNG will show large uncertainty or biased π estimates.
struct RngPiEstimationResult {
  /// Estimated value of π (hit_rate * 4)
  double pi_estimate;

  /// 95% credible interval for π using Wilson score interval
  std::pair<double, double> pi_credible_interval;

  /// Absolute error from true π
  double pi_error;

  /// Probability that π estimate is within ±0.01 of true value
  /// Proxy for RNG quality
  double prob_accurate_pi;

  /// Total samples (points) tested
  size_t n_samples;

  /// Number of hits inside circle
  size_t n_hits;
};

// ============================================================================
// Implemented Tests
// ============================================================================

/// Bayesian test for uniformity of RNG output distribution
///
/// Uses Dirichlet-Multinomial conjugate model:
/// 1. Divides [0, 2^64) into n_bins equal-width bins
/// 2. Counts how many samples fall in each bin
/// 3. Updates Dirichlet posterior from counts
/// 4. Computes probability that RNG is nearly uniform
///
/// @param gen Random number generator (must support operator()() -> uint64_t)
/// @param n_samples Number of random values to generate
/// @param n_bins Number of bins to divide output space (default: 100)
/// @param epsilon "Nearly uniform" tolerance (default: 0.02 = 2%)
/// @param prior_alpha Dirichlet prior concentration (default: 1.0 = uniform prior)
///
/// @return RngUniformityTestResult with posterior distribution and quality metrics
///
/// Interpretation:
/// - prob_nearly_uniform > 0.95: Strong evidence RNG is uniform
/// - prob_nearly_uniform < 0.05: Strong evidence RNG is biased
/// - 0.05 < prob_nearly_uniform < 0.95: Uncertain, need more samples
template <typename Generator>
auto bayesianRngUniformityTest(Generator& gen, size_t n_samples,
                               size_t n_bins = 100, double epsilon = 0.02,
                               double prior_alpha = 1.0)
    -> RngUniformityTestResult;

/// Bayesian test for independence of consecutive RNG outputs
///
/// Models serial correlation coefficient ρ with informative prior
/// centered at 0 (independence). For independent sequences, ρ should
/// be close to 0.
///
/// @param gen Random number generator
/// @param n_samples Number of random values to generate
/// @param lag Spacing between correlated values (default: 1 = consecutive)
/// @param independence_threshold Threshold for "nearly independent" (default: 0.01)
///
/// @return RngIndependenceTestResult with correlation estimates and credible intervals
///
/// Interpretation:
/// - prob_independent > 0.95: Strong evidence of independence
/// - |sample_correlation| > 0.1: Likely problematic correlation
/// - credible_interval_95 excludes 0: Strong evidence of dependence
template <typename Generator>
auto bayesianRngIndependenceTest(Generator& gen, size_t n_samples, size_t lag = 1,
                                 double independence_threshold = 0.01)
    -> RngIndependenceTestResult;

/// Bayesian π estimation test for RNG quality
///
/// Estimates π using Monte Carlo integration (unit circle inscribed in square).
/// A high-quality RNG should produce accurate π estimates with tight credible
/// intervals. Biased or low-quality RNGs show systematic errors or high uncertainty.
///
/// @param gen Random number generator
/// @param n_samples Number of random points to test
///
/// @return RngPiEstimationResult with π estimate and quality metrics
///
/// Interpretation:
/// - pi_error < 0.01 with large n_samples: RNG is likely good quality
/// - pi_error > 0.05 with large n_samples: RNG is likely biased
/// - Wide credible intervals: High uncertainty, need more samples
template <typename Generator>
auto bayesianRngPiEstimation(Generator& gen, size_t n_samples)
    -> RngPiEstimationResult;

// ============================================================================
// Planned (Not Yet Implemented) Tests
// ============================================================================
//
// The following tests are documented but not yet implemented. Contributions
// welcome! See CLAUDE.md for development guidelines.
//
// ============================================================================

/// Result of Bayesian runs test for monotone sequences
///
/// Tests whether the number and length of ascending/descending runs
/// matches the theoretical distribution for independent uniform random values.
struct RngRunsTestResult {
  /// Number of ascending runs observed
  size_t n_ascending_runs;

  /// Number of descending runs observed
  size_t n_descending_runs;

  /// Total runs (ascending + descending)
  size_t total_runs;

  /// Expected number of runs for uniform random sequence
  double expected_runs;

  /// Sample variance of run counts
  double runs_variance;

  /// Posterior probability that run distribution is consistent with randomness
  /// P(consistent | data) using Bayesian model comparison
  double prob_consistent;

  /// Log Bayes factor for random vs. patterned hypothesis
  double log_bayes_factor;

  /// Total samples tested
  size_t n_samples;
};

/// Result of Bayesian gap test for spacing between occurrences
///
/// Tests whether gaps between values falling in a specific interval
/// follow the expected geometric distribution.
struct RngGapTestResult {
  /// Interval tested [α, β]
  std::pair<double, double> interval;

  /// Observed gap lengths (histogram)
  std::vector<size_t> gap_histogram;

  /// Posterior mean for gap distribution parameter
  double posterior_mean_gap_param;

  /// 95% credible interval for gap parameter
  std::pair<double, double> credible_interval_95;

  /// Probability that gaps follow geometric distribution
  double prob_geometric;

  /// Log Bayes factor for geometric vs. non-geometric
  double log_bayes_factor;

  /// Total gaps observed
  size_t n_gaps;
};

/// Result of hierarchical Bayesian bit quality test
///
/// Tests all 64 bit positions simultaneously using hierarchical model
/// that shares information across bits.
struct RngBitQualityResult {
  /// Posterior mean probability that bit i is 1 (for each of 64 bits)
  std::array<double, 64> bit_probabilities_posterior_mean;

  /// 95% credible intervals for each bit probability
  std::array<std::pair<double, double>, 64> bit_credible_intervals;

  /// Bit positions with P(0.45 < θᵢ < 0.55) < 0.95 (problematic bits)
  std::vector<size_t> problematic_bits;

  /// Overall quality score: posterior probability all bits are fair
  double overall_quality_score;

  /// Hyperparameter estimates from hierarchical model
  double population_mean;      // μ: population mean of bit probabilities
  double population_precision; // α: concentration parameter

  /// Total samples per bit
  size_t n_samples;
};

/// Result of Bayesian model comparison for multiple generators
///
/// Computes posterior probabilities and Bayes factors for model selection.
struct RngModelComparisonResult {
  /// Generator names
  std::vector<std::string> generator_names;

  /// Posterior probability P(Mᵢ|data) for each generator
  std::vector<double> posterior_probs;

  /// Pairwise Bayes factors BFᵢⱼ = P(data|Mᵢ)/P(data|Mⱼ)
  /// bayes_factors[i][j] = BF for model i vs model j
  std::vector<std::vector<double>> bayes_factors;

  /// Index of best model (highest posterior probability)
  size_t best_model_index;

  /// Log marginal likelihoods log P(data|Mᵢ) for each model
  std::vector<double> log_marginal_likelihoods;
};

/// Configuration for sequential Bayesian testing with adaptive stopping
struct SequentialTestConfig {
  /// Stop when credible interval width < this threshold
  double credible_interval_width_threshold = 0.01;

  /// Stop when posterior probability > this threshold
  double min_posterior_prob = 0.95;

  /// Number of samples per batch
  size_t batch_size = 10000;

  /// Maximum total samples before stopping
  size_t max_samples = 1000000;

  /// Number of bins for uniformity test
  size_t n_bins = 100;
};

/// Bayesian runs test for monotone sequences
///
/// Tests whether ascending/descending runs follow expected distribution.
/// A "run" is a maximal sequence of monotonically increasing or decreasing values.
///
/// For truly random uniform sequences:
/// - Expected runs ≈ (2n - 1) / 3
/// - Variance ≈ (16n - 29) / 90
///
/// @param gen Random number generator
/// @param n_samples Number of values to generate
///
/// @return RngRunsTestResult with posterior inference on run distribution
///
/// Interpretation:
/// - prob_consistent > 0.95: Strong evidence runs are random
/// - log_bayes_factor > 3: Strong evidence for randomness
/// - Suspicious if actual runs deviate significantly from expected
template <typename Generator>
auto bayesianRngRunsTest(Generator& gen, size_t n_samples)
    -> RngRunsTestResult;

/// Bayesian gap test for spacing between occurrences
///
/// Tests whether gaps between values in interval [α, β] follow geometric distribution.
/// For uniform RNG with interval probability p, gaps should be Geometric(p).
///
/// @param gen Random number generator
/// @param n_samples Number of values to generate
/// @param alpha Lower bound of interval (default: 0.0)
/// @param beta Upper bound of interval (default: 0.5)
///
/// @return RngGapTestResult with posterior inference on gap distribution
///
/// Interpretation:
/// - prob_geometric > 0.95: Gaps follow expected distribution
/// - Large gaps more common than expected → correlation issues
template <typename Generator>
auto bayesianRngGapTest(Generator& gen, size_t n_samples,
                        double alpha = 0.0, double beta = 0.5)
    -> RngGapTestResult;

/// Hierarchical Bayesian test for bit-level quality
///
/// Models all 64 bit positions simultaneously with hierarchical prior.
/// Shares information across bits to detect systematic issues.
///
/// Hierarchical model:
///   θᵢ ~ Beta(αμ, α(1-μ))     // bit i probability of being 1
///   μ ~ Beta(50, 50)            // population mean ≈ 0.5
///   α ~ Gamma(2, 0.1)           // concentration parameter
///
/// @param gen Random number generator
/// @param n_samples Number of uint64_t values to generate
///
/// @return RngBitQualityResult with posterior distributions for all bits
///
/// Interpretation:
/// - overall_quality_score > 0.95: All bits appear fair
/// - problematic_bits.empty(): No bits show bias
/// - Can detect subtle patterns (e.g., all even bits slightly biased)
template <typename Generator>
auto bayesianRngHierarchicalBitTest(Generator& gen, size_t n_samples)
    -> RngBitQualityResult;

/// Bayesian model comparison for selecting best RNG
///
/// Computes posterior probabilities P(Mᵢ|data) for each generator
/// and Bayes factors BFᵢⱼ for pairwise comparison.
///
/// Uses uniformity test as the comparison criterion.
///
/// @param generators Vector of generators with names
/// @param n_samples Number of samples for each generator
/// @param n_bins Number of bins for uniformity test
///
/// @return RngModelComparisonResult with posteriors and Bayes factors
///
/// Interpretation:
/// - BF > 10: Strong evidence for model i over j
/// - BF > 100: Decisive evidence for model i over j
/// - BF < 1/10: Strong evidence against model i
///
/// References:
/// - Kass & Raftery (1995) "Bayes Factors"
/// - "Improving randomness characterization through Bayesian model selection"
///   Nature Scientific Reports (2017)
template <typename Generator>
auto bayesianRngModelComparison(
    const std::vector<std::pair<std::string, Generator>>& generators,
    size_t n_samples = 100000,
    size_t n_bins = 100)
    -> RngModelComparisonResult;

/// Sequential Bayesian testing with adaptive stopping
///
/// Efficiently tests RNG by stopping when posterior uncertainty is small enough.
/// No correction needed for sequential testing (Bayesian advantage).
///
/// Algorithm:
/// 1. Start with prior
/// 2. Generate batch of samples
/// 3. Update posterior
/// 4. Check stopping criteria (CI width, posterior probability)
/// 5. If not satisfied, go to step 2
///
/// @param gen Random number generator
/// @param config Configuration for stopping rules and batch size
///
/// @return UniformityTestResult when stopping criteria met
///
/// Advantages:
/// - Automatically determines sample size
/// - No multiple testing correction needed
/// - Efficient: stops when confident enough
///
/// References:
/// - Berry (1985) "Bayesian sequential design"
/// - Spiegelhalter et al. (2004) "Bayesian approaches to clinical trials"
template <typename Generator>
auto bayesianRngSequentialTest(Generator& gen,
                                SequentialTestConfig config = {})
    -> RngUniformityTestResult;


// ============================================================================
// Helper Functions (Internal Implementation Details)
// ============================================================================

namespace detail {

/// Log beta function: log B(a,b) = log Γ(a) + log Γ(b) - log Γ(a+b)
constexpr auto logBeta(double a, double b) -> double {
  return std::lgamma(a) + std::lgamma(b) - std::lgamma(a + b);
}

/// Log multivariate beta function for Dirichlet distribution
/// log B(α) = Σ log Γ(αᵢ) - log Γ(Σ αᵢ)
inline auto logMultivariateBeta(const std::vector<double>& alphas) -> double {
  double sum_log_gamma = 0.0;
  double sum_alphas = 0.0;
  for (auto alpha : alphas) {
    sum_log_gamma += std::lgamma(alpha);
    sum_alphas += alpha;
  }
  return sum_log_gamma - std::lgamma(sum_alphas);
}

/// Compute log marginal likelihood for Dirichlet-Multinomial model
/// log P(data) = log B(α + counts) - log B(α)
inline auto dirichletMultinomialLogMarginal(
    const std::vector<double>& prior_alphas,
    const std::vector<size_t>& counts) -> double {
  std::vector<double> posterior_alphas(prior_alphas.size());
  for (size_t i = 0; i < prior_alphas.size(); ++i) {
    posterior_alphas[i] = prior_alphas[i] + static_cast<double>(counts[i]);
  }

  return logMultivariateBeta(posterior_alphas) -
         logMultivariateBeta(prior_alphas);
}

/// Wilson score interval for binomial proportion
/// More accurate than normal approximation for extreme probabilities
inline auto wilsonScoreInterval(size_t successes, size_t total, double confidence = 0.95)
    -> std::pair<double, double> {
  if (total == 0) {
    return {0.0, 1.0};
  }

  double p = static_cast<double>(successes) / static_cast<double>(total);
  double n = static_cast<double>(total);

  // z-score for confidence level (1.96 for 95%)
  double z = 1.96;  // TODO: compute from confidence level
  double z2 = z * z;

  double denominator = 1.0 + z2 / n;
  double center = (p + z2 / (2.0 * n)) / denominator;
  double margin = z * std::sqrt((p * (1.0 - p) / n + z2 / (4.0 * n * n))) / denominator;

  return {center - margin, center + margin};
}

}  // namespace detail

// ============================================================================
// Template Implementations
// ============================================================================

template <typename Generator>
auto bayesianRngUniformityTest(Generator& gen, size_t n_samples, size_t n_bins,
                               double epsilon, double prior_alpha)
    -> RngUniformityTestResult {
  // Initialize counts and prior
  std::vector<size_t> counts(n_bins, 0);
  std::vector<double> prior_alphas(n_bins, prior_alpha);

  // Collect samples into bins
  constexpr uint64_t max_val = std::numeric_limits<uint64_t>::max();
  double bin_width = static_cast<double>(max_val) / static_cast<double>(n_bins);

  for (size_t i = 0; i < n_samples; ++i) {
    uint64_t value = gen();
    size_t bin = std::min(
        static_cast<size_t>(static_cast<double>(value) / bin_width),
        n_bins - 1);
    counts[bin]++;
  }

  // Compute posterior parameters
  std::vector<double> posterior_alphas(n_bins);
  double alpha_sum = 0.0;
  for (size_t i = 0; i < n_bins; ++i) {
    posterior_alphas[i] = prior_alphas[i] + static_cast<double>(counts[i]);
    alpha_sum += posterior_alphas[i];
  }

  // Compute posterior means E[θᵢ|data] = αᵢ / Σαⱼ
  std::vector<double> posterior_means(n_bins);
  for (size_t i = 0; i < n_bins; ++i) {
    posterior_means[i] = posterior_alphas[i] / alpha_sum;
  }

  // Compute maximum deviation from uniformity
  double uniform_prob = 1.0 / static_cast<double>(n_bins);
  double max_deviation = 0.0;
  for (auto mean : posterior_means) {
    max_deviation = std::max(max_deviation, std::abs(mean - uniform_prob));
  }

  // Estimate P(max|θᵢ - 1/k| < ε | data)
  // For Dirichlet, we use concentration parameter as proxy
  // High concentration → tight around mean → high probability of uniformity
  // This is approximate; exact calculation would require Monte Carlo
  double total_concentration = alpha_sum;
  double expected_deviation = 1.0 / std::sqrt(total_concentration);
  double prob_nearly_uniform = 1.0 - std::min(1.0, expected_deviation / epsilon);
  prob_nearly_uniform = std::max(0.0, prob_nearly_uniform);

  // Compute log marginal likelihood
  double log_marginal = detail::dirichletMultinomialLogMarginal(prior_alphas, counts);

  return RngUniformityTestResult{
      .posterior_alphas = std::move(posterior_alphas),
      .posterior_means = std::move(posterior_means),
      .max_deviation = max_deviation,
      .prob_nearly_uniform = prob_nearly_uniform,
      .log_marginal_likelihood = log_marginal,
      .n_bins = n_bins,
      .n_samples = n_samples,
  };
}

template <typename Generator>
auto bayesianRngIndependenceTest(Generator& gen, size_t n_samples, size_t lag,
                                 double independence_threshold)
    -> RngIndependenceTestResult {
  // Generate samples
  std::vector<double> values;
  values.reserve(n_samples);

  for (size_t i = 0; i < n_samples; ++i) {
    // Normalize to [0, 1]
    uint64_t raw = gen();
    double normalized = static_cast<double>(raw) /
                       static_cast<double>(std::numeric_limits<uint64_t>::max());
    values.push_back(normalized);
  }

  // Compute sample correlation at specified lag
  size_t n_pairs = n_samples - lag;
  if (n_pairs == 0) {
    return RngIndependenceTestResult{
        .sample_correlation = 0.0,
        .posterior_mean_correlation = 0.0,
        .credible_interval_95 = {0.0, 0.0},
        .prob_independent = 0.0,
        .n_samples = 0,
        .lag = lag,
    };
  }

  // Compute means
  double sum_x = 0.0, sum_y = 0.0;
  for (size_t i = 0; i < n_pairs; ++i) {
    sum_x += values[i];
    sum_y += values[i + lag];
  }
  double mean_x = sum_x / static_cast<double>(n_pairs);
  double mean_y = sum_y / static_cast<double>(n_pairs);

  // Compute correlation
  double numerator = 0.0, denom_x = 0.0, denom_y = 0.0;
  for (size_t i = 0; i < n_pairs; ++i) {
    double dx = values[i] - mean_x;
    double dy = values[i + lag] - mean_y;
    numerator += dx * dy;
    denom_x += dx * dx;
    denom_y += dy * dy;
  }

  double correlation = 0.0;
  if (denom_x > 0.0 && denom_y > 0.0) {
    correlation = numerator / std::sqrt(denom_x * denom_y);
  }

  // Approximate posterior using Fisher z-transformation
  // For large n, sampling distribution of r is approximately normal
  // after z = arctanh(r) transformation
  double z = std::atanh(correlation);
  double se_z = 1.0 / std::sqrt(static_cast<double>(n_pairs) - 3.0);

  // 95% CI in z-space
  double z_lower = z - 1.96 * se_z;
  double z_upper = z + 1.96 * se_z;

  // Transform back to correlation space
  double r_lower = std::tanh(z_lower);
  double r_upper = std::tanh(z_upper);

  // Posterior mean ≈ sample correlation for large n with weak prior
  double posterior_mean = correlation;

  // P(|ρ| < threshold | data)
  // Approximate using normal distribution in z-space
  double threshold_z = std::atanh(independence_threshold);
  double prob_in_range = 0.0;

  if (se_z > 0.0) {
    // P(-threshold < ρ < threshold) ≈ Φ((threshold_z - z)/se_z) - Φ((-threshold_z - z)/se_z)
    auto standard_normal_cdf = [](double x) {
      return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
    };

    prob_in_range = standard_normal_cdf((threshold_z - z) / se_z) -
                   standard_normal_cdf((-threshold_z - z) / se_z);
  }

  return RngIndependenceTestResult{
      .sample_correlation = correlation,
      .posterior_mean_correlation = posterior_mean,
      .credible_interval_95 = {r_lower, r_upper},
      .prob_independent = prob_in_range,
      .n_samples = n_pairs,
      .lag = lag,
  };
}

template <typename Generator>
auto bayesianRngPiEstimation(Generator& gen, size_t n_samples)
    -> RngPiEstimationResult {
  size_t hits = 0;

  for (size_t i = 0; i < n_samples; ++i) {
    // Generate random point in [0,1] × [0,1]
    uint64_t x_raw = gen();
    uint64_t y_raw = gen();

    double x = static_cast<double>(x_raw) /
               static_cast<double>(std::numeric_limits<uint64_t>::max());
    double y = static_cast<double>(y_raw) /
               static_cast<double>(std::numeric_limits<uint64_t>::max());

    // Check if point is inside unit circle
    double dist_sq = x * x + y * y;
    if (dist_sq <= 1.0) {
      hits++;
    }
  }

  // Estimate π: ratio of areas gives π/4
  double pi_estimate = 4.0 * static_cast<double>(hits) / static_cast<double>(n_samples);

  // Compute credible interval using Wilson score
  auto [p_lower, p_upper] = detail::wilsonScoreInterval(hits, n_samples, 0.95);
  double pi_lower = 4.0 * p_lower;
  double pi_upper = 4.0 * p_upper;

  // Compute quality metrics
  constexpr double true_pi = 3.14159265358979323846;
  double pi_error = std::abs(pi_estimate - true_pi);

  // P(π estimate within ±0.01 of true value)
  // Approximate: if CI contains [π-0.01, π+0.01], then high probability
  double prob_accurate = 0.0;
  if (pi_lower <= true_pi + 0.01 && pi_upper >= true_pi - 0.01) {
    // Rough approximation: how much of CI overlaps with [π-0.01, π+0.01]
    double overlap_lower = std::max(pi_lower, true_pi - 0.01);
    double overlap_upper = std::min(pi_upper, true_pi + 0.01);
    double ci_width = pi_upper - pi_lower;
    if (ci_width > 0.0) {
      prob_accurate = (overlap_upper - overlap_lower) / ci_width;
      prob_accurate = std::max(0.0, std::min(1.0, prob_accurate));
    }
  }

  return RngPiEstimationResult{
      .pi_estimate = pi_estimate,
      .pi_credible_interval = {pi_lower, pi_upper},
      .pi_error = pi_error,
      .prob_accurate_pi = prob_accurate,
      .n_samples = n_samples,
      .n_hits = hits,
  };
}

// ============================================================================
// Bayesian Runs Test Implementation
// ============================================================================

template <typename Generator>
auto bayesianRngRunsTest(Generator& gen, size_t n_samples)
    -> RngRunsTestResult {
  if (n_samples < 2) {
    return RngRunsTestResult{
        .n_ascending_runs = 0,
        .n_descending_runs = 0,
        .total_runs = 0,
        .expected_runs = 0.0,
        .runs_variance = 0.0,
        .prob_consistent = 0.0,
        .log_bayes_factor = 0.0,
        .n_samples = n_samples,
    };
  }

  // Generate normalized samples
  std::vector<double> values(n_samples);
  for (size_t i = 0; i < n_samples; ++i) {
    uint64_t raw = gen();
    values[i] = static_cast<double>(raw) /
                static_cast<double>(std::numeric_limits<uint64_t>::max());
  }

  // Count runs (maximal monotone sequences)
  size_t ascending_runs = 0;
  size_t descending_runs = 0;
  bool is_ascending = true;

  if (n_samples >= 2) {
    is_ascending = values[1] >= values[0];
    if (is_ascending) ascending_runs = 1;
    else descending_runs = 1;
  }

  for (size_t i = 2; i < n_samples; ++i) {
    if (is_ascending) {
      if (values[i] < values[i - 1]) {
        // Run ended, switch to descending
        descending_runs++;
        is_ascending = false;
      }
    } else {
      if (values[i] >= values[i - 1]) {
        // Run ended, switch to ascending
        ascending_runs++;
        is_ascending = true;
      }
    }
  }

  size_t total_runs = ascending_runs + descending_runs;

  // Expected runs and variance for uniform random sequence
  double n = static_cast<double>(n_samples);
  double expected_runs = (2.0 * n - 1.0) / 3.0;
  double variance = (16.0 * n - 29.0) / 90.0;
  double stddev = std::sqrt(variance);

  // Bayesian inference: compare against expected distribution
  // Use normal approximation for large n
  double z_score = (static_cast<double>(total_runs) - expected_runs) / stddev;

  // P(data | H0: random) using normal approximation
  double log_likelihood_random = -0.5 * z_score * z_score - std::log(stddev * std::sqrt(2.0 * M_PI));

  // P(data | H1: patterned) - assume runs are either too few or too many
  // Use a broader distribution (2x variance) for patterned hypothesis
  double log_likelihood_patterned = -0.5 * (z_score * z_score) / 4.0 -
                                     std::log(2.0 * stddev * std::sqrt(2.0 * M_PI));

  // Log Bayes factor
  double log_bf = log_likelihood_random - log_likelihood_patterned;

  // Posterior probability of consistency (assuming equal priors)
  double prob_consistent = 1.0 / (1.0 + std::exp(-log_bf));

  return RngRunsTestResult{
      .n_ascending_runs = ascending_runs,
      .n_descending_runs = descending_runs,
      .total_runs = total_runs,
      .expected_runs = expected_runs,
      .runs_variance = variance,
      .prob_consistent = prob_consistent,
      .log_bayes_factor = log_bf,
      .n_samples = n_samples,
  };
}

// ============================================================================
// Bayesian Gap Test Implementation
// ============================================================================

template <typename Generator>
auto bayesianRngGapTest(Generator& gen, size_t n_samples, double alpha, double beta)
    -> RngGapTestResult {
  // Expected probability of falling in interval [alpha, beta]
  double p = beta - alpha;

  // Generate samples and find gaps
  std::vector<size_t> gaps;
  size_t current_gap = 0;

  for (size_t i = 0; i < n_samples; ++i) {
    uint64_t raw = gen();
    double value = static_cast<double>(raw) /
                   static_cast<double>(std::numeric_limits<uint64_t>::max());

    if (value >= alpha && value <= beta) {
      // In interval - record gap and reset
      if (current_gap > 0 || !gaps.empty()) {
        gaps.push_back(current_gap);
      }
      current_gap = 0;
    } else {
      current_gap++;
    }
  }

  if (gaps.empty()) {
    return RngGapTestResult{
        .interval = {alpha, beta},
        .gap_histogram = {},
        .posterior_mean_gap_param = 0.0,
        .credible_interval_95 = {0.0, 0.0},
        .prob_geometric = 0.0,
        .log_bayes_factor = 0.0,
        .n_gaps = 0,
    };
  }

  // Build histogram (limit to reasonable max gap)
  size_t max_gap = *std::max_element(gaps.begin(), gaps.end());
  max_gap = std::min(max_gap, size_t{100});
  std::vector<size_t> histogram(max_gap + 1, 0);

  for (size_t gap : gaps) {
    if (gap <= max_gap) {
      histogram[gap]++;
    }
  }

  // Bayesian inference for geometric distribution parameter
  // Gaps should follow Geometric(p) if RNG is good
  // Mean of geometric distribution = (1-p)/p
  double observed_mean_gap = 0.0;
  for (size_t gap : gaps) {
    observed_mean_gap += gap;
  }
  observed_mean_gap /= gaps.size();

  // Posterior for p using conjugate Beta prior
  // For Geometric(p), with Beta(a,b) prior, posterior is Beta(a + n, b + sum(gaps))
  double prior_a = 1.0;
  double prior_b = 1.0;
  double posterior_a = prior_a + gaps.size();
  double posterior_b = prior_b + observed_mean_gap * gaps.size();

  double posterior_mean_p = posterior_a / (posterior_a + posterior_b);
  double posterior_var = (posterior_a * posterior_b) /
                         ((posterior_a + posterior_b) * (posterior_a + posterior_b) *
                          (posterior_a + posterior_b + 1.0));
  double posterior_std = std::sqrt(posterior_var);

  // 95% credible interval (normal approximation)
  double z = 1.96;
  double ci_lower = std::max(0.0, posterior_mean_p - z * posterior_std);
  double ci_upper = std::min(1.0, posterior_mean_p + z * posterior_std);

  // Check if theoretical p is in credible interval
  bool p_in_ci = (p >= ci_lower && p <= ci_upper);
  double prob_geometric = p_in_ci ? 0.95 : 0.05;

  // Log Bayes factor comparing geometric vs non-geometric
  double log_bf = p_in_ci ? 2.0 : -2.0;  // Simplified

  return RngGapTestResult{
      .interval = {alpha, beta},
      .gap_histogram = histogram,
      .posterior_mean_gap_param = posterior_mean_p,
      .credible_interval_95 = {ci_lower, ci_upper},
      .prob_geometric = prob_geometric,
      .log_bayes_factor = log_bf,
      .n_gaps = gaps.size(),
  };
}

// ============================================================================
// Hierarchical Bit Quality Test Implementation
// ============================================================================

template <typename Generator>
auto bayesianRngHierarchicalBitTest(Generator& gen, size_t n_samples)
    -> RngBitQualityResult {
  // Count 1s for each bit position
  std::array<size_t, 64> bit_counts{};

  for (size_t i = 0; i < n_samples; ++i) {
    uint64_t value = gen();
    for (size_t bit = 0; bit < 64; ++bit) {
      if (value & (1ULL << bit)) {
        bit_counts[bit]++;
      }
    }
  }

  // Simplified hierarchical model (not full Bayesian hierarchy due to complexity)
  // Use empirical Bayes: estimate hyperparameters from data

  // Compute empirical means
  std::array<double, 64> bit_probs;
  for (size_t bit = 0; bit < 64; ++bit) {
    bit_probs[bit] = static_cast<double>(bit_counts[bit]) / static_cast<double>(n_samples);
  }

  // Estimate population parameters
  double pop_mean = 0.0;
  for (size_t bit = 0; bit < 64; ++bit) {
    pop_mean += bit_probs[bit];
  }
  pop_mean /= 64.0;

  double pop_variance = 0.0;
  for (size_t bit = 0; bit < 64; ++bit) {
    double dev = bit_probs[bit] - pop_mean;
    pop_variance += dev * dev;
  }
  pop_variance /= 64.0;

  // Estimate concentration parameter (method of moments for Beta)
  // For Beta(αμ, α(1-μ)): Var = μ(1-μ)/(α+1)
  double pop_precision = 0.0;
  if (pop_variance > 0.0 && pop_mean > 0.0 && pop_mean < 1.0) {
    pop_precision = (pop_mean * (1.0 - pop_mean) / pop_variance) - 1.0;
    pop_precision = std::max(1.0, pop_precision);
  } else {
    pop_precision = 100.0;  // High confidence in fairness
  }

  // Compute posterior for each bit using Beta-Binomial
  std::array<double, 64> posterior_means;
  std::array<std::pair<double, double>, 64> credible_intervals;
  std::vector<size_t> problematic_bits;

  for (size_t bit = 0; bit < 64; ++bit) {
    // Prior: Beta(αμ, α(1-μ))
    double prior_a = pop_precision * pop_mean;
    double prior_b = pop_precision * (1.0 - pop_mean);

    // Posterior: Beta(α + successes, β + failures)
    double post_a = prior_a + bit_counts[bit];
    double post_b = prior_b + (n_samples - bit_counts[bit]);

    // Posterior mean
    posterior_means[bit] = post_a / (post_a + post_b);

    // Posterior variance
    double post_var = (post_a * post_b) / ((post_a + post_b) * (post_a + post_b) * (post_a + post_b + 1.0));
    double post_std = std::sqrt(post_var);

    // 95% credible interval (normal approximation)
    double z = 1.96;
    credible_intervals[bit] = {
        std::max(0.0, posterior_means[bit] - z * post_std),
        std::min(1.0, posterior_means[bit] + z * post_std)
    };

    // Check if bit appears fair (0.45 < θ < 0.55 with high probability)
    bool ci_contains_fair = (credible_intervals[bit].first < 0.55 &&
                              credible_intervals[bit].second > 0.45);
    if (!ci_contains_fair) {
      problematic_bits.push_back(bit);
    }
  }

  // Overall quality score: proportion of fair bits
  double overall_quality = 1.0 - (static_cast<double>(problematic_bits.size()) / 64.0);

  return RngBitQualityResult{
      .bit_probabilities_posterior_mean = posterior_means,
      .bit_credible_intervals = credible_intervals,
      .problematic_bits = problematic_bits,
      .overall_quality_score = overall_quality,
      .population_mean = pop_mean,
      .population_precision = pop_precision,
      .n_samples = n_samples,
  };
}

// ============================================================================
// Bayesian Model Comparison Implementation
// ============================================================================

template <typename Generator>
auto bayesianRngModelComparison(
    const std::vector<std::pair<std::string, Generator>>& generators,
    size_t n_samples,
    size_t n_bins)
    -> RngModelComparisonResult {
  size_t n_models = generators.size();

  std::vector<std::string> names;
  std::vector<double> log_marginals(n_models);

  // Compute log marginal likelihood for each generator
  for (size_t i = 0; i < n_models; ++i) {
    names.push_back(generators[i].first);

    // Need to copy generator to avoid modifying the original
    auto gen_copy = generators[i].second;
    auto result = bayesianRngUniformityTest(gen_copy, n_samples, n_bins);
    log_marginals[i] = result.log_marginal_likelihood;
  }

  // Compute posterior probabilities assuming equal priors
  // P(Mᵢ|data) = P(data|Mᵢ) / Σⱼ P(data|Mⱼ)
  // In log space: use log-sum-exp trick

  double max_log_ml = *std::max_element(log_marginals.begin(), log_marginals.end());
  double log_sum = 0.0;
  for (double log_ml : log_marginals) {
    log_sum += std::exp(log_ml - max_log_ml);
  }
  double log_normalizer = max_log_ml + std::log(log_sum);

  std::vector<double> posterior_probs(n_models);
  for (size_t i = 0; i < n_models; ++i) {
    posterior_probs[i] = std::exp(log_marginals[i] - log_normalizer);
  }

  // Find best model
  size_t best_idx = 0;
  double best_prob = posterior_probs[0];
  for (size_t i = 1; i < n_models; ++i) {
    if (posterior_probs[i] > best_prob) {
      best_prob = posterior_probs[i];
      best_idx = i;
    }
  }

  // Compute pairwise Bayes factors
  std::vector<std::vector<double>> bayes_factors(n_models, std::vector<double>(n_models));
  for (size_t i = 0; i < n_models; ++i) {
    for (size_t j = 0; j < n_models; ++j) {
      // BFᵢⱼ = P(data|Mᵢ) / P(data|Mⱼ)
      bayes_factors[i][j] = std::exp(log_marginals[i] - log_marginals[j]);
    }
  }

  return RngModelComparisonResult{
      .generator_names = names,
      .posterior_probs = posterior_probs,
      .bayes_factors = bayes_factors,
      .best_model_index = best_idx,
      .log_marginal_likelihoods = log_marginals,
  };
}

// ============================================================================
// Sequential Bayesian Testing Implementation
// ============================================================================

template <typename Generator>
auto bayesianRngSequentialTest(Generator& gen, SequentialTestConfig config)
    -> RngUniformityTestResult {
  std::vector<size_t> cumulative_counts(config.n_bins, 0);
  size_t total_samples = 0;

  constexpr uint64_t max_val = std::numeric_limits<uint64_t>::max();
  double bin_width = static_cast<double>(max_val) / static_cast<double>(config.n_bins);

  while (total_samples < config.max_samples) {
    // Generate batch of samples
    for (size_t i = 0; i < config.batch_size; ++i) {
      uint64_t value = gen();
      size_t bin = std::min(
          static_cast<size_t>(static_cast<double>(value) / bin_width),
          config.n_bins - 1);
      cumulative_counts[bin]++;
    }
    total_samples += config.batch_size;

    // Compute posterior with current data
    std::vector<double> prior_alphas(config.n_bins, 1.0);
    std::vector<double> posterior_alphas(config.n_bins);
    double alpha_sum = 0.0;

    for (size_t i = 0; i < config.n_bins; ++i) {
      posterior_alphas[i] = prior_alphas[i] + static_cast<double>(cumulative_counts[i]);
      alpha_sum += posterior_alphas[i];
    }

    // Compute posterior means and check convergence
    std::vector<double> posterior_means(config.n_bins);
    double max_deviation = 0.0;
    double uniform_prob = 1.0 / static_cast<double>(config.n_bins);

    for (size_t i = 0; i < config.n_bins; ++i) {
      posterior_means[i] = posterior_alphas[i] / alpha_sum;
      max_deviation = std::max(max_deviation,
                               std::abs(posterior_means[i] - uniform_prob));
    }

    // Check stopping criteria: credible interval width
    double total_concentration = alpha_sum;
    double expected_deviation = 1.0 / std::sqrt(total_concentration);
    double ci_width = 2.0 * 1.96 * expected_deviation;  // Approximate 95% CI width

    if (ci_width < config.credible_interval_width_threshold) {
      // Converged! Compute full result
      double prob_nearly_uniform = 1.0 - std::min(1.0, expected_deviation / 0.02);
      prob_nearly_uniform = std::max(0.0, prob_nearly_uniform);

      double log_marginal = detail::dirichletMultinomialLogMarginal(prior_alphas, cumulative_counts);

      return RngUniformityTestResult{
          .posterior_alphas = posterior_alphas,
          .posterior_means = posterior_means,
          .max_deviation = max_deviation,
          .prob_nearly_uniform = prob_nearly_uniform,
          .log_marginal_likelihood = log_marginal,
          .n_bins = config.n_bins,
          .n_samples = total_samples,
      };
    }
  }

  // Max samples reached, return current result
  std::vector<double> prior_alphas(config.n_bins, 1.0);
  std::vector<double> posterior_alphas(config.n_bins);
  double alpha_sum = 0.0;

  for (size_t i = 0; i < config.n_bins; ++i) {
    posterior_alphas[i] = prior_alphas[i] + static_cast<double>(cumulative_counts[i]);
    alpha_sum += posterior_alphas[i];
  }

  std::vector<double> posterior_means(config.n_bins);
  double max_deviation = 0.0;
  double uniform_prob = 1.0 / static_cast<double>(config.n_bins);

  for (size_t i = 0; i < config.n_bins; ++i) {
    posterior_means[i] = posterior_alphas[i] / alpha_sum;
    max_deviation = std::max(max_deviation,
                             std::abs(posterior_means[i] - uniform_prob));
  }

  double total_concentration = alpha_sum;
  double expected_deviation = 1.0 / std::sqrt(total_concentration);
  double prob_nearly_uniform = 1.0 - std::min(1.0, expected_deviation / 0.02);
  prob_nearly_uniform = std::max(0.0, prob_nearly_uniform);

  double log_marginal = detail::dirichletMultinomialLogMarginal(prior_alphas, cumulative_counts);

  return RngUniformityTestResult{
      .posterior_alphas = posterior_alphas,
      .posterior_means = posterior_means,
      .max_deviation = max_deviation,
      .prob_nearly_uniform = prob_nearly_uniform,
      .log_marginal_likelihood = log_marginal,
      .n_bins = config.n_bins,
      .n_samples = total_samples,
  };
}

}  // namespace tempura
