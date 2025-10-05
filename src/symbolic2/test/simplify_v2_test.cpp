#include "symbolic2/simplify_v2.h"
#include <print>

using namespace tempura;

int main() {
  std::println("=== Testing simplify_v2.h with Pattern Matching ===\n");

  // Test 1: Power identities
  {
    std::println("Test 1: Power identities");
    Symbol x;
    
    constexpr auto expr1 = pow(x, 0_c);
    constexpr auto s1 = simplify(expr1);
    static_assert(match(s1, 1_c), "x^0 should be 1");
    std::println("  pow(x, 0) → 1 ✓");
    
    constexpr auto expr2 = pow(x, 1_c);
    constexpr auto s2 = simplify(expr2);
    static_assert(match(s2, x), "x^1 should be x");
    std::println("  pow(x, 1) → x ✓");
    
    constexpr auto expr3 = pow(1_c, x);
    constexpr auto s3 = simplify(expr3);
    static_assert(match(s3, 1_c), "1^x should be 1");
    std::println("  pow(1, x) → 1 ✓");
    
    std::println("  ✓ Power identities work!\n");
  }

  // Test 2: Addition identities
  {
    std::println("Test 2: Addition identities");
    Symbol x;
    
    constexpr auto expr1 = 0_c + x;
    constexpr auto s1 = simplify(expr1);
    static_assert(match(s1, x), "0 + x should be x");
    std::println("  0 + x → x ✓");
    
    constexpr auto expr2 = x + 0_c;
    constexpr auto s2 = simplify(expr2);
    static_assert(match(s2, x), "x + 0 should be x");
    std::println("  x + 0 → x ✓");
    
    std::println("  ✓ Addition identities work!\n");
  }

  // Test 3: Multiplication identities
  {
    std::println("Test 3: Multiplication identities");
    Symbol x;
    
    constexpr auto expr1 = 0_c * x;
    constexpr auto s1 = simplify(expr1);
    static_assert(match(s1, 0_c), "0 * x should be 0");
    std::println("  0 * x → 0 ✓");
    
    constexpr auto expr2 = 1_c * x;
    constexpr auto s2 = simplify(expr2);
    static_assert(match(s2, x), "1 * x should be x");
    std::println("  1 * x → x ✓");
    
    std::println("  ✓ Multiplication identities work!\n");
  }

  // Test 4: Logarithm identities (using RewriteSystem)
  {
    std::println("Test 4: Logarithm identities");
    
    constexpr auto expr1 = log(1_c);
    constexpr auto s1 = simplify(expr1);
    static_assert(match(s1, 0_c), "log(1) should be 0");
    std::println("  log(1) → 0 ✓");
    
    constexpr auto expr2 = log(e);
    constexpr auto s2 = simplify(expr2);
    static_assert(match(s2, 1_c), "log(e) should be 1");
    std::println("  log(e) → 1 ✓");
    
    std::println("  ✓ Logarithm identities work!\n");
  }

  // Test 5: Exp-log cancellation (using RewriteSystem)
  {
    std::println("Test 5: Exp-log cancellation");
    Symbol x;
    
    constexpr auto expr1 = exp(log(x));
    constexpr auto s1 = simplify(expr1);
    // exp(log(x)) → x via ExpRules
    static_assert(match(s1, x), "exp(log(x)) should be x");
    std::println("  exp(log(x)) → x ✓");
    
    // Note: log(exp(x)) requires multi-step simplification:
    // log(exp(x)) → log(e^x) → x*log(e) → x*1 → x
    // This works in the original simplify.h but requires careful ordering
    
    std::println("  ✓ Exp-log cancellation works!\n");
  }

  // Test 6: Trigonometric identities (using RewriteSystem)
  {
    std::println("Test 6: Trigonometric identities");
    
    constexpr auto expr1 = sin(π * 0.5_c);
    constexpr auto s1 = simplify(expr1);
    static_assert(match(s1, 1_c), "sin(π/2) should be 1");
    std::println("  sin(π/2) → 1 ✓");
    
    constexpr auto expr2 = sin(π);
    constexpr auto s2 = simplify(expr2);
    static_assert(match(s2, 0_c), "sin(π) should be 0");
    std::println("  sin(π) → 0 ✓");
    
    constexpr auto expr3 = cos(π * 0.5_c);
    constexpr auto s3 = simplify(expr3);
    static_assert(match(s3, 0_c), "cos(π/2) should be 0");
    std::println("  cos(π/2) → 0 ✓");
    
    std::println("  ✓ Trigonometric identities work!\n");
  }

  // Test 7: Constant folding
  {
    std::println("Test 7: Constant folding");
    
    constexpr auto expr1 = 2_c + 3_c;
    constexpr auto s1 = simplify(expr1);
    static_assert(match(s1, 5_c), "2 + 3 should be 5");
    std::println("  2 + 3 → 5 ✓");
    
    constexpr auto expr2 = 10_c * 5_c;
    constexpr auto s2 = simplify(expr2);
    static_assert(match(s2, 50_c), "10 * 5 should be 50");
    std::println("  10 * 5 → 50 ✓");
    
    std::println("  ✓ Constant folding works!\n");
  }

  std::println("=== All Tests Passed! ===");
  return 0;
}
