// BMJ Weekend Submissions Analysis
// Statistical Rethinking 2025 - Hierarchical Models (B01)
// Data: BMJSubmissions.csv (doi:10.1136/bmj.l6460)
//
// Models the probability of weekend submission using a logit-linear model:
//   W ~ Bernoulli(p)
//   logit(p) = a + b[L]
//
// Compares:
//   m1: No-pooling     - b[L] ~ Normal(0, 0.5) fixed
//   m2: Partial-pooling - b[L] ~ Normal(0, sigma), sigma ~ Exponential(1)
//
// Demonstrates shrinkage toward the global mean for countries with small samples.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "bayes/estimation/metropolis_hastings.h"
#include "csv.h"
#include "plot.h"

using namespace tempura;

struct CountryData {
  std::string name;
  int64_t country_id;
  int64_t total;
  int64_t weekend;
};

auto invLogit(double x) -> double { return 1.0 / (1.0 + std::exp(-x)); }

// Parse CSV and aggregate by country
auto loadData(const std::string& path) -> std::vector<CountryData> {
  std::ifstream file{path};
  if (!file) {
    std::println(stderr, "Error: Cannot open {}", path);
    return {};
  }

  CsvReader reader{file};
  reader.skipRows(1);

  std::vector<CountryData> countries(39);  // IDs 1-38

  while (reader.nextRow()) {
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

  return result;
}

// Compute mean and percentile interval from samples
struct Summary {
  double mean;
  double lo;  // 5.5th percentile (89% interval)
  double hi;  // 94.5th percentile
};

auto summarize(std::vector<double> samples) -> Summary {
  double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
  double mean = sum / static_cast<double>(samples.size());

  std::ranges::sort(samples);
  auto idx_lo = static_cast<std::size_t>(0.055 * static_cast<double>(samples.size() - 1));
  auto idx_hi = static_cast<std::size_t>(0.945 * static_cast<double>(samples.size() - 1));

  return {mean, samples[idx_lo], samples[idx_hi]};
}

auto main() -> int {
  // Use black for inverted fg - works on dark terminals
  // (Skip queryTerminalBackground() - OSC 11 interferes with terminal multiplexers like Zellij)
  constexpr RGB terminal_bg{0, 0, 0};

  std::mt19937_64 rng{42};

  std::println(R"(
=== BMJ WEEKEND SUBMISSIONS ANALYSIS ===
Statistical Rethinking 2025 - Homework B01

Data: 49,464 article submissions to The BMJ (2012-2018)
Question: What is the probability that authors from each country submit on weekends?
)");

  auto countries = loadData("examples/stat_rethinking/BMJSubmissions.csv");
  if (countries.empty()) return 1;

  std::println("Loaded {} countries\n", countries.size());

  // Sort by country ID for consistent indexing
  std::ranges::sort(countries, [](const auto& a, const auto& b) {
    return a.country_id < b.country_id;
  });

  const std::size_t kNumCountries = countries.size();

  // =========================================================================
  // PRIOR PREDICTIVE SIMULATION
  // =========================================================================
  std::println(R"(
=== PRIOR PREDICTIVE SIMULATION ===

What do our priors imply about weekend submission probabilities?

If submission timing were random, P(weekend) = 2/7 ≈ 0.14
So we expect rates BELOW this (people prefer weekdays for work).

Model: logit(p) = a + b[L]
  a ~ Normal(-2, 1)     # global intercept
  b[L] ~ Normal(0, 0.5) # country offset

inv_logit(-2) ≈ 0.12, so prior centers near the random baseline.
)");

  std::normal_distribution<double> prior_a{-2.0, 1.0};
  std::normal_distribution<double> prior_b{0.0, 0.5};

  // Generate 12 prior simulations - horizontal sparkline plot
  constexpr int kNumSims = 12;
  constexpr int kNumBins = 50;          // Probability bins (0-50% in 1% increments)
  constexpr double kBinWidth = 0.01;    // 1% per bin

  std::array<std::array<int, kNumBins>, kNumSims> histograms{};

  for (int sim = 0; sim < kNumSims; ++sim) {
    double a = prior_a(rng);
    for (std::size_t j = 0; j < kNumCountries; ++j) {
      double b = prior_b(rng);
      double p = invLogit(a + b);
      auto bin = static_cast<int>(p / kBinWidth);
      if (bin >= 0 && bin < kNumBins) {
        histograms[sim][bin]++;
      }
    }
  }

  // Find max count for scaling
  int max_count = 1;
  for (const auto& hist : histograms) {
    for (int count : hist) {
      max_count = std::max(max_count, count);
    }
  }

  // Symmetric violin: upper row grows DOWN, lower row grows UP
  // Uses fg/bg color inversion for upper row to make blocks "fill from top"
  // Eighth blocks ▁▂▃▄▅▆▇█ index 1-8 fill from bottom
  constexpr const char* kEighths[] = {" ", "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

  // terminal_bg was queried at program start to avoid OSC interference

  std::println("Prior predictive violin (12 simulations, 38 countries each):\n");
  std::println("     │0%%       10%%       20%%       30%%       40%%       50%%");
  std::println("     │|          |          |          |          |          |");

  for (int sim = 0; sim < kNumSims; ++sim) {
    std::string upper_row;
    std::string lower_row;

    for (int bin = 0; bin < kNumBins; ++bin) {
      int count = histograms[sim][bin];

      if (count == 0) {
        upper_row += " ";
        lower_row += " ";
      } else {
        // Scale to 1-8 levels per row
        int h = 1 + (count - 1) * 7 / max_count;
        h = std::clamp(h, 1, 8);

        // Color intensity based on density
        double t = static_cast<double>(count) / max_count;
        RGB color{
            static_cast<uint8_t>(100 + 155 * t),
            static_cast<uint8_t>(200 + 55 * t),
            static_cast<uint8_t>(255 * (1.0 - 0.6 * t))
        };

        // Upper row: normal - blocks fill UP from bottom (away from center)
        upper_row += color.wrap(kEighths[h]);

        // Lower row: INVERTED colors + complementary block
        // Makes blocks appear to fill DOWN from top (away from center)
        // height h → use block (8-h) with fg=terminal_bg, bg=color
        // h=1 → ▇ inverted = 1/8 from top (small, close to center)
        // h=7 → ▁ inverted = 7/8 from top (large, extends down away from center)
        // h=8 → █ with normal colors (full block)
        if (h == 8) {
          lower_row += color.wrap(kEighths[8]);
        } else {
          lower_row += terminal_bg.wrapFgBg(kEighths[8 - h], color);
        }
      }
    }

    // Upper row (bars grow up), then lower row (bars grow down)
    std::println("sim{:2}│{}\033[0m", sim + 1, upper_row);
    std::println("     │{}\033[0m", lower_row);
  }

  std::println(R"(
Symmetric violin: bars grow AWAY from center (outward bulge = higher density).
Lower row uses inverted fg/bg colors to make blocks fill from top down.

Most draws center around 10-20%%. Some (like sim 5) cluster tightly when
the global intercept 'a' happens to be very negative.
)");

  // =========================================================================
  // MODEL SETUP
  // =========================================================================
  // State layout: [a, b[0], b[1], ..., b[37]]
  // For partial-pooling: [a, sigma, b[0], ..., b[37]]

  constexpr std::size_t kNoPoolDim = 1 + 38;      // a + 38 country offsets
  constexpr std::size_t kPartialPoolDim = 2 + 38; // a + sigma + 38 offsets

  using NoPoolState = std::array<double, kNoPoolDim>;
  using PartialPoolState = std::array<double, kPartialPoolDim>;

  // Prepare observed data
  std::vector<int64_t> n_obs(kNumCountries);
  std::vector<int64_t> k_obs(kNumCountries);
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    n_obs[i] = countries[i].total;
    k_obs[i] = countries[i].weekend;
  }

  // =========================================================================
  // MODEL 1: NO-POOLING
  // =========================================================================
  // W ~ Bernoulli(p)
  // logit(p) = a + b[L]
  // a ~ Normal(-2, 1)
  // b[L] ~ Normal(0, 0.5)  <- FIXED sigma

  auto log_posterior_no_pool = [&](const NoPoolState& state) {
    double a = state[0];
    double lp = 0.0;

    // Prior on a: Normal(-2, 1)
    lp += -0.5 * (a + 2.0) * (a + 2.0);

    // Prior on b[L]: Normal(0, 0.5)
    constexpr double sigma_fixed = 0.5;
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double b_i = state[1 + i];
      lp += -0.5 * (b_i / sigma_fixed) * (b_i / sigma_fixed);
    }

    // Likelihood: Binomial(k | n, p) where logit(p) = a + b[i]
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double b_i = state[1 + i];
      double logit_p = a + b_i;
      double p = invLogit(logit_p);

      // log Binomial(k | n, p) = k*log(p) + (n-k)*log(1-p) + const
      double k = static_cast<double>(k_obs[i]);
      double n = static_cast<double>(n_obs[i]);
      lp += k * std::log(p + 1e-10) + (n - k) * std::log(1.0 - p + 1e-10);
    }

    return lp;
  };

  // =========================================================================
  // MODEL 2: PARTIAL-POOLING
  // =========================================================================
  // W ~ Bernoulli(p)
  // logit(p) = a + b[L]
  // a ~ Normal(-2, 1)
  // b[L] ~ Normal(0, sigma)
  // sigma ~ Exponential(1)

  auto log_posterior_partial_pool = [&](const PartialPoolState& state) {
    double a = state[0];
    double log_sigma = state[1];  // Sample in log space for positivity
    double sigma = std::exp(log_sigma);
    double lp = 0.0;

    // Prior on a: Normal(-2, 1)
    lp += -0.5 * (a + 2.0) * (a + 2.0);

    // Prior on sigma: Exponential(1) with Jacobian for log transform
    // log p(sigma) = -sigma, Jacobian = sigma
    // log p(log_sigma) = -exp(log_sigma) + log_sigma
    lp += -sigma + log_sigma;

    // Prior on b[L]: Normal(0, sigma)
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double b_i = state[2 + i];
      lp += -0.5 * (b_i / sigma) * (b_i / sigma) - std::log(sigma);
    }

    // Likelihood
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double b_i = state[2 + i];
      double logit_p = a + b_i;
      double p = invLogit(logit_p);

      double k = static_cast<double>(k_obs[i]);
      double n = static_cast<double>(n_obs[i]);
      lp += k * std::log(p + 1e-10) + (n - k) * std::log(1.0 - p + 1e-10);
    }

    return lp;
  };

  // =========================================================================
  // MCMC SAMPLING
  // =========================================================================
  std::println("=== FITTING MODELS VIA MCMC ===\n");

  constexpr std::size_t kBurnIn = 5000;
  constexpr std::size_t kSamples = 10000;
  constexpr std::size_t kThin = 5;

  // --- Model 1: No-pooling ---
  std::println("Model 1 (no-pooling): a ~ Normal(-2,1), b[L] ~ Normal(0, 0.5)");

  NoPoolState init_no_pool{};
  init_no_pool[0] = -2.0;  // a
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    init_no_pool[1 + i] = 0.0;  // b[i]
  }

  bayes::GaussianWalkND<double, kNoPoolDim> proposal_no_pool{0.02};
  auto mh_no_pool = bayes::makeMetropolisHastings<NoPoolState>(
      log_posterior_no_pool, proposal_no_pool);

  bayes::Chain chain_no_pool{mh_no_pool, rng, init_no_pool};
  chain_no_pool.advance(kBurnIn);
  auto [samples_no_pool, accept_no_pool] = chain_no_pool.takeWithStats(kSamples, kThin);
  std::println("  Acceptance rate: {:.1f}%", accept_no_pool * 100);

  // --- Model 2: Partial-pooling ---
  std::println("Model 2 (partial-pooling): a ~ Normal(-2,1), b[L] ~ Normal(0,sigma), sigma ~ Exp(1)");

  PartialPoolState init_partial{};
  init_partial[0] = -2.0;              // a
  init_partial[1] = std::log(0.5);     // log(sigma)
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    init_partial[2 + i] = 0.0;  // b[i]
  }

  bayes::GaussianWalkND<double, kPartialPoolDim> proposal_partial{0.02};
  auto mh_partial = bayes::makeMetropolisHastings<PartialPoolState>(
      log_posterior_partial_pool, proposal_partial);

  bayes::Chain chain_partial{mh_partial, rng, init_partial};
  chain_partial.advance(kBurnIn);
  auto [samples_partial, accept_partial] = chain_partial.takeWithStats(kSamples, kThin);
  std::println("  Acceptance rate: {:.1f}%\n", accept_partial * 100);

  // =========================================================================
  // EXTRACT POSTERIOR PREDICTIONS
  // =========================================================================
  // For each country, compute p = inv_logit(a + b[i]) for each MCMC sample

  std::vector<std::vector<double>> p_samples_no_pool(kNumCountries);
  std::vector<std::vector<double>> p_samples_partial(kNumCountries);

  for (const auto& s : samples_no_pool) {
    double a = s[0];
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double p = invLogit(a + s[1 + i]);
      p_samples_no_pool[i].push_back(p);
    }
  }

  for (const auto& s : samples_partial) {
    double a = s[0];
    for (std::size_t i = 0; i < kNumCountries; ++i) {
      double p = invLogit(a + s[2 + i]);
      p_samples_partial[i].push_back(p);
    }
  }

  // Compute summaries
  std::vector<Summary> summary_no_pool(kNumCountries);
  std::vector<Summary> summary_partial(kNumCountries);

  for (std::size_t i = 0; i < kNumCountries; ++i) {
    summary_no_pool[i] = summarize(p_samples_no_pool[i]);
    summary_partial[i] = summarize(p_samples_partial[i]);
  }

  // Global mean from partial-pooling model
  std::vector<double> global_mean_samples;
  for (const auto& s : samples_partial) {
    global_mean_samples.push_back(invLogit(s[0]));
  }
  auto global_summary = summarize(global_mean_samples);

  // Sigma posterior
  std::vector<double> sigma_samples;
  for (const auto& s : samples_partial) {
    sigma_samples.push_back(std::exp(s[1]));
  }
  auto sigma_summary = summarize(sigma_samples);

  std::println("=== INFERRED PARAMETERS ===\n");
  std::println("Global intercept (partial-pooling):");
  std::println("  a: mean inv_logit(a) = {:.1f}% [{:.1f}%, {:.1f}%]",
               global_summary.mean * 100, global_summary.lo * 100, global_summary.hi * 100);
  std::println("\nBetween-country variation:");
  std::println("  sigma: {:.3f} [{:.3f}, {:.3f}]",
               sigma_summary.mean, sigma_summary.lo, sigma_summary.hi);

  // =========================================================================
  // COUNTRY RANKINGS
  // =========================================================================
  std::println("\n=== COUNTRY RANKINGS (sorted by partial-pooling estimate) ===\n");

  struct CountryResult {
    std::string name;
    int64_t n;
    double raw_rate;
    Summary no_pool;
    Summary partial;
  };

  std::vector<CountryResult> results;
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    double raw = static_cast<double>(k_obs[i]) / static_cast<double>(n_obs[i]);
    results.push_back({
        countries[i].name,
        n_obs[i],
        raw,
        summary_no_pool[i],
        summary_partial[i]
    });
  }

  // Sort by partial-pooling mean (descending)
  std::ranges::sort(results, [](const auto& a, const auto& b) {
    return a.partial.mean > b.partial.mean;
  });

  std::println("{:18} {:>6}  {:>22}  {:>22}",
               "Country", "n", "No-pooling (89% CI)", "Partial-pooling (89% CI)");
  std::println("{:-<75}", "");

  for (const auto& r : results) {
    std::println("{:18} {:6}  {:5.1f}% [{:4.1f}%, {:4.1f}%]  {:5.1f}% [{:4.1f}%, {:4.1f}%]",
                 r.name.substr(0, 18), r.n,
                 r.no_pool.mean * 100, r.no_pool.lo * 100, r.no_pool.hi * 100,
                 r.partial.mean * 100, r.partial.lo * 100, r.partial.hi * 100);
  }

  // =========================================================================
  // SHRINKAGE VISUALIZATION
  // =========================================================================
  std::println("\n=== SHRINKAGE: HOW PARTIAL-POOLING SHIFTS ESTIMATES ===\n");
  std::println("Countries with smaller samples get pulled more toward the global mean.\n");

  std::println("{:18} {:>10} {:>12} {:>12}",
               "Country", "log(n)", "Shift (pp)", "Direction");
  std::println("{:-<55}", "");

  // Sort by sample size for this view
  std::vector<CountryResult> by_size = results;
  std::ranges::sort(by_size, [](const auto& a, const auto& b) {
    return a.n < b.n;
  });

  for (const auto& r : by_size) {
    double shift = (r.partial.mean - r.no_pool.mean) * 100;
    std::string direction = "~";
    if (shift < -0.1) {
      direction = "<- shrunk";
    } else if (shift > 0.1) {
      direction = "-> expanded";
    }
    std::println("{:18} {:10.2f} {:+11.2f} {}",
                 r.name.substr(0, 18), std::log(static_cast<double>(r.n)), shift, direction);
  }

  std::println(R"(
Key observations:
- Countries with small n (left side of log scale) show larger shifts
- Most shifts are NEGATIVE (toward the mean) for countries above average
- Countries with large n (UK, China, India) barely shift - data dominates
)");

  // =========================================================================
  // VISUAL: CREDIBLE INTERVALS
  // =========================================================================
  std::println("\n=== CREDIBLE INTERVAL COMPARISON (Top 10 and Bottom 5) ===\n");
  std::println("Bars show 89%% credible intervals. Wider = more uncertainty.\n");

  std::println("         5%%       10%%       15%%       20%%       25%%       30%%       35%%");
  std::println("         |         |         |         |         |         |         |");

  constexpr double x_min = 0.05;
  constexpr double x_max = 0.35;
  constexpr int bar_width = 60;

  // Top 10
  for (std::size_t i = 0; i < 10 && i < results.size(); ++i) {
    const auto& r = results[i];
    std::println("\n{} (n={}):", r.name, r.n);
    std::print("  No-pool:  {}  {:.1f}%%\n",
               credibleIntervalBar(r.no_pool.lo, r.no_pool.hi, r.no_pool.mean,
                                   x_min, x_max, bar_width, colors::kCyan),
               r.no_pool.mean * 100);
    std::print("  Partial:  {}  {:.1f}%%\n",
               credibleIntervalBar(r.partial.lo, r.partial.hi, r.partial.mean,
                                   x_min, x_max, bar_width, colors::kYellow),
               r.partial.mean * 100);
  }

  std::println("\n  ... (middle countries omitted) ...");

  // Bottom 5
  for (std::size_t i = results.size() - 5; i < results.size(); ++i) {
    const auto& r = results[i];
    std::println("\n{} (n={}):", r.name, r.n);
    std::print("  No-pool:  {}  {:.1f}%%\n",
               credibleIntervalBar(r.no_pool.lo, r.no_pool.hi, r.no_pool.mean,
                                   x_min, x_max, bar_width, colors::kCyan),
               r.no_pool.mean * 100);
    std::print("  Partial:  {}  {:.1f}%%\n",
               credibleIntervalBar(r.partial.lo, r.partial.hi, r.partial.mean,
                                   x_min, x_max, bar_width, colors::kYellow),
               r.partial.mean * 100);
  }

  // =========================================================================
  // KEY INSIGHTS
  // =========================================================================
  std::println(R"(

=== KEY INSIGHTS ===

1. PRIOR CHOICE MATTERS
   - Random submission: P(weekend) = 2/7 ≈ 14%%
   - Prior a ~ Normal(-2, 1) centers at inv_logit(-2) ≈ 12%%
   - This encodes belief that weekend submission is uncommon

2. NO-POOLING vs PARTIAL-POOLING
   - No-pooling: Each country estimated independently
   - Partial-pooling: Countries share information via learned sigma
   - Sigma ≈ {:.2f} tells us how much countries truly vary

3. SHRINKAGE PATTERN
   - Small samples (Cameroon, Bangladesh): Large shrinkage toward mean
   - Large samples (UK, India, China): Almost no shrinkage
   - This is ADAPTIVE regularization - the model learns how much to trust each estimate

4. INTERPRETATION
   - Top: Cameroon, Bangladesh, Turkey (but wide intervals!)
   - Bottom: India, Norway, Denmark (narrow intervals, confident)
   - China is precisely estimated AND high - genuine cultural difference?

5. WHY PARTIAL-POOLING IS BETTER
   - Extreme estimates from small samples are likely noise
   - Borrowing strength across countries improves predictions
   - Predictions for NEW countries start at learned mean, not 50%%
)", sigma_summary.mean);

  return 0;
}
