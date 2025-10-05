#include "../pattern_matching.h"
#include "../symbolic.h"
#include <cassert>
#include <iostream>

using namespace tempura;

// Test that repeated pattern variables match the same expression
int main() {
  std::cout << "=== Repeated Pattern Variables Test ===" << std::endl << std::endl;

  constexpr Symbol a;
  constexpr Symbol b;
  constexpr Symbol c;

  // Test 1: x_ + x_ should match a + a but not a + b
  std::cout << "Testing x_ + x_ pattern..." << std::endl;
  {
    auto rule = Rewrite{x_ + x_, x_ * 2_c};

    // Should match a + a
    auto expr1 = a + a;
    auto result1 = rule.apply(expr1);
    static_assert(match(result1, a * 2_c));
    std::cout << "  ✓ a + a → a * 2" << std::endl;

    // Should NOT match a + b (different expressions)
    auto expr2 = a + b;
    auto result2 = rule.apply(expr2);
    static_assert(match(result2, a + b));  // Unchanged
    std::cout << "  ✓ a + b → a + b (no match)" << std::endl;
  }

  // Test 2: x_ * a_ + x_ should match a * 3 + a but not a * 3 + b
  std::cout << std::endl << "Testing x_ * a_ + x_ pattern..." << std::endl;
  {
    auto rule = Rewrite{x_ * a_ + x_, x_ * (a_ + 1_c)};

    // Should match a * 3 + a
    auto expr1 = a * 3_c + a;
    auto result1 = rule.apply(expr1);
    static_assert(match(result1, a * (3_c + 1_c)));
    std::cout << "  ✓ a * 3 + a → a * (3 + 1)" << std::endl;

    // Should NOT match a * 3 + b
    auto expr2 = a * 3_c + b;
    auto result2 = rule.apply(expr2);
    static_assert(match(result2, a * 3_c + b));  // Unchanged
    std::cout << "  ✓ a * 3 + b → a * 3 + b (no match)" << std::endl;
  }

  // Test 3: pow(x_, a_) * pow(x_, b_) should match pow(a, 2) * pow(a, 3) but not pow(a, 2) * pow(b, 3)
  std::cout << std::endl << "Testing pow(x_, a_) * pow(x_, b_) pattern..." << std::endl;
  {
    auto rule = Rewrite{pow(x_, a_) * pow(x_, b_), pow(x_, a_ + b_)};

    // Should match pow(a, 2) * pow(a, 3)
    auto expr1 = pow(a, 2_c) * pow(a, 3_c);
    auto result1 = rule.apply(expr1);
    static_assert(match(result1, pow(a, 2_c + 3_c)));
    std::cout << "  ✓ a^2 * a^3 → a^(2+3)" << std::endl;

    // Should NOT match pow(a, 2) * pow(b, 3)
    auto expr2 = pow(a, 2_c) * pow(b, 3_c);
    auto result2 = rule.apply(expr2);
    static_assert(match(result2, pow(a, 2_c) * pow(b, 3_c)));  // Unchanged
    std::cout << "  ✓ a^2 * b^3 → a^2 * b^3 (no match)" << std::endl;
  }

  // Test 4: Complex nested pattern
  std::cout << std::endl << "Testing complex nested pattern..." << std::endl;
  {
    // (x_ + y_) * (x_ + y_) should match (a + b) * (a + b)
    auto rule = Rewrite{(x_ + y_) * (x_ + y_), pow(x_ + y_, 2_c)};

    auto expr1 = (a + b) * (a + b);
    auto result1 = rule.apply(expr1);
    static_assert(match(result1, pow(a + b, 2_c)));
    std::cout << "  ✓ (a + b) * (a + b) → (a + b)^2" << std::endl;

    // Should NOT match (a + b) * (a + c)
    auto expr2 = (a + b) * (a + c);
    auto result2 = rule.apply(expr2);
    static_assert(match(result2, (a + b) * (a + c)));  // Unchanged
    std::cout << "  ✓ (a + b) * (a + c) → (a + b) * (a + c) (no match)" << std::endl;
  }

  std::cout << std::endl << "=== All repeated variable tests passed! ===" << std::endl;
  return 0;
}
