// Statistical Rethinking 2025 - Week 1 Homework
// https://github.com/rmcelreath/stat_rethinking_2025/tree/main/homework
//
// Demonstrates symbolic4's compile-time probabilistic programming:
// - Declarative model specification with beta(), bernoulli()
// - sample() for ancestral sampling
// - logProb() for symbolic log-probability expressions

#include <algorithm>
#include <numeric>
#include <print>
#include <random>
#include <vector>

#include "bayes/binomial.h"  // For posterior predictive (symbolic4 lacks binomial RV)
#include "symbolic4/distributions/distributions.h"
#include "plot.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  std::mt19937_64 rng{42};
  constexpr int kSamples = 100'000;

  // ═══════════════════════════════════════════════════════════════════════════
  std::println(R"(
=== PROBLEM 1: THE RANDOM ALLOCATION GAME ===

Setup: Participants roll a die privately. If 4/5/6, they win 10 (self-reported).
Data:  171 participants, 111 claimed the prize (65%).
Question: What proportion are HONEST?

Generative reasoning - suppose 50% cheat:
  Cheats (50%): always claim        -> 50% claim
  Honest (50%): claim if roll >= 4  -> 25% claim
                              Total:  75% claim

We observed 65%, so fewer than 50% cheat. Try 30% cheats:
  0.30 x 1.0 + 0.70 x 0.5 = 65%

Model:
  h = proportion honest
  P(claim) = h x 0.5 + (1-h) x 1.0 = 1 - 0.5h
  Let p = P(claim), so h = 2(1-p)
  Posterior: p ~ Beta(112, 61)
)");

  constexpr int64_t n = 171;
  constexpr int64_t k = 111;

  // Define posterior using symbolic4 - conjugate Beta-Binomial update
  // Prior: Beta(1, 1) = Uniform
  // Posterior: Beta(1 + k, 1 + (n - k)) = Beta(112, 61)
  // k and n are constexpr, so the posterior parameters are compile-time constants:
  // Beta(1 + 111, 1 + (171 - 111)) = Beta(112, 61)
  auto p_posterior = beta(112.0_c, 61.0_c);

  // Show the log-probability expression (compile-time symbolic)
  auto lp = logProb(p_posterior);
  std::println("Log-probability expression (compile-time): log Beta(p | 112, 61)");
  std::println("  evaluated at p=0.65: {:.4f}\n", evaluate(lp, p_posterior = 0.65));

  // Sample h = 2(1-p) using symbolic4's sample
  std::vector<double> h_samples;
  h_samples.reserve(kSamples);
  for (int i = 0; i < kSamples; ++i) {
    double p = sample(p_posterior, rng);
    h_samples.push_back(2.0 * (1.0 - p));
  }
  std::ranges::sort(h_samples);

  double mean_h = std::reduce(h_samples.begin(), h_samples.end()) / kSamples;
  double ci_lo = h_samples[static_cast<size_t>(0.055 * kSamples)];
  double ci_hi = h_samples[static_cast<size_t>(0.945 * kSamples)];

  std::println("Result: h = {:.2f}  (89% CI: [{:.2f}, {:.2f}])\n", mean_h, ci_lo,
               ci_hi);
  std::print("{}", generateTextHistogram(
                       h_samples, {.bins = 20,
                                   .width = 50,
                                   .color = colors::kCyan,
                                   .min_x = 0.4,
                                   .max_x = 1.0}));

  // ═══════════════════════════════════════════════════════════════════════════
  std::println(R"(

=== PROBLEM 2: POSTERIOR PREDICTIVE ===

Question: How many of the NEXT 10 participants will claim?

Method: Integrate over uncertainty in p:
  1. Draw p ~ Beta(112, 61)
  2. Draw claims ~ Binomial(10, p)
  3. Repeat
)");

  constexpr int64_t n_new = 10;
  std::vector<int64_t> pred_samples(kSamples);
  for (int i = 0; i < kSamples; ++i) {
    double p = sample(p_posterior, rng);
    // Use bayes::Binomial for posterior predictive
    pred_samples[i] = bayes::Binomial{n_new, p}.sample(rng);
  }

  // Count outcomes
  std::vector<int64_t> counts(n_new + 1, 0);
  for (int64_t x : pred_samples) {
    counts[x]++;
  }

  std::vector<TextBar> bars;
  for (int64_t i = 0; i <= n_new; ++i) {
    double prob = static_cast<double>(counts[i]) / kSamples;
    bars.push_back({
        .length = static_cast<int64_t>(prob * 200),
        .label = std::format("{:2}", i),
        .end_text = std::format("{:5.1f}%", prob * 100),
        .color = colors::kGreen,
    });
  }
  std::print("{}", buildBarChartText(bars));

  double pred_mean =
      std::reduce(pred_samples.begin(), pred_samples.end(), 0LL) /
      static_cast<double>(kSamples);
  std::ranges::sort(pred_samples);
  int64_t pred_lo = pred_samples[static_cast<size_t>(0.055 * kSamples)];
  int64_t pred_hi = pred_samples[static_cast<size_t>(0.945 * kSamples)];

  std::println("\nResult: {:.1f} claims expected  (89% interval: [{}, {}])",
               pred_mean, pred_lo, pred_hi);

  // ═══════════════════════════════════════════════════════════════════════════
  std::println(R"(

=== PROBLEM 3: HIERARCHICAL MODEL (symbolic4 showcase) ===

Demonstrating symbolic4's compile-time model construction for a simple
hierarchical model: mu -> y

  mu ~ Normal(0, 10)    # prior on mean
  y  ~ Normal(mu, 1)    # observation with known variance
)");

  // Define hierarchical model - expression graph built at compile time
  auto mu = normal(0_c, 10_c);
  auto y = normal(mu, 1_c);

  // Joint log-probability is a symbolic expression
  auto joint_lp = logProb(mu, y);
  std::println("Joint log p(mu, y) evaluated at mu=2, y=2.5: {:.4f}",
               evaluate(joint_lp, mu = 2.0, y = 2.5));

  // Sample from prior - sampleAll returns values for all nodes
  auto trace = sampleAll(rng, mu, y);
  std::println("Prior sample: mu = {:.2f}, y = {:.2f}", trace[mu], trace[y]);

  // ═══════════════════════════════════════════════════════════════════════════
  std::println(R"(

=== KEY LESSONS ===

1. Generative reasoning: "If X% cheat, what would we see?" Math confirms intuition.
2. Bayesian updating: More data -> narrower credible intervals.
3. Posterior predictive: Integrate over parameter uncertainty, don't just plug in estimates.
4. symbolic4 features:
   - Declarative model specification: beta(), normal(), etc.
   - Compile-time expression graphs: logProb() builds symbolic expressions
   - Hierarchical sampling: sampleAll() samples entire model
)");

  return 0;
}
