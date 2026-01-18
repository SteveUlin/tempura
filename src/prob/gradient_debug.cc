#include "prob/log_prob.h"

#include <iostream>
#include <print>

#include "symbolic3/derivative.h"
#include "symbolic3/evaluate.h"

using namespace tempura::prob;
using namespace tempura::symbolic3;

auto main() -> int {
  constexpr Symbol x;
  constexpr Symbol μ;
  constexpr Symbol σ;

  // Test 1: Simplest case - d/dx(x) should be 1
  std::println("=== Test 1: d/dx(x) ===");
  constexpr auto dx = diff(x, x);
  auto bindings = BinderPack{x = 1.0, μ = 0.0, σ = 2.0};
  double dx_val = evaluate(dx, bindings);
  std::println("  d/dx(x) = {} (expected: 1)", dx_val);

  // Test 2: d/dx(x - μ) should be 1
  std::println("\n=== Test 2: d/dx(x - μ) ===");
  constexpr auto diff_expr = x - μ;
  constexpr auto d_diff = diff(diff_expr, x);
  double d_diff_val = evaluate(d_diff, bindings);
  std::println("  d/dx(x - μ) = {} (expected: 1)", d_diff_val);

  // Test 3: d/dx((x - μ) / σ) should be 1/σ = 0.5
  std::println("\n=== Test 3: d/dx((x - μ) / σ) ===");
  constexpr auto z = (x - μ) / σ;
  constexpr auto dz_dx = diff(z, x);
  double dz_val = evaluate(dz_dx, bindings);
  std::println("  d/dx((x-μ)/σ) = {} (expected: 0.5)", dz_val);

  // Test 4: d/dx(z²) where z = (x-μ)/σ
  std::println("\n=== Test 4: d/dx(z²) where z = (x-μ)/σ ===");
  constexpr auto z_sq = z * z;
  constexpr auto dz_sq = diff(z_sq, x);
  double dz_sq_val = evaluate(dz_sq, bindings);
  // At x=1, μ=0, σ=2: z = 0.5, dz²/dx = 2z * dz/dx = 2*0.5*0.5 = 0.5
  std::println("  d/dx(z²) = {} (expected: 0.5)", dz_sq_val);

  // Test 5: d/dx(-½z²)
  std::println("\n=== Test 5: d/dx(-½z²) ===");
  constexpr auto neg_half_z_sq = Fraction<-1, 2>{} * z * z;
  constexpr auto d_neg_half = diff(neg_half_z_sq, x);
  double d_neg_half_val = evaluate(d_neg_half, bindings);
  // Expected: -½ * 2z * dz/dx = -z/σ = -0.5/2 = -0.25
  std::println("  d/dx(-½z²) = {} (expected: -0.25)", d_neg_half_val);

  // Test 6: Full logNormal derivative
  std::println("\n=== Test 6: d/dx(logNormal(x, μ, σ)) ===");
  constexpr auto lp = logNormal(x, μ, σ);
  constexpr auto dlp_dx = diff(lp, x);
  double dlp_val = evaluate(dlp_dx, bindings);
  // Expected: -(x-μ)/σ² = -1/4 = -0.25
  std::println("  d/dx(logNormal) = {} (expected: -0.25)", dlp_val);

  // Test 7: Simplified version
  std::println("\n=== Test 7: Simplified d/dx(logNormal) ===");
  constexpr auto dlp_simple = diff_simplified(lp, x);
  double dlp_simple_val = evaluate(dlp_simple, bindings);
  std::println("  Simplified = {} (expected: -0.25)", dlp_simple_val);

  // Test 8: Evaluate the log-probability itself (not gradient)
  std::println("\n=== Test 8: Evaluate logNormal(x, μ, σ) directly ===");
  double lp_val = evaluate(lp, bindings);
  double expected_lp = logNormal(1.0, 0.0, 2.0);
  std::println("  Symbolic eval = {}", lp_val);
  std::println("  Direct compute = {}", expected_lp);

  return 0;
}
