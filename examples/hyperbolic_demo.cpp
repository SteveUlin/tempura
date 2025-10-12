//==============================================================================
// Hyperbolic Functions Demo
//
// Demonstrates the new hyperbolic function support in symbolic3:
// - sinh, cosh, tanh operators
// - Symmetry properties (odd/even functions)
// - Definitions in terms of exponentials
// - Hyperbolic identity: cosh²(x) - sinh²(x) = 1
// - Numerical evaluation
//==============================================================================

#include "symbolic3/symbolic3.h"
#include <iostream>
#include <print>
#include <cmath>

using namespace tempura::symbolic3;

int main() {
  constexpr Symbol x;
  
  std::print("\n=== Hyperbolic Functions in Symbolic3 ===\n\n\n");
  
  // Basic hyperbolic expressions
  std::print("Basic expressions:\n");
  std::print("  sinh(x) created\n");
  std::print("  cosh(x) created\n");
  std::print("  tanh(x) created\n");
  
  // Identity rules at zero
  std::print("\nIdentity rules:\n");
  {
    constexpr auto ctx = default_context();
    constexpr auto sinh_zero = simplify(sinh(0_c), ctx);
    constexpr auto cosh_zero = simplify(cosh(0_c), ctx);
    constexpr auto tanh_zero = simplify(tanh(0_c), ctx);
    
    static_assert(match(sinh_zero, 0_c));
    static_assert(match(cosh_zero, 1_c));
    static_assert(match(tanh_zero, 0_c));
    
    std::print("  sinh(0) → 0\n");
    std::print("  cosh(0) → 1\n");
    std::print("  tanh(0) → 0\n");
  }
  
  // Symmetry properties
  std::print("\nSymmetry properties:\n");
  {
    constexpr auto ctx = default_context();
    constexpr auto sinh_neg = SinhRules.apply(sinh(-x), ctx);
    constexpr auto cosh_neg = CoshRules.apply(cosh(-x), ctx);
    constexpr auto tanh_neg = TanhRules.apply(tanh(-x), ctx);
    
    static_assert(match(sinh_neg, -sinh(x)));
    static_assert(match(cosh_neg, cosh(x)));
    static_assert(match(tanh_neg, -tanh(x)));
    
    std::print("  sinh(-x) → -sinh(x)  (odd function)\n");
    std::print("  cosh(-x) → cosh(x)   (even function)\n");
    std::print("  tanh(-x) → -tanh(x)  (odd function)\n");
  }
  
  // Hyperbolic identity
  std::print("\nHyperbolic identity:\n");
  {
    constexpr auto ctx = default_context();
    constexpr auto identity = pow(cosh(x), 2_c) - pow(sinh(x), 2_c);
    constexpr auto result = HyperbolicIdentityRules.apply(identity, ctx);
    
    static_assert(match(result, 1_c));
    std::print("  cosh²(x) - sinh²(x) → 1\n");
  }
  
  // Numerical evaluation
  std::print("\nNumerical evaluation at x = 1:\n");
  {
    auto sinh_val = evaluate(sinh(x), BinderPack{x = 1.0});
    auto cosh_val = evaluate(cosh(x), BinderPack{x = 1.0});
    auto tanh_val = evaluate(tanh(x), BinderPack{x = 1.0});
    
    std::print("  sinh(1) = {:.6f}", sinh_val);
    std::print("  cosh(1) = {:.6f}", cosh_val);
    std::print("  tanh(1) = {:.6f}", tanh_val);
    
    // Verify identity numerically
    auto identity_val = std::pow(cosh_val, 2) - std::pow(sinh_val, 2);
    std::print("  cosh²(1) - sinh²(1) = {:.10f}", identity_val);
  }
  
  // Relationship to exp
  std::print("\nRelationship to exponential:\n");
  {
    // sinh(x) + cosh(x) = exp(x)
    auto lhs = evaluate(sinh(x) + cosh(x), BinderPack{x = 1.0});
    auto rhs = std::exp(1.0);
    std::print("  sinh(1) + cosh(1) = {:.6f}", lhs);
    std::print("  exp(1)            = {:.6f}", rhs);
    std::print("  Difference        = {:.10e}", std::abs(lhs - rhs));
  }
  
  // Definition in terms of exponentials
  std::print("\nDefinitions (symbolic):\n");
  {
    constexpr auto ctx = default_context();
    constexpr auto sinh_def = SinhRuleCategories::definition.apply(sinh(x), ctx);
    constexpr auto cosh_def = CoshRuleCategories::definition.apply(cosh(x), ctx);
    
    static_assert(match(sinh_def, (exp(x) - exp(-x)) / 2_c));
    static_assert(match(cosh_def, (exp(x) + exp(-x)) / 2_c));
    
    std::print("  sinh(x) → (exp(x) - exp(-x))/2\n");
    std::print("  cosh(x) → (exp(x) + exp(-x))/2\n");
  }
  
  std::print("\n✓ All hyperbolic function operations demonstrated!\n\n");
  
  return 0;
}
