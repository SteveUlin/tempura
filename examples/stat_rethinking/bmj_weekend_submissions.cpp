// BMJ Weekend Submissions Analysis
// Statistical Rethinking 2025 - Hierarchical Models
// Data: BMJSubmissions.csv (doi:10.1136/bmj.l6460)
//
// Compares no-pooling vs partial-pooling estimates for weekend submission
// probability across 38 countries. Demonstrates shrinkage toward the global mean.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <numeric>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "bayes/estimation/metropolis_hastings.h"
#include "bayes2/bayes2.h"
#include "csv.h"
#include "plot.h"
#include "prob/log_prob.h"
#include "symbolic3/indexed_evaluate.h"

using namespace tempura;
using namespace tempura::bayes2;
using namespace tempura::prob;
using namespace tempura::symbolic3;

// Dimension tag for plate notation - represents the set of countries
struct Countries {};

struct CountryData {
  std::string name;
  int64_t country_id;
  int64_t total;
  int64_t weekend;
};

// Beta PDF (unnormalized for plotting - we only care about shape)
auto betaPdf(double alpha, double beta) {
  return [alpha, beta](double x) -> double {
    if (x <= 0.0 || x >= 1.0) return 0.0;
    // Use log to avoid overflow, then exp
    double log_pdf = (alpha - 1) * std::log(x) + (beta - 1) * std::log(1 - x);
    return std::exp(log_pdf);
  };
}

// sparklineHistogram is now provided by plot.h

// Beta distribution statistics
struct BetaStats {
  double alpha;
  double beta;
  double mean;
  double std;
  double ci_low;   // 2.5th percentile (approx)
  double ci_high;  // 97.5th percentile (approx)
};

auto betaStats(double alpha, double beta) -> BetaStats {
  double mean = alpha / (alpha + beta);
  double var = alpha * beta / ((alpha + beta) * (alpha + beta) * (alpha + beta + 1));
  double std = std::sqrt(var);
  // Approximate 95% CI using normal approximation (good for large α, β)
  return {alpha, beta, mean, std, mean - 1.96 * std, mean + 1.96 * std};
}

// Peak-normalized Beta PDF - scales so maximum value is 1.0
// This makes narrow distributions visible alongside wide ones
auto betaPdfNormalized(double alpha, double beta) {
  // Mode of Beta distribution (where PDF is maximum)
  double mode = (alpha - 1) / (alpha + beta - 2);
  if (alpha <= 1) mode = 0.001;  // Handle edge cases
  if (beta <= 1) mode = 0.999;

  // Compute the peak value for normalization
  auto raw_pdf = betaPdf(alpha, beta);
  double peak = raw_pdf(mode);

  return [alpha, beta, peak](double x) -> double {
    if (x <= 0.0 || x >= 1.0) return 0.0;
    double log_pdf = (alpha - 1) * std::log(x) + (beta - 1) * std::log(1 - x);
    return std::exp(log_pdf) / peak;  // Normalize to [0, 1]
  };
}

// Parse CSV and aggregate by country using CsvReader
auto loadData(const std::string& path) -> std::vector<CountryData> {
  std::ifstream file{path};
  if (!file) {
    std::println(stderr, "Error: Cannot open {}", path);
    return {};
  }

  CsvReader reader{file};
  reader.skipRows(1);  // Skip header

  std::vector<CountryData> countries(39);  // IDs 1-38

  while (reader.nextRow()) {
    // Columns: T, Y, country_name, L (country_id), W (weekend)
    std::string name = reader.get<std::string>(2);
    int64_t id = reader.get<int64_t>(3);
    int64_t weekend = reader.get<int64_t>(4);

    countries[id].name = name;
    countries[id].country_id = id;
    countries[id].total++;
    countries[id].weekend += weekend;
  }

  std::vector<CountryData> result;
  for (const auto& c : countries) {
    if (c.total > 0) {
      result.push_back(c);
    }
  }
  std::ranges::sort(result, [](const auto& a, const auto& b) {
    return a.total > b.total;
  });

  return result;
}


auto main() -> int {
  std::mt19937_64 rng{42};
  constexpr int kSamples = 10'000;

  std::println(R"(
=== BMJ WEEKEND SUBMISSIONS ANALYSIS ===

Data: 49,464 article submissions to The BMJ (2012-2018)
Question: What is the probability that authors from each country submit on weekends?
)");

  auto countries = loadData("examples/stat_rethinking/BMJSubmissions.csv");
  if (countries.empty()) return 1;

  std::println("Loaded {} countries\n", countries.size());

  int64_t total_n = 0;
  int64_t total_k = 0;
  for (const auto& c : countries) {
    total_n += c.total;
    total_k += c.weekend;
  }
  double global_rate = static_cast<double>(total_k) / total_n;

  // Prior hyperparameters for Beta distribution on country-level rates.
  // These reflect our PRIOR BELIEFS before seeing any data:
  //   - If submissions were uniformly random across days: 2/7 ≈ 28.6%
  //   - But research suggests people prefer weekdays, so center lower at ~15%
  //   - Weak prior (low concentration) allows data to dominate
  //
  // Beta(3, 17) has mean = 3/20 = 15%, std ≈ 7.7%
  // This says: "Rates probably around 15%, rarely above 30%"
  //
  // NOTE: In a full Bayesian treatment, we would put priors on α and β:
  //   alpha ~ Gamma(2, 0.1)
  //   beta ~ Gamma(2, 0.1)
  // And use HMC to jointly sample (α, β, θ[1]...θ[38]) from the posterior.
  // This example uses fixed hyperparameters for simplicity.
  constexpr double alpha_prior = 3.0;
  constexpr double beta_prior = 17.0;

  // =========================================================================
  // GENERATIVE MODEL VISUALIZATION
  // =========================================================================
  std::println(R"(
=== THE GENERATIVE MODEL ===

FULL BAYESIAN (hierarchical):             SIMPLIFIED (this example):
  alpha ~ Gamma(2, 0.1)                     alpha = 3     (fixed)
  beta  ~ Gamma(2, 0.1)                     beta  = 17    (fixed)
  theta[i] ~ Beta(alpha, beta)              theta[i] ~ Beta(alpha, beta)
  k ~ Binomial(n, theta[i])                 k ~ Binomial(n, theta[i])

NO-POOLING (baseline):
  theta[i] ~ Beta(1, 1) = Uniform
  k ~ Binomial(n, theta[i])

The simplified model uses conjugate Beta-Binomial updating (analytical).
Full Bayesian inference would use HMC to jointly sample (α, β, θ[1..38]).
)");

  // Plot 1: Prior distributions (before any data)
  std::println("=== PRIOR DISTRIBUTIONS (before seeing any data) ===\n");

  std::function<double(double)> flat_prior = betaPdf(1.0, 1.0);
  std::function<double(double)> hierarchical_prior = betaPdf(alpha_prior, beta_prior);

  std::vector<Series> prior_series = {
      {flat_prior, colors::kCyan, "No-pooling: Beta(1,1) = Uniform"},
      {hierarchical_prior, colors::kYellow,
       std::format("Partial-pooling: Beta({:.1f}, {:.1f})", alpha_prior, beta_prior)},
  };

  std::print("{}", multiPlotBraille(prior_series, 0.0, 0.5, 70, 12,
                                    true, false, "Prior Beliefs About Weekend Rate"));

  double prior_mean = alpha_prior / (alpha_prior + beta_prior) * 100;
  std::println(R"(
The flat prior (cyan) says "any rate 0-100% is equally likely"
The hierarchical prior (yellow) says "rates cluster around {:.0f}%, rarely exceed 30%"
  This encodes prior belief that weekend rates are lower than random (2/7 ≈ 29%)
)", prior_mean);

  // =========================================================================
  // POSTERIOR COMPARISON FOR SELECTED COUNTRIES
  // =========================================================================
  std::println("\n=== POSTERIOR DISTRIBUTIONS (after seeing data) ===\n");

  // Find interesting countries: one large, one medium, one small sample
  auto find_country = [&](const std::string& name) -> const CountryData* {
    for (const auto& c : countries) {
      if (c.name == name) return &c;
    }
    return nullptr;
  };

  // Large sample: UK (n~11k)
  // Medium sample: Japan (n~1.5k)
  // Small sample: Cameroon (n~100)
  struct ExampleCountry {
    std::string name;
    int64_t n;
    int64_t k;
  };
  std::vector<ExampleCountry> examples;

  if (auto* uk = find_country("United Kingdom")) {
    examples.push_back({"UK", uk->total, uk->weekend});
  }
  if (auto* japan = find_country("Japan")) {
    examples.push_back({"Japan", japan->total, japan->weekend});
  }
  if (auto* cameroon = find_country("Cameroon")) {
    examples.push_back({"Cameroon", cameroon->total, cameroon->weekend});
  }

  std::println("How CERTAIN are we about each country's weekend rate?\n");
  std::println("{:12} {:>7} {:>9}  {:>20}  {:>8}",
               "Country", "n", "Estimate", "95% Credible Interval", "Width");
  std::println("{:-<65}", "");

  for (const auto& ex : examples) {
    // No-pooling posterior: Beta(1 + k, 1 + n - k)
    auto no_pool = betaStats(1.0 + ex.k, 1.0 + ex.n - ex.k);

    std::println("{:12} {:7}   {:6.2f}%    [{:5.2f}%, {:5.2f}%]    ±{:.2f}%",
                 ex.name, ex.n,
                 no_pool.mean * 100,
                 no_pool.ci_low * 100, no_pool.ci_high * 100,
                 (no_pool.ci_high - no_pool.ci_low) / 2 * 100);
  }

  std::println(R"(
The KEY insight: Sample size determines CERTAINTY, not the estimate itself.

- UK has 11k observations → we're very certain (±0.6% range)
- Japan has 1.5k observations → moderately certain (±1.6% range)
- Cameroon has 103 observations → quite uncertain (±7.5% range)

When we're UNCERTAIN (small n), the prior has more influence → shrinkage!
)");

  // Show 95% credible intervals as horizontal bars - clearer than bell curves
  // for comparing uncertainty across countries
  std::println("=== CREDIBLE INTERVAL COMPARISON ===\n");
  std::println("Each bar shows the 95%% credible interval for weekend submission rate.");
  std::println("Wider bar = more uncertainty, narrower bar = more certainty.\n");

  // Find overall range for consistent x-axis
  constexpr double x_min_all = 0.05;
  constexpr double x_max_all = 0.35;
  constexpr int bar_width = 60;

  // X-axis labels
  std::println("         5%%       10%%       15%%       20%%       25%%       30%%       35%%");
  std::println("         |         |         |         |         |         |         |");

  for (const auto& ex : examples) {
    auto no_pool = betaStats(1.0 + ex.k, 1.0 + ex.n - ex.k);
    auto partial_pool = betaStats(alpha_prior + ex.k,
                                  beta_prior + ex.n - ex.k);

    double shrinkage = (no_pool.mean - partial_pool.mean) * 100;

    std::println("\n{} (n={}):", ex.name, ex.n);

    // No-pooling bar (cyan)
    std::print("  No-pool:    {}  {:.1f}%% [{:.1f}%%, {:.1f}%%]\n",
               credibleIntervalBar(no_pool.ci_low, no_pool.ci_high, no_pool.mean,
                                   x_min_all, x_max_all, bar_width, colors::kCyan),
               no_pool.mean * 100, no_pool.ci_low * 100, no_pool.ci_high * 100);

    // Partial-pooling bar (yellow)
    std::print("  Partial:    {}  {:.1f}%% [{:.1f}%%, {:.1f}%%]\n",
               credibleIntervalBar(partial_pool.ci_low, partial_pool.ci_high, partial_pool.mean,
                                   x_min_all, x_max_all, bar_width, colors::kYellow),
               partial_pool.mean * 100, partial_pool.ci_low * 100, partial_pool.ci_high * 100);

    std::println("  Shrinkage: {:+.2f}pp toward global mean ({:.1f}%%)", -shrinkage, global_rate * 100);
  }

  std::println(R"(
Reading the bars:
- UK: Bars are nearly invisible (very narrow) - extremely high certainty
- Japan: Medium-width bars - moderate certainty
- Cameroon: Wide bars - high uncertainty, partial-pooling pulls estimate left
)");

  // Show compact sparkline histograms for each country
  std::println("=== POSTERIOR DISTRIBUTIONS (sparkline histograms) ===\n");
  std::println("Each histogram shows the probability density. Taller = more likely.");
  std::println("All plotted on same x-axis: 5%% to 35%%\n");

  constexpr double hist_min = 0.05;
  constexpr double hist_max = 0.35;
  constexpr int hist_bins = 60;

  // X-axis scale
  std::println("              5%%        10%%        15%%        20%%        25%%        30%%        35%%");

  for (const auto& ex : examples) {
    auto no_pool = betaStats(1.0 + ex.k, 1.0 + ex.n - ex.k);
    auto partial_pool = betaStats(alpha_prior + ex.k,
                                  beta_prior + ex.n - ex.k);

    std::function<double(double)> no_pool_pdf =
        betaPdf(1.0 + ex.k, 1.0 + ex.n - ex.k);
    std::function<double(double)> partial_pdf =
        betaPdf(alpha_prior + ex.k, beta_prior + ex.n - ex.k);

    auto [np_top, np_bot] = sparklineHistogram(no_pool_pdf, hist_min, hist_max, hist_bins);
    auto [pp_top, pp_bot] = sparklineHistogram(partial_pdf, hist_min, hist_max, hist_bins);

    double shrinkage = (no_pool.mean - partial_pool.mean) * 100;

    std::println("\n{} (n={}):", ex.name, ex.n);

    // For very narrow distributions, add a position marker
    bool is_narrow = (no_pool.std < 0.005);

    if (is_narrow) {
      // Create a marker line showing where the spike is
      std::string marker(hist_bins, ' ');
      int pos = static_cast<int>((no_pool.mean - hist_min) / (hist_max - hist_min) * hist_bins);
      pos = std::clamp(pos, 0, hist_bins - 1);
      marker[pos] = '|';
      std::println("  No-pool:  \033[38;2;100;255;255m{}█\033[0m  {:.1f}%% (spike too narrow to show)", marker, no_pool.mean * 100);
      marker[pos] = '|';
      int pos2 = static_cast<int>((partial_pool.mean - hist_min) / (hist_max - hist_min) * hist_bins);
      pos2 = std::clamp(pos2, 0, hist_bins - 1);
      marker = std::string(hist_bins, ' ');
      marker[pos2] = '|';
      std::println("  Partial:  \033[38;2;255;255;100m{}█\033[0m  {:.1f}%%", marker, partial_pool.mean * 100);
    } else {
      std::println("  No-pool:  \033[38;2;100;255;255m{}\033[0m  {:.1f}%%", np_top, no_pool.mean * 100);
      std::println("            \033[38;2;100;255;255m{}\033[0m", np_bot);
      std::println("  Partial:  \033[38;2;255;255;100m{}\033[0m  {:.1f}%%", pp_top, partial_pool.mean * 100);
      std::println("            \033[38;2;255;255;100m{}\033[0m", pp_bot);
    }
    std::println("  Shrinkage: {:+.2f}pp", -shrinkage);
  }

  // =========================================================================
  // HIERARCHICAL MODEL WITH INDEXED API
  // =========================================================================
  std::println("\n=== HIERARCHICAL MODEL USING INDEXED API ===\n");

  // Define the FULL hierarchical model with Binomial likelihood:
  //
  //   alpha ~ Gamma(2, 0.1)          // hyperparameter prior
  //   beta_hyper ~ Gamma(2, 0.1)     // hyperparameter prior
  //   theta[i] ~ Beta(alpha, beta)   // per-country probability
  //   k[i] ~ Binomial(n[i], theta[i]) // observed weekend counts
  //
  // In bayes2, this is expressed naturally with plate notation:

  auto alpha_hyper = gamma(2.0, 0.1);
  auto beta_hyper = gamma(2.0, 0.1);
  auto theta = plate<Countries>(beta(alpha_hyper, beta_hyper));

  // n[i] is observed data (trial counts), represented as an indexed symbol
  auto n_obs = indexedSymbol<Countries>();

  // k[i] ~ Binomial(n[i], theta[i]) - the observed data node
  auto k_obs = plate<Countries>(binomial(n_obs, theta));

  // Prepare observed data as indexed arrays
  std::vector<double> n_values;  // total submissions per country
  std::vector<double> k_values;  // weekend submissions per country
  for (const auto& c : countries) {
    n_values.push_back(static_cast<double>(c.total));
    k_values.push_back(static_cast<double>(c.weekend));
  }

  // Compute posterior means for each country using conjugate update
  std::vector<double> theta_posterior_means;
  for (const auto& c : countries) {
    double post_alpha = alpha_prior + c.weekend;
    double post_beta = beta_prior + c.total - c.weekend;
    theta_posterior_means.push_back(post_alpha / (post_alpha + post_beta));
  }

  // jointLogProb(k_obs) traverses the FULL DAG:
  //   k_obs → binomial log-prob (summed over countries)
  //   → theta → beta log-prob (summed over countries)
  //   → alpha_hyper, beta_hyper → gamma log-probs
  auto full_joint_lp = jointLogProb(k_obs);

  double total_log_prob = evaluate(full_joint_lp,
                                   BinderPack{alpha_hyper = alpha_prior,
                                              beta_hyper = beta_prior,
                                              theta = indexed(theta_posterior_means),
                                              n_obs = indexed(n_values),
                                              k_obs = indexed(k_values)});

  std::println("Model: theta[i] ~ Beta(alpha, beta), k[i] ~ Binomial(n[i], theta[i])");
  std::println("  alpha = {:.2f}, beta = {:.2f}", alpha_prior, beta_prior);
  std::println("  {} countries with posterior means", countries.size());
  std::println("  jointLogProb(k_obs) traverses full DAG automatically");
  std::println("  Total joint log-prob = {:.2f}", total_log_prob);

  // =========================================================================
  // FULL BAYESIAN INFERENCE VIA METROPOLIS-HASTINGS
  // =========================================================================
  std::println("\n=== FULL BAYESIAN INFERENCE VIA MCMC ===\n");

  // Joint posterior: p(α, β, θ[1..38] | data) ∝ p(α) p(β) Π p(θ[i]|α,β) Π p(k[i]|n[i],θ[i])
  //
  // State vector: [log(α), log(β), logit(θ[0]), ..., logit(θ[37])]
  // We use transformed parameters to enable unconstrained sampling.

  constexpr std::size_t kNumCountries = 38;
  constexpr std::size_t kStateDim = 2 + kNumCountries;  // α, β, θ[0..37]

  using State = std::array<double, kStateDim>;

  // =========================================================================
  // HYPERPRIORS: Gamma(shape, rate) on α and β
  // =========================================================================
  //
  // We need priors on the Beta distribution's parameters α and β.
  // Gamma is a natural choice: positive support, flexible shape.
  //
  // Gamma(shape=k, rate=λ) has:
  //   Mean = k/λ          → 2/0.1 = 20
  //   Mode = (k-1)/λ      → 1/0.1 = 10  (for k ≥ 1)
  //   Std  = √(k/λ²)      → √200 ≈ 14
  //   95% interval ≈ [1, 55]  (roughly)
  //
  // Why shape=2, rate=0.1?
  //
  //   1. WEAKLY INFORMATIVE: Mean 20 with std 14 covers a wide range.
  //      This lets the data determine the true values.
  //
  //   2. CONCENTRATION PRIOR: E[α + β] ≈ 40 pseudo-observations.
  //      Not too tight (would force countries to be identical) nor
  //      too loose (would give no regularization).
  //
  //   3. MEAN PRIOR: E[α/(α+β)] ≈ 0.5, but with huge variance.
  //      This is intentionally uninformative about the mean rate.
  //      The data overwhelms this prior.
  //
  //   4. MODE AT 10: The prior gently favors moderate values of α, β
  //      over very small values (which would make Beta nearly uniform)
  //      or very large values (which would concentrate all rates tightly).
  //
  // Alternative: Gamma(1, 0.1) would be exponential (mode at 0).
  //              Gamma(0.1, 0.1) would be nearly improper.
  //              Gamma(10, 0.5) would be more informative (mean=20, std≈4.5).
  //
  // The priors are already encoded in the symbolic model (alpha_hyper, beta_hyper).
  // We use them via full_joint_lp which computes the complete joint log-prob.

  // Log-posterior function (manual computation)
  // State: [log_alpha, log_beta, logit_theta[0], ..., logit_theta[37]]
  //
  // TODO: The symbolic full_joint_lp should work here, but there's a bug
  // in the indexed evaluation. For now, compute manually.
  auto log_posterior = [&](const State& state) -> double {
    double log_alpha = state[0];
    double log_beta = state[1];
    double alpha_val = std::exp(log_alpha);
    double beta_val = std::exp(log_beta);

    // Reject invalid hyperparameters
    if (alpha_val < 1e-6 || beta_val < 1e-6 || alpha_val > 1e6 || beta_val > 1e6) {
      return -std::numeric_limits<double>::infinity();
    }

    // Prior on α, β (+ Jacobian for log transform)
    double lp = prob::logGamma(alpha_val, 2.0, 0.1) + log_alpha;
    lp += prob::logGamma(beta_val, 2.0, 0.1) + log_beta;

    // For each country: p(θ[i] | α, β) and p(k[i] | n[i], θ[i])
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double logit_theta = state[2 + i];
      double theta_i = 1.0 / (1.0 + std::exp(-logit_theta));  // sigmoid

      // Reject invalid theta
      if (theta_i < 1e-10 || theta_i > 1.0 - 1e-10) {
        return -std::numeric_limits<double>::infinity();
      }

      // Prior on θ[i] (+ Jacobian for logit transform)
      // d(theta)/d(logit_theta) = theta * (1 - theta)
      double jacobian = theta_i * (1.0 - theta_i);
      lp += prob::logBeta(theta_i, alpha_val, beta_val) + std::log(jacobian);

      // Likelihood: k[i] ~ Binomial(n[i], θ[i])
      lp += prob::logBinomial(k_values[i], n_values[i], theta_i);
    }

    return lp;
  };

  // Initialize state at reasonable values
  State initial_state{};
  initial_state[0] = std::log(3.0);   // log(α) ≈ log(3)
  initial_state[1] = std::log(17.0);  // log(β) ≈ log(17)
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    double raw_rate = k_values[i] / n_values[i];
    raw_rate = std::clamp(raw_rate, 0.01, 0.99);
    initial_state[2 + i] = std::log(raw_rate / (1.0 - raw_rate));  // logit
  }

  // Gaussian random walk proposal
  bayes::GaussianWalkND<double, kStateDim> proposal{0.05};

  auto mh_kernel = bayes::makeMetropolisHastings<State>(log_posterior, proposal);

  std::println("Running Metropolis-Hastings MCMC...");
  std::println("  Parameters: {} (α, β, θ[1..38])", kStateDim);
  std::println("  Priors: α ~ Gamma(2, 0.1), β ~ Gamma(2, 0.1)");

  constexpr std::size_t kMcmcBurnIn = 5000;
  constexpr std::size_t kMcmcSamples = 10000;
  constexpr std::size_t kMcmcThin = 5;

  std::println("  Burn-in: {}, Samples: {}, Thinning: {}", kMcmcBurnIn, kMcmcSamples, kMcmcThin);

  bayes::Chain chain{mh_kernel, rng, initial_state};
  chain.advance(kMcmcBurnIn);
  auto [mcmc_samples, acceptance_rate] = chain.takeWithStats(kMcmcSamples, kMcmcThin);

  std::println("  Acceptance rate: {:.1f}%\n", acceptance_rate * 100);

  // Extract posterior summaries
  std::vector<double> alpha_samples;
  std::vector<double> beta_samples;
  std::vector<std::vector<double>> theta_samples(kNumCountries);

  for (const auto& s : mcmc_samples) {
    alpha_samples.push_back(std::exp(s[0]));
    beta_samples.push_back(std::exp(s[1]));
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double logit_theta = s[2 + i];
      double theta_i = 1.0 / (1.0 + std::exp(-logit_theta));
      theta_samples[i].push_back(theta_i);
    }
  }

  // Compute posterior means
  auto mean = [](const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
  };
  auto quantile = [](std::vector<double> v, double q) {
    std::ranges::sort(v);
    auto idx = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1));
    return v[idx];
  };

  double alpha_mean = mean(alpha_samples);
  double beta_mean = mean(beta_samples);
  double alpha_lo = quantile(alpha_samples, 0.025);
  double alpha_hi = quantile(alpha_samples, 0.975);
  double beta_lo = quantile(beta_samples, 0.025);
  double beta_hi = quantile(beta_samples, 0.975);

  std::println("=== INFERRED HYPERPARAMETERS ===\n");
  std::println("  α (concentration on success):");
  std::println("    Posterior mean: {:.2f}", alpha_mean);
  std::println("    95%% CI: [{:.2f}, {:.2f}]", alpha_lo, alpha_hi);
  std::println("  β (concentration on failure):");
  std::println("    Posterior mean: {:.2f}", beta_mean);
  std::println("    95%% CI: [{:.2f}, {:.2f}]", beta_lo, beta_hi);

  double implied_mean = alpha_mean / (alpha_mean + beta_mean);
  double implied_conc = alpha_mean + beta_mean;
  std::println("\n  Implied population distribution: Beta({:.1f}, {:.1f})", alpha_mean, beta_mean);
  std::println("    Mean weekend rate: {:.1f}%%", implied_mean * 100);
  std::println("    Concentration: {:.0f} (higher = less between-country variation)", implied_conc);

  // Update theta_posterior_means with MCMC results
  std::println("\n=== MCMC vs CONJUGATE POSTERIOR MEANS ===\n");
  std::println("{:18} {:>10} {:>10} {:>10}", "Country", "Raw %", "Conjugate", "MCMC");
  std::println("{:-<55}", "");

  std::vector<double> mcmc_theta_means(kNumCountries);
  for (std::size_t i = 0; i < kNumCountries && i < 10; ++i) {
    mcmc_theta_means[i] = mean(theta_samples[i]);
    double raw = k_values[i] / n_values[i];
    std::println("{:18} {:9.1f}% {:9.1f}% {:9.1f}%",
                 countries[i].name.substr(0, 18),
                 raw * 100,
                 theta_posterior_means[i] * 100,
                 mcmc_theta_means[i] * 100);
  }
  std::println("  ... ({} more countries)", kNumCountries - 10);

  // =========================================================================
  // SHRINKAGE VISUALIZATION
  // =========================================================================
  std::println("\n=== SHRINKAGE: RAW RATE vs PARTIAL-POOLING ESTIMATE ===\n");

  struct Estimate {
    std::string name;
    int64_t n;
    double raw_rate;
    double partial_pool_mean;
  };
  std::vector<Estimate> estimates;

  for (std::size_t i = 0; i < countries.size(); ++i) {
    const auto& c = countries[i];
    double raw = static_cast<double>(c.weekend) / c.total;
    estimates.push_back({c.name, c.total, raw, theta_posterior_means[i]});
  }

  // Create scatter plot of raw vs pooled estimates
  std::vector<Point> shrinkage_points;
  for (const auto& e : estimates) {
    shrinkage_points.push_back({e.raw_rate, e.partial_pool_mean});
  }

  DensityPlotOptions scatter_opts;
  scatter_opts.width = 50;
  scatter_opts.height = 20;
  scatter_opts.min_x = 0.05;
  scatter_opts.max_x = 0.35;
  scatter_opts.min_y = 0.05;
  scatter_opts.max_y = 0.35;
  scatter_opts.title = "Raw Rate (x) vs Pooled Estimate (y)";
  scatter_opts.low_color = colors::kBlue;
  scatter_opts.high_color = colors::kYellow;

  std::print("{}", scatterPlot(shrinkage_points, 50, 20, colors::kCyan));

  std::println(R"(
If there were no shrinkage, all points would lie on the diagonal (x=y).
Points BELOW the diagonal: rate shrunk DOWN toward global mean ({:.1f}%)
Points ABOVE the diagonal: rate shrunk UP toward global mean
)", global_rate * 100);

  // =========================================================================
  // TABLE OF RESULTS
  // =========================================================================
  std::println("\n=== WEEKEND SUBMISSION PROBABILITIES ===\n");
  std::println("{:18} {:>6} {:>6}  {:>8}  {:>12}  {:>10}",
               "Country", "n", "k", "Raw %", "Pooled %", "Shrinkage");
  std::println("{:-<70}", "");

  std::ranges::sort(estimates, [](const auto& a, const auto& b) {
    return a.partial_pool_mean > b.partial_pool_mean;
  });

  for (const auto& e : estimates) {
    double shrinkage = (e.raw_rate - e.partial_pool_mean) * 100;
    std::println("{:18} {:6} {:6}  {:7.1f}%  {:11.1f}%  {:+9.2f}pp",
                 e.name.substr(0, 18), e.n,
                 static_cast<int64_t>(e.raw_rate * e.n),
                 e.raw_rate * 100, e.partial_pool_mean * 100, shrinkage);
  }

  // =========================================================================
  // KEY INSIGHTS
  // =========================================================================
  std::println(R"(

=== KEY INSIGHTS ===

1. HIERARCHICAL PRIOR: Beta({:.1f}, {:.1f})
   - Encodes our PRIOR BELIEFS about weekend submission rates
   - Mean: {:.1f}% (lower than random 2/7 ≈ 29% because weekdays are preferred)
   - Concentration: {:.0f} (weak prior - about {:.0f} "pseudo-observations")
   - NOTE: Full Bayesian inference would put priors on α, β and sample jointly

2. SHRINKAGE MAGNITUDE depends on sample size:
   - Large n (UK, China): < 1pp shrinkage - data dominates prior
   - Medium n (Japan, Taiwan): 1-3pp shrinkage - balance of prior and data
   - Small n (Cameroon, Bangladesh): larger shrinkage - prior dominates

3. WHY PARTIAL-POOLING IS BETTER:
   - Extreme estimates from small samples are likely noise
   - Borrowing strength from other countries regularizes estimates
   - Predictions for new countries start at the prior mean, not 50%%

4. BAYESIAN VS FREQUENTIST:
   - This example uses conjugate Beta-Binomial updating (analytical)
   - Full treatment: put Gamma priors on α, β and use HMC
   - The bayes::sym::Joint DSL can compute gradients for HMC automatically
)", alpha_prior, beta_prior,
     alpha_prior / (alpha_prior + beta_prior) * 100,
     alpha_prior + beta_prior, alpha_prior + beta_prior);

  return 0;
}
