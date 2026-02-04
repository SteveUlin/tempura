// Bangladesh Contraception - Simplified Gather Test
// Tests the gather() API for cross-plate indexing in hierarchical models.
//
// Model (simplified):
//   a[d] ~ Normal(0, 1) for d in Districts
//   district_idx[i] is which district observation i belongs to
//   y[i] ~ Bernoulli(logistic(a[district_idx[i]])) for i in Observations
//
// This tests:
//   1. gather(a, district_idx) - cross-plate indexing
//   2. Integration with SumOver and indexed evaluation
//   3. Symbolic differentiation through gather

#include <cmath>
#include <print>
#include <random>
#include <vector>

#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/indexed_eval.h"
#include "symbolic4/strategy/diff.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto invLogit(double x) -> double { return 1.0 / (1.0 + std::exp(-x)); }

auto main() -> int {
  std::println(R"(
=== BANGLADESH GATHER TEST ===

Testing cross-plate indexing with gather():
  - a[d] varies over Districts
  - district_idx[i] varies over Observations
  - gather(a, district_idx) produces a[district_idx[i]] for each observation
)");

  // Dimension tags
  struct Districts {};
  struct Obs {};

  // Simulate data
  constexpr std::size_t kNumDistricts = 5;
  constexpr std::size_t kNumObs = 20;

  // District effects (what we'd be inferring in full model)
  std::vector<double> a_true = {-1.0, -0.5, 0.0, 0.5, 1.0};

  // Which district each observation belongs to (randomly assigned)
  std::mt19937_64 rng{42};
  std::uniform_int_distribution<int> district_dist(0, kNumDistricts - 1);
  std::vector<double> district_idx(kNumObs);
  for (std::size_t i = 0; i < kNumObs; ++i) {
    district_idx[i] = static_cast<double>(district_dist(rng));
  }

  // Outcome based on district effects
  std::vector<double> y_data(kNumObs);
  std::uniform_real_distribution<double> uniform(0.0, 1.0);
  for (std::size_t i = 0; i < kNumObs; ++i) {
    auto d = static_cast<std::size_t>(district_idx[i]);
    double p = invLogit(a_true[d]);
    y_data[i] = uniform(rng) < p ? 1.0 : 0.0;
  }

  std::println("Generated {} observations across {} districts", kNumObs, kNumDistricts);
  std::println("District effects (true): {:.2f}, {:.2f}, {:.2f}, {:.2f}, {:.2f}",
               a_true[0], a_true[1], a_true[2], a_true[3], a_true[4]);

  // Count observations per district
  std::vector<int> counts(kNumDistricts, 0);
  std::vector<int> successes(kNumDistricts, 0);
  for (std::size_t i = 0; i < kNumObs; ++i) {
    auto d = static_cast<std::size_t>(district_idx[i]);
    counts[d]++;
    if (y_data[i] > 0.5) successes[d]++;
  }
  std::println("\nObservations per district: {}, {}, {}, {}, {}",
               counts[0], counts[1], counts[2], counts[3], counts[4]);
  std::println("Successes per district: {}, {}, {}, {}, {}",
               successes[0], successes[1], successes[2], successes[3], successes[4]);

  // --------------------------------------------------------------------------
  // Test 1: Basic gather evaluation
  // --------------------------------------------------------------------------
  std::println("\n--- Test 1: Basic gather evaluation ---");

  IndexedSymbol<struct AId, Districts> a;
  IndexedSymbol<struct IdxId, Obs> idx;

  auto gathered = gather(a, idx);

  IndexedBinding<decltype(a), 1> a_bind{std::span<const double>(a_true)};
  IndexedBinding<decltype(idx), 1> idx_bind{std::span<const double>(district_idx)};

  // Sum of gathered values (should equal sum of a_true[district_idx[i]])
  auto sum_gathered = sumOver<Obs>(gathered);
  double sum_val = evaluateIndexed(sum_gathered, a_bind, idx_bind);

  double expected_sum = 0.0;
  for (std::size_t i = 0; i < kNumObs; ++i) {
    auto d = static_cast<std::size_t>(district_idx[i]);
    expected_sum += a_true[d];
  }

  std::println("Sum of gathered values: {:.4f}", sum_val);
  std::println("Expected sum: {:.4f}", expected_sum);
  std::println("Match: {}", std::abs(sum_val - expected_sum) < 1e-10 ? "✓" : "✗");

  // --------------------------------------------------------------------------
  // Test 2: Gather with expression (a * scale)
  // --------------------------------------------------------------------------
  std::println("\n--- Test 2: Gather with expression ---");

  Symbol<struct Scale> scale;
  auto scaled_gathered = sumOver<Obs>(gather(a * scale, idx));
  double scaled_val = evaluateIndexed(scaled_gathered, a_bind, idx_bind, scale = 2.0);
  std::println("Sum of scaled gathered: {:.4f} (expected: {:.4f})",
               scaled_val, expected_sum * 2.0);
  std::println("Match: {}", std::abs(scaled_val - expected_sum * 2.0) < 1e-10 ? "✓" : "✗");

  // --------------------------------------------------------------------------
  // Test 3: Gradient of sum w.r.t. scalar parameter
  // --------------------------------------------------------------------------
  std::println("\n--- Test 3: Gradient via symbolic diff ---");

  // f(mu) = SumOver<Obs>(gather(a + mu, idx))
  // df/dmu = number of observations (since d(a+mu)/dmu = 1 for each)
  Symbol<struct Mu> mu;
  auto f_mu = sumOver<Obs>(gather(a + mu, idx));
  auto df_dmu = diff(f_mu, mu);

  double grad_val = evaluateIndexed(df_dmu, a_bind, idx_bind, mu = 0.0);
  std::println("Gradient d/dmu[sum(gather(a+mu, idx))]: {:.4f}", grad_val);
  std::println("Expected (num obs): {:.4f}", static_cast<double>(kNumObs));
  std::println("Match: {}", std::abs(grad_val - static_cast<double>(kNumObs)) < 1e-10 ? "✓" : "✗");

  // --------------------------------------------------------------------------
  // Test 4: Numerical gradient verification
  // --------------------------------------------------------------------------
  std::println("\n--- Test 4: Numerical gradient check ---");

  double eps = 1e-6;
  double f_plus = evaluateIndexed(f_mu, a_bind, idx_bind, mu = eps);
  double f_minus = evaluateIndexed(f_mu, a_bind, idx_bind, mu = -eps);
  double numerical_grad = (f_plus - f_minus) / (2 * eps);

  std::println("Symbolic gradient: {:.6f}", grad_val);
  std::println("Numerical gradient: {:.6f}", numerical_grad);
  std::println("Match: {}", std::abs(grad_val - numerical_grad) < 1e-4 ? "✓" : "✗");

  // --------------------------------------------------------------------------
  // Test 5: Bernoulli-like log-likelihood (simplified)
  // --------------------------------------------------------------------------
  std::println("\n--- Test 5: Log-likelihood-like expression ---");

  // log-lik ≈ sum_i [y[i] * a[idx[i]] - log(1 + exp(a[idx[i]]))]
  // For simplicity, just compute sum_i [y[i] * a[idx[i]]]
  IndexedSymbol<struct YId, Obs> y;
  auto weighted_sum = sumOver<Obs>(y * gather(a, idx));

  IndexedBinding<decltype(y), 1> y_bind{std::span<const double>(y_data)};

  double weighted_val = evaluateIndexed(weighted_sum, a_bind, idx_bind, y_bind);

  double expected_weighted = 0.0;
  for (std::size_t i = 0; i < kNumObs; ++i) {
    auto d = static_cast<std::size_t>(district_idx[i]);
    expected_weighted += y_data[i] * a_true[d];
  }

  std::println("Weighted sum (y * gathered): {:.4f}", weighted_val);
  std::println("Expected: {:.4f}", expected_weighted);
  std::println("Match: {}", std::abs(weighted_val - expected_weighted) < 1e-10 ? "✓" : "✗");

  std::println("\n=== ALL TESTS COMPLETE ===\n");

  return 0;
}
