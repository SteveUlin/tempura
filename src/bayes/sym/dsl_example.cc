#include "bayes/sym/dsl.h"

#include <cmath>
#include <print>
#include <type_traits>

using namespace tempura::bayes::sym;
using namespace tempura::symbolic3;

auto main() -> int {
  std::println("=== Bayesian Model DSL (Option 3: Inference-time Conditioning) ===\n");

  // =========================================================================
  // Define variables (all treated uniformly)
  // =========================================================================
  constexpr Symbol mu;
  constexpr Symbol sigma;
  constexpr Symbol y;

  // Verify variables are unique types
  static_assert(!std::is_same_v<decltype(mu), decltype(sigma)>);
  static_assert(!std::is_same_v<decltype(sigma), decltype(y)>);

  // =========================================================================
  // Define joint distribution
  // =========================================================================
  auto joint = Joint{
      mu | Normal(0_c, 10_c),       // mu ~ Normal(0, 10)
      sigma | HalfNormal(5_c),      // sigma ~ HalfNormal(5)
      y | Normal(mu, sigma)         // y ~ Normal(mu, sigma)
  };

  std::println("Joint distribution defined with 3 variables.");

  // =========================================================================
  // Condition on observations, specify inference targets
  // =========================================================================
  auto posterior = joint
      .observe(y = 2.5)           // condition on y = 2.5
      .infer(mu, sigma);          // infer mu and sigma (in this order)

  std::println("Posterior: p(mu, sigma | y=2.5)");
  std::println("  Inference params: mu (index 0), sigma (index 1)\n");

  // =========================================================================
  // Evaluate log-probability
  // =========================================================================
  std::println("=== Log-probability ===");

  double lp = posterior.logProb(1.0, 2.0);  // mu=1.0, sigma=2.0
  std::println("  posterior.logProb(1.0, 2.0) = {:.6f}", lp);

  // Manual verification
  double lp_prior_mu = logNormal(1.0, 0.0, 10.0);
  double lp_prior_sigma = logHalfNormal(2.0, 5.0);
  double lp_likelihood = logNormal(2.5, 1.0, 2.0);
  double lp_expected = lp_prior_mu + lp_prior_sigma + lp_likelihood;

  std::println("  Expected: {:.6f}", lp_expected);
  std::println("  Match: {}", std::abs(lp - lp_expected) < 1e-10 ? "YES" : "NO");

  // =========================================================================
  // Evaluate gradient
  // =========================================================================
  std::println("\n=== Gradient ===");

  auto grad = posterior.gradient(1.0, 2.0);
  std::println("  posterior.gradient(1.0, 2.0) = [{:.6f}, {:.6f}]",
               grad[0], grad[1]);

  // Expected gradients
  double expected_grad_mu = -1.0 / 100.0 + (2.5 - 1.0) / 4.0;  // 0.365
  double expected_grad_sigma = -2.0 / 25.0 +
      ((2.5 - 1.0) * (2.5 - 1.0) / 8.0 - 1.0 / 2.0);  // -0.29875

  std::println("  Expected: [{:.6f}, {:.6f}]",
               expected_grad_mu, expected_grad_sigma);
  std::println("  Match mu:    {}",
               std::abs(grad[0] - expected_grad_mu) < 1e-10 ? "YES" : "NO");
  std::println("  Match sigma: {}",
               std::abs(grad[1] - expected_grad_sigma) < 1e-10 ? "YES" : "NO");

  // =========================================================================
  // Different param order
  // =========================================================================
  std::println("\n=== Different param order ===");

  auto posterior2 = joint
      .observe(y = 2.5)
      .infer(sigma, mu);  // sigma first, then mu

  double lp2 = posterior2.logProb(2.0, 1.0);  // sigma=2.0, mu=1.0
  std::println("  infer<sigma, mu>().logProb(2.0, 1.0) = {:.6f}", lp2);
  std::println("  Same as before: {}",
               std::abs(lp2 - lp) < 1e-10 ? "YES" : "NO");

  auto grad2 = posterior2.gradient(2.0, 1.0);
  std::println("  gradient = [{:.6f}, {:.6f}]", grad2[0], grad2[1]);
  std::println("  grad[0] is d/d(sigma): {}",
               std::abs(grad2[0] - expected_grad_sigma) < 1e-10 ? "YES" : "NO");
  std::println("  grad[1] is d/d(mu): {}",
               std::abs(grad2[1] - expected_grad_mu) < 1e-10 ? "YES" : "NO");

  // =========================================================================
  // Multiple observations
  // =========================================================================
  std::println("\n=== Multiple observations ===");

  constexpr Symbol y1;
  constexpr Symbol y2;
  constexpr Symbol mu2;  // need new symbol since mu is in scope

  auto joint2 = Joint{
      mu2 | Normal(0_c, 10_c),
      y1 | Normal(mu2, 1_c),   // y1 ~ Normal(mu, 1)
      y2 | Normal(mu2, 1_c)    // y2 ~ Normal(mu, 1)
  };

  auto posterior3 = joint2
      .observe(y1 = 1.5, y2 = 2.5)
      .infer(mu2);

  double lp3 = posterior3.logProb(2.0);  // mu = 2.0
  std::println("  Two observations: logProb(2.0) = {:.6f}", lp3);

  auto grad3 = posterior3.gradient(2.0);
  std::println("  gradient = [{:.6f}]", grad3[0]);

  std::println("\n=== Complete ===");

  return 0;
}
