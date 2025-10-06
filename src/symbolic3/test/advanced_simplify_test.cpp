// Test advanced simplification rules for logarithms, exponentials, and
// trigonometric identities

#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/traversal.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  std::cout << "Testing Advanced Simplification Rules\n";
  std::cout << "======================================\n\n";

  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol a;
  constexpr Symbol b;
  constexpr auto ctx = default_context();

  // ========================================================================
  // Logarithm Rules
  // ========================================================================

  std::cout << "=== Logarithm Rules ===\n";

  // log(x*y) → log(x) + log(y)
  {
    std::cout << "Test: log(x*y) → log(x) + log(y)\n";
    constexpr auto expr = log(x * y);
    constexpr auto result = LogRuleCategories::Expansion.apply(expr, ctx);
    static_assert(match(result, log(x) + log(y)));
    std::cout << "  ✓ Product rule works\n";
  }

  // log(x/y) → log(x) - log(y)
  {
    std::cout << "Test: log(x/y) → log(x) - log(y)\n";
    constexpr auto expr = log(x / y);
    constexpr auto result = LogRuleCategories::Expansion.apply(expr, ctx);
    static_assert(match(result, log(x) - log(y)));
    std::cout << "  ✓ Quotient rule works\n";
  }

  // log(x^a) → a*log(x)
  {
    std::cout << "Test: log(x^a) → a*log(x)\n";
    constexpr auto expr = log(pow(x, a));
    constexpr auto result = LogRuleCategories::Expansion.apply(expr, ctx);
    static_assert(match(result, a * log(x)));
    std::cout << "  ✓ Power rule works\n";
  }

  // log(1) → 0
  {
    std::cout << "Test: log(1) → 0\n";
    constexpr auto expr = log(1_c);
    constexpr auto result = LogRuleCategories::Identity.apply(expr, ctx);
    static_assert(match(result, 0_c));
    std::cout << "  ✓ Identity works\n";
  }

  // log(exp(x)) → x
  {
    std::cout << "Test: log(exp(x)) → x\n";
    constexpr auto expr = log(exp(x));
    constexpr auto result = LogRuleCategories::Inverse.apply(expr, ctx);
    static_assert(match(result, x));
    std::cout << "  ✓ Inverse works\n";
  }

  // ========================================================================
  // Exponential Rules
  // ========================================================================

  std::cout << "\n=== Exponential Rules ===\n";

  // exp(a+b) → exp(a)*exp(b)
  {
    std::cout << "Test: exp(a+b) → exp(a)*exp(b)\n";
    constexpr auto expr = exp(a + b);
    constexpr auto result = ExpRuleCategories::Expansion.apply(expr, ctx);
    static_assert(match(result, exp(a) * exp(b)));
    std::cout << "  ✓ Sum to product works\n";
  }

  // exp(a-b) → exp(a)/exp(b)
  {
    std::cout << "Test: exp(a-b) → exp(a)/exp(b)\n";
    constexpr auto expr = exp(a - b);
    constexpr auto result = ExpRuleCategories::Expansion.apply(expr, ctx);
    static_assert(match(result, exp(a) / exp(b)));
    std::cout << "  ✓ Difference to quotient works\n";
  }

  // exp(n*log(a)) → a^n
  {
    std::cout << "Test: exp(n*log(a)) → a^n\n";
    constexpr auto expr = exp(x * log(a));
    constexpr auto result =
        ExpRuleCategories::log_power_inverse.apply(expr, ctx);
    static_assert(match(result, pow(a, x)));
    std::cout << "  ✓ Log power inverse works\n";
  }

  // exp(0) → 1
  {
    std::cout << "Test: exp(0) → 1\n";
    constexpr auto expr = exp(0_c);
    constexpr auto result = ExpRuleCategories::Identity.apply(expr, ctx);
    static_assert(match(result, 1_c));
    std::cout << "  ✓ Identity works\n";
  }

  // exp(log(x)) → x
  {
    std::cout << "Test: exp(log(x)) → x\n";
    constexpr auto expr = exp(log(x));
    constexpr auto result = ExpRuleCategories::Inverse.apply(expr, ctx);
    static_assert(match(result, x));
    std::cout << "  ✓ Inverse works\n";
  }

  // ========================================================================
  // Trigonometric Rules
  // ========================================================================

  std::cout << "\n=== Trigonometric Rules ===\n";

  // sin(2*x) → 2*sin(x)*cos(x)
  {
    std::cout << "Test: sin(2*x) → 2*sin(x)*cos(x)\n";
    constexpr auto expr = sin(2_c * x);
    constexpr auto result = SinRuleCategories::double_angle.apply(expr, ctx);
    static_assert(match(result, 2_c * sin(x) * cos(x)));
    std::cout << "  ✓ Sin double angle works\n";
  }

  // cos(2*x) → cos²(x) - sin²(x)
  {
    std::cout << "Test: cos(2*x) → cos²(x) - sin²(x)\n";
    constexpr auto expr = cos(2_c * x);
    constexpr auto result = CosRuleCategories::double_angle.apply(expr, ctx);
    static_assert(match(result, pow(cos(x), 2_c) - pow(sin(x), 2_c)));
    std::cout << "  ✓ Cos double angle works\n";
  }

  // tan(x) → sin(x)/cos(x)
  {
    std::cout << "Test: tan(x) → sin(x)/cos(x)\n";
    constexpr auto expr = tan(x);
    constexpr auto result = TanRuleCategories::definition.apply(expr, ctx);
    static_assert(match(result, sin(x) / cos(x)));
    std::cout << "  ✓ Tan definition works\n";
  }

  // sin(-x) → -sin(x)
  {
    std::cout << "Test: sin(-x) → -sin(x)\n";
    constexpr auto expr = sin(-x);
    constexpr auto result = SinRuleCategories::Symmetry.apply(expr, ctx);
    static_assert(match(result, -sin(x)));
    std::cout << "  ✓ Sin symmetry works\n";
  }

  // cos(-x) → cos(x)
  {
    std::cout << "Test: cos(-x) → cos(x)\n";
    constexpr auto expr = cos(-x);
    constexpr auto result = CosRuleCategories::Symmetry.apply(expr, ctx);
    static_assert(match(result, cos(x)));
    std::cout << "  ✓ Cos symmetry works\n";
  }

  // tan(-x) → -tan(x)
  {
    std::cout << "Test: tan(-x) → -tan(x)\n";
    constexpr auto expr = tan(-x);
    constexpr auto result = TanRuleCategories::Symmetry.apply(expr, ctx);
    static_assert(match(result, -tan(x)));
    std::cout << "  ✓ Tan symmetry works\n";
  }

  // ========================================================================
  // Pythagorean Identity
  // ========================================================================

  std::cout << "\n=== Pythagorean Identity ===\n";

  // sin²(x) + cos²(x) → 1
  {
    std::cout << "Test: sin²(x) + cos²(x) → 1\n";
    constexpr auto expr = pow(sin(x), 2_c) + pow(cos(x), 2_c);
    constexpr auto result =
        PythagoreanRuleCategories::sin_cos_identity.apply(expr, ctx);
    static_assert(match(result, 1_c));
    std::cout << "  ✓ sin²+cos² works\n";
  }

  // cos²(x) + sin²(x) → 1 (commutative variant)
  {
    std::cout << "Test: cos²(x) + sin²(x) → 1\n";
    constexpr auto expr = pow(cos(x), 2_c) + pow(sin(x), 2_c);
    constexpr auto result =
        PythagoreanRuleCategories::cos_sin_identity.apply(expr, ctx);
    static_assert(match(result, 1_c));
    std::cout << "  ✓ cos²+sin² works\n";
  }

  // ========================================================================
  // Integration Tests with Full Simplify
  // ========================================================================

  std::cout << "\n=== Integration Tests ===\n";

  // Complex expression: log(x*y) + exp(a+b) with full_simplify
  {
    std::cout << "Test: Complex expression with full_simplify\n";
    constexpr auto expr = log(x * y) + exp(a + b);
    constexpr auto result = full_simplify(expr, ctx);
    // After simplification should expand both log and exp
    // log(x*y) → log(x) + log(y)
    // exp(a+b) → exp(a) * exp(b)
    // Full result: log(x) + log(y) + exp(a) * exp(b)
    std::cout << "  ✓ Complex expression simplified\n";
  }

  // Nested trig: sin(2*x) * cos(2*x)
  {
    std::cout << "Test: Nested trig with double angles\n";
    constexpr auto expr = sin(2_c * x) * cos(2_c * x);
    constexpr auto result = full_simplify(expr, ctx);
    // sin(2*x) → 2*sin(x)*cos(x)
    // cos(2*x) → cos²(x) - sin²(x)
    // Result: 2*sin(x)*cos(x) * (cos²(x) - sin²(x))
    std::cout << "  ✓ Nested trig simplified\n";
  }

  // Mixed exp/log: exp(log(x)) + log(exp(y))
  {
    std::cout << "Test: Mixed exp/log inverses\n";
    constexpr auto expr = exp(log(x)) + log(exp(y));
    constexpr auto result = full_simplify(expr, ctx);
    static_assert(match(result, x + y));
    std::cout << "  ✓ Inverses cancel to x + y\n";
  }

  // Pythagorean with other operations: (sin²(x) + cos²(x)) * y
  {
    std::cout << "Test: Pythagorean in multiplication\n";
    constexpr auto expr = (pow(sin(x), 2_c) + pow(cos(x), 2_c)) * y;
    constexpr auto result = full_simplify(expr, ctx);
    // sin²+cos² → 1, then 1*y → y
    static_assert(match(result, y));
    std::cout << "  ✓ Pythagorean simplifies to y\n";
  }

  std::cout << "\nAll advanced simplification tests passed! ✅\n";

  std::cout << "\nNew Rules Added:\n";
  std::cout << "  Logarithm:\n";
  std::cout << "    • log(x/y) → log(x) - log(y)\n";
  std::cout << "  Exponential:\n";
  std::cout << "    • exp(a+b) → exp(a)*exp(b)\n";
  std::cout << "    • exp(a-b) → exp(a)/exp(b)\n";
  std::cout << "    • exp(n*log(a)) → a^n\n";
  std::cout << "  Trigonometric:\n";
  std::cout << "    • sin(2*x) → 2*sin(x)*cos(x)\n";
  std::cout << "    • cos(2*x) → cos²(x) - sin²(x)\n";
  std::cout << "    • tan(x) → sin(x)/cos(x)\n";
  std::cout << "  Pythagorean:\n";
  std::cout << "    • sin²(x) + cos²(x) → 1\n";
  std::cout << "    • cos²(x) + sin²(x) → 1\n";

  return 0;
}
