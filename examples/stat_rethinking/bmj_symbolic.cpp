// BMJ Weekend Submissions - Using Non-Centered Parameterization
// Statistical Rethinking 2025 - Homework B01
//
// Demonstrates:
// - Non-centered parameterization for hierarchical models
// - Indexed latent parameters with plate<>()
// - Automatic dynamic HMC for runtime-sized state
// - Symbol-indexed sample access with DynamicSamples
//
// Model (non-centered):
//   a ~ Normal(-2, 1)
//   sigma ~ Exponential(1)
//   z_b[L] ~ Normal(0, 1)        # Standardized effects
//   b[L] = sigma * z_b[L]        # Deterministic (not sampled)
//   logit(p[L]) = a + b[L]
//   k[L] ~ Binomial(n[L], p[L])

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "csv.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/infer.h"
#include "symbolic4/mcmc/samples.h"
#include "symbolic4/mcmc/sampler.h"

using namespace tempura;
using namespace tempura::symbolic4;

// ============================================================================
// Data
// ============================================================================

struct CountryData {
  std::string name;
  int64_t country_id;
  int64_t total;
  int64_t weekend;
};

auto loadData(const std::string& path) -> std::vector<CountryData> {
  std::ifstream file{path};
  if (!file) {
    std::println(stderr, "Error: Cannot open {}", path);
    return {};
  }

  CsvReader reader{file};
  reader.skipRows(1);

  std::vector<CountryData> countries(39);
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
    if (c.total > 0) result.push_back(c);
  }
  return result;
}

// ============================================================================
// Summary Statistics
// ============================================================================

struct Summary {
  double mean, lo, hi;
};

// Uses mean() and quantile() from symbolic4/mcmc/samples.h
auto summarize(const std::vector<double>& samples) -> Summary {
  return {mean(samples), quantile(samples, 0.055), quantile(samples, 0.945)};
}

auto invLogit(double x) -> double { return 1.0 / (1.0 + std::exp(-x)); }

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::mt19937_64 rng{42};

  std::println(R"(
=== BMJ WEEKEND SUBMISSIONS - NON-CENTERED PARAMETERIZATION ===

Model: logit(p[L]) = a + sigma * z_b[L]
  a ~ Normal(-2, 1)
  sigma ~ Exponential(1)
  z_b[L] ~ Normal(0, 1)        # Non-centered: standard normal
  b[L] = sigma * z_b[L]        # Deterministic transform
  k[L] ~ Binomial(n[L], p[L])
)");

  // Load data
  auto countries = loadData("examples/stat_rethinking/BMJSubmissions.csv");
  if (countries.empty()) return 1;

  std::ranges::sort(countries, [](const auto& l, const auto& r) {
    return l.country_id < r.country_id;
  });

  const std::size_t kNumCountries = countries.size();
  std::println("Loaded {} countries\n", kNumCountries);

  std::vector<double> k_obs(kNumCountries), n_obs(kNumCountries);
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    k_obs[i] = static_cast<double>(countries[i].weekend);
    n_obs[i] = static_cast<double>(countries[i].total);
  }

  // --------------------------------------------------------------------------
  // Define the model using non-centered parameterization
  // --------------------------------------------------------------------------
  struct Countries {};

  // Priors (scalar)
  auto a = normal(-2.0, 1.0);
  auto sigma = exponential(1.0);

  // Non-centered parameterization: z_b[L] ~ N(0, 1), b[L] = sigma * z_b[L]
  // This decouples the geometry of sigma from the country effects,
  // eliminating the "funnel" that makes centered parameterization hard to sample.
  auto z_b = plate<Countries>(normal(lit(0.0), lit(1.0)));

  // Data symbols
  auto n = data<Countries>();

  // Likelihood - plate over countries
  // p[i] = sigmoid(a + sigma * z_b[i])
  auto p = 1_c / (1_c + exp(-(a + sigma * z_b)));
  auto k = plate<Countries>(binomial(n, p));

  // Build posterior - explicitly list params since auto-discovery has issues
  // with mixed scalar + indexed params (see discoverParams in discover_params.h)
  auto posterior = inferExplicit(a, sigma, z_b, k)
      .bind(n = indexed(n_obs), k = indexed(k_obs));

  std::println("Model built with inferExplicit() + bind()");
  std::println("  State dimension: {} (2 scalar + {} indexed)",
               posterior.stateDim(), kNumCountries);

  // --------------------------------------------------------------------------
  // Run HMC sampling with dynamic HMC for indexed params
  // --------------------------------------------------------------------------
  std::println("\nRunning HMC: warmup 500, draws 2000...");

  // Init values in CONSTRAINED space — posterior applies inverse transforms internally.
  // For sigma (Positive support): user passes 0.5, posterior uses log(0.5) internally.
  // For z_b (Real support): identity transform, so 0.0 stays 0.0.
  std::vector<double> z_b_init(kNumCountries, 0.0);

  auto samples = posterior.sample(
      HmcConfig{.epsilon = 0.01, .steps = 20, .warmup = 500, .draws = 2000},
      BinderPack{a = -2.0, sigma = 0.5, z_b = z_b_init},
      rng);

  std::println("Collected {} samples\n", samples.size());

  // --------------------------------------------------------------------------
  // Posterior summaries using symbol-indexed access
  // --------------------------------------------------------------------------
  // samples[param] provides type-safe access to draws for each parameter:
  // - samples[a]     → vector<double> of all a draws
  // - samples[sigma] → vector<double> of all sigma draws
  // - samples[z_b]   → DynamicDense matrix (draws × countries)

  // Extract draws for scalar parameters
  auto a_draws = samples[a];
  auto sigma_draws = samples[sigma];
  std::size_t num_draws = a_draws.size();

  // Samples are already in constrained space (transforms applied during sampling)
  // sigma_draws are exp(z), a_draws are unconstrained (Real support)
  std::vector<double> global_mean_samples;
  global_mean_samples.reserve(num_draws);
  for (double a_val : a_draws) {
    global_mean_samples.push_back(invLogit(a_val));
  }

  auto global_summary = summarize(global_mean_samples);
  auto sigma_summary = summarize(sigma_draws);

  std::println("=== POSTERIOR ESTIMATES ===\n");
  std::println("Global intercept: inv_logit(a) = {:.1f}% [{:.1f}%, {:.1f}%]",
               global_summary.mean * 100, global_summary.lo * 100, global_summary.hi * 100);
  std::println("Between-country SD: sigma = {:.3f} [{:.3f}, {:.3f}]\n",
               sigma_summary.mean, sigma_summary.lo, sigma_summary.hi);

  // Extract indexed parameter draws
  auto z_b_draws = samples[z_b];  // matrix: draws × countries

  // Compute country-specific probabilities
  // b[L] = sigma * z_b[L], so p[L] = logistic(a + sigma * z_b[L])
  std::vector<std::vector<double>> p_samples(kNumCountries);
  for (std::size_t draw = 0; draw < num_draws; ++draw) {
    double a_val = a_draws[draw];
    double sigma_val = sigma_draws[draw];
    for (std::size_t country = 0; country < kNumCountries; ++country) {
      double z_b_val = z_b_draws[draw, country];
      double b_val = sigma_val * z_b_val;
      double prob = invLogit(a_val + b_val);
      p_samples[country].push_back(prob);
    }
  }

  struct Result {
    std::string name;
    int64_t n;
    Summary posterior;
  };

  std::vector<Result> results;
  for (std::size_t i = 0; i < kNumCountries; ++i) {
    results.push_back({countries[i].name, countries[i].total, summarize(p_samples[i])});
  }

  std::ranges::sort(results, [](const auto& l, const auto& r) {
    return l.posterior.mean > r.posterior.mean;
  });

  std::println("{:18} {:>6}  {:>22}", "Country", "n", "Posterior (89% CI)");
  std::println("{:-<50}", "");
  for (const auto& r : results) {
    std::println("{:18} {:6}  {:5.1f}% [{:4.1f}%, {:4.1f}%]",
                 r.name.substr(0, 18), r.n,
                 r.posterior.mean * 100, r.posterior.lo * 100, r.posterior.hi * 100);
  }

  return 0;
}
