// Hierarchical Model with Cross-Plate Indexing via gather()
// Demonstrates the pattern needed for Bangladesh contraception model.
//
// Model:
//   a_bar ~ Normal(0, 1)           # Global intercept
//   sigma ~ Exponential(1)         # Between-group SD
//   z[g] ~ Normal(0, 1) for g in Groups   # Non-centered effects
//   a[g] = a_bar + sigma * z[g]    # Group intercepts (deterministic)
//
//   group_idx[i] is which group observation i belongs to
//   y[i] ~ Bernoulli(logistic(a[group_idx[i]])) for i in Observations
//
// This tests:
//   1. gather() for cross-plate indexing in likelihood
//   2. Integration with inferExplicit() and sample()
//   3. Correct handling of scalar + indexed params

#include <algorithm>
#include <cmath>
#include <print>
#include <random>
#include <vector>

#include "symbolic4/indexed/data.h"
#include "symbolic4/infer.h"
#include "symbolic4/mcmc/samples.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto invLogit(double x) -> double { return 1.0 / (1.0 + std::exp(-x)); }

auto main() -> int {
  std::println("Starting hierarchical_gather_example...");
  std::mt19937_64 rng{42};
  std::println("RNG initialized");

  std::println(R"(
=== HIERARCHICAL MODEL WITH GATHER ===

Model: logit(p[i]) = a[group_idx[i]]
       a[g] = a_bar + sigma * z[g]

  a_bar ~ Normal(0, 1)
  sigma ~ Exponential(1)
  z[g] ~ Normal(0, 1)
  y[i] ~ Bernoulli(p[i])

This demonstrates cross-plate indexing: observations are nested in groups,
and we use gather() to index into group-level parameters.
)");

  // ========================================================================
  // Simulate data
  // ========================================================================
  constexpr std::size_t kNumGroups = 8;
  constexpr std::size_t kNumObs = 200;

  // True parameters
  double true_a_bar = -0.5;
  double true_sigma = 0.8;

  // Generate true group effects
  std::normal_distribution<double> z_dist(0.0, 1.0);
  std::vector<double> true_z(kNumGroups);
  std::vector<double> true_a(kNumGroups);
  for (std::size_t g = 0; g < kNumGroups; ++g) {
    true_z[g] = z_dist(rng);
    true_a[g] = true_a_bar + true_sigma * true_z[g];
  }

  // Generate observations with random group assignments
  std::uniform_int_distribution<int> group_dist(0, static_cast<int>(kNumGroups) - 1);
  std::uniform_real_distribution<double> uniform(0.0, 1.0);

  std::vector<double> group_idx(kNumObs);
  std::vector<double> y_obs(kNumObs);
  std::vector<int> group_counts(kNumGroups, 0);
  std::vector<int> group_successes(kNumGroups, 0);

  for (std::size_t i = 0; i < kNumObs; ++i) {
    int g = group_dist(rng);
    group_idx[i] = static_cast<double>(g);
    double p = invLogit(true_a[g]);
    y_obs[i] = uniform(rng) < p ? 1.0 : 0.0;
    group_counts[g]++;
    if (y_obs[i] > 0.5) group_successes[g]++;
  }

  std::println("Generated {} observations across {} groups", kNumObs, kNumGroups);
  std::println("True a_bar: {:.2f}, sigma: {:.2f}\n", true_a_bar, true_sigma);

  std::println("{:>8} {:>8} {:>8} {:>8} {:>8}", "Group", "n", "success", "rate", "true_a");
  std::println("{:-<45}", "");
  for (std::size_t g = 0; g < kNumGroups; ++g) {
    double rate = group_counts[g] > 0
                      ? 100.0 * static_cast<double>(group_successes[g]) / static_cast<double>(group_counts[g])
                      : 0.0;
    std::println("{:8} {:8} {:8} {:7.1f}% {:8.2f}", g, group_counts[g], group_successes[g], rate, true_a[g]);
  }

  // ========================================================================
  // Define model using symbolic API with gather()
  // ========================================================================
  struct Groups {};
  struct Obs {};

  // Priors
  auto a_bar = normal(0_c, 1_c);
  auto sigma = exponential(1_c);

  // Non-centered parameterization: z[g] ~ N(0,1), a[g] = a_bar + sigma * z[g]
  auto z = plate(normal(0_c, 1_c), Groups{});

  // Data symbols - district gives which group each observation belongs to
  auto district = data(Obs{});
  auto n = data(Groups{});  // Not used in Bernoulli, but available

  // The key pattern: z[district] gives z[district[i]] for each observation
  // Then a[district[i]] = a_bar + sigma * z[district[i]]
  auto group_effect = a_bar + sigma * z[district];  // Natural bracket syntax!

  // Likelihood - plate over observations
  auto p = 1_c / (1_c + exp(-group_effect));
  auto y = plate(bernoulli(p), Obs{});

  // ========================================================================
  // Build posterior and sample
  // ========================================================================
  std::println("Building posterior with inferExplicit()...");

  auto posterior = inferExplicit(a_bar, sigma, z, y)
      .withDimension(Groups{}, kNumGroups)  // Must set Groups dimension explicitly
      .bind(district = indexed(group_idx), y = indexed(y_obs));

  std::println("State dimension: {} (2 scalar + {} indexed)", posterior.stateDim(), kNumGroups);

  // Test log-prob evaluation before HMC
  std::println("Testing log-prob evaluation...");
  std::vector<double> test_state(posterior.stateDim(), 0.0);
  // Set sigma (in unconstrained space, log(0.5) ≈ -0.69)
  test_state[1] = -0.69;
  double test_lp = posterior.logProb(test_state);
  std::println("Test log-prob: {:.4f}\n", test_lp);

  // Run HMC
  std::println("Running HMC: warmup 500, draws 2000...");

  std::vector<double> z_init(kNumGroups, 0.0);

  auto samples = posterior.sample(
      HmcConfig{.epsilon = 0.02, .steps = 15, .warmup = 500, .draws = 2000},
      BinderPack{a_bar = 0.0, sigma = 0.5, z = z_init},
      rng);

  std::println("Collected {} samples\n", samples.size());

  // ========================================================================
  // Posterior summaries
  // ========================================================================
  auto a_bar_draws = samples[a_bar];
  auto sigma_draws = samples[sigma];
  auto z_draws = samples[z];

  auto summarize = [](const std::vector<double>& s) {
    return std::make_tuple(mean(s), quantile(s, 0.055), quantile(s, 0.945));
  };

  auto [a_bar_mean, a_bar_lo, a_bar_hi] = summarize(a_bar_draws);
  auto [sigma_mean, sigma_lo, sigma_hi] = summarize(sigma_draws);

  std::println("=== POSTERIOR SUMMARIES ===\n");
  std::println("{:15} {:>10} {:>20} {:>10}", "Parameter", "Mean", "89% CI", "True");
  std::println("{:-<60}", "");
  std::println("{:15} {:10.2f} [{:6.2f}, {:6.2f}] {:10.2f}",
               "a_bar", a_bar_mean, a_bar_lo, a_bar_hi, true_a_bar);
  std::println("{:15} {:10.2f} [{:6.2f}, {:6.2f}] {:10.2f}",
               "sigma", sigma_mean, sigma_lo, sigma_hi, true_sigma);

  // Group-level effects
  std::println("\nGroup effects (z * sigma + a_bar):");
  std::println("{:>8} {:>10} {:>20} {:>10}", "Group", "Mean", "89% CI", "True");
  std::println("{:-<55}", "");

  std::size_t num_draws = a_bar_draws.size();
  for (std::size_t g = 0; g < kNumGroups; ++g) {
    std::vector<double> a_g(num_draws);
    for (std::size_t d = 0; d < num_draws; ++d) {
      a_g[d] = a_bar_draws[d] + sigma_draws[d] * z_draws[d, g];
    }
    auto [mean_g, lo_g, hi_g] = summarize(a_g);
    std::println("{:8} {:10.2f} [{:6.2f}, {:6.2f}] {:10.2f}", g, mean_g, lo_g, hi_g, true_a[g]);
  }

  std::println("\n=== MODEL WITH GATHER SUCCESSFUL ===\n");

  return 0;
}
