#include "../pattern_matching.h"
#include "../constants.h"
#include "../operators.h"
#include "../ordering.h"
#include <iostream>

using namespace tempura;

// Define some symbols for testing
inline constexpr Symbol a{};
inline constexpr Symbol b{};
inline constexpr Symbol c{};

// Test basic rewrite without predicate (should work as before)
void testBasicRewrite() {
  std::cout << "Testing basic rewrite without predicate...\n";

  constexpr auto rule = Rewrite{pow(x_, 0_c), 1_c};
  constexpr auto expr = pow(a, 0_c);
  constexpr auto result = rule.apply(expr);

  static_assert(match(result, 1_c), "Basic rewrite should work");
  std::cout << "  ✓ Basic rewrite: pow(a, 0) → 1\n";
}

// Test rewrite with ordering predicate
void testOrderingPredicate() {
  std::cout << "Testing rewrite with ordering predicate...\n";

  // Rule: x + y → y + x if y < x (canonical ordering)
  constexpr auto canonicalAdd = Rewrite{
    x_ + y_,
    y_ + x_,
    [](auto ctx) {
      return symbolicLessThan(get(ctx, y_), get(ctx, x_));
    }
  };

  // Test 1: b + a → a + b (since a < b)
  constexpr auto expr1 = b + a;
  constexpr auto result1 = canonicalAdd.apply(expr1);
  static_assert(match(result1, a + b), "Should reorder b + a to a + b");
  std::cout << "  ✓ Ordering: b + a → a + b\n";

  // Test 2: a + b → a + b (already in order, predicate should fail)
  constexpr auto expr2 = a + b;
  constexpr auto result2 = canonicalAdd.apply(expr2);
  static_assert(match(result2, a + b), "Should not reorder a + b");
  std::cout << "  ✓ Ordering: a + b → a + b (no change)\n";

  // Test 3: Expression ordering - c + b → b + c (since b < c)
  constexpr auto expr3 = c + b;
  constexpr auto result3 = canonicalAdd.apply(expr3);
  static_assert(match(result3, b + c), "Should reorder c + b to b + c");
  std::cout << "  ✓ Ordering: c + b → b + c\n";
}

// Test rewrite with multiplication ordering
void testMultiplicationOrdering() {
  std::cout << "Testing multiplication ordering predicate...\n";

  // Rule: x * y → y * x if y < x
  constexpr auto canonicalMul = Rewrite{
    x_ * y_,
    y_ * x_,
    [](auto ctx) {
      return symbolicLessThan(get(ctx, y_), get(ctx, x_));
    }
  };

  // Test: b * a → a * b
  constexpr auto expr = b * a;
  constexpr auto result = canonicalMul.apply(expr);
  static_assert(match(result, a * b), "Should reorder b * a to a * b");
  std::cout << "  ✓ Ordering: b * a → a * b\n";
}

// Test associativity reordering with predicate
void testAssociativityOrdering() {
  std::cout << "Testing associativity ordering predicate...\n";

  // Rule: (a + c) + b → (a + b) + c if b < c
  constexpr auto assocReorder = Rewrite{
    (a_ + c_) + b_,
    (a_ + b_) + c_,
    [](auto ctx) {
      return symbolicLessThan(get(ctx, b_), get(ctx, c_));
    }
  };

  // Test: (a + c) + b → (a + b) + c (since b < c)
  constexpr auto expr = (a + c) + b;
  constexpr auto result = assocReorder.apply(expr);
  static_assert(match(result, (a + b) + c), "Should reorder by associativity");
  std::cout << "  ✓ Associativity: (a + c) + b → (a + b) + c\n";
}

// Test RewriteSystem with predicates
void testRewriteSystemWithPredicates() {
  std::cout << "Testing RewriteSystem with predicates...\n";

  constexpr auto rules = RewriteSystem{
    // Identity rules first
    Rewrite{0_c + x_, x_},
    Rewrite{x_ + 0_c, x_},
    // Ordering rule with predicate
    Rewrite{x_ + y_, y_ + x_, [](auto ctx) {
      return symbolicLessThan(get(ctx, y_), get(ctx, x_));
    }}
  };

  // Test 1: Identity
  constexpr auto expr1 = 0_c + a;
  constexpr auto result1 = rules.apply(expr1);
  static_assert(match(result1, a), "Should apply identity");
  std::cout << "  ✓ Identity: 0 + a → a\n";

  // Test 2: Ordering
  constexpr auto expr2 = b + a;
  constexpr auto result2 = rules.apply(expr2);
  static_assert(match(result2, a + b), "Should apply ordering");
  std::cout << "  ✓ Ordering: b + a → a + b\n";

  // Test 3: No change when already ordered
  constexpr auto expr3 = a + b;
  constexpr auto result3 = rules.apply(expr3);
  static_assert(match(result3, a + b), "Should not change");
  std::cout << "  ✓ No change: a + b → a + b\n";
}

// Test complex predicate with multiple checks
void testComplexPredicate() {
  std::cout << "Testing complex predicate...\n";

  // Rule: x * y → y * x if y < x AND both are not constants
  constexpr auto complexRule = Rewrite{
    x_ * y_,
    y_ * x_,
    [](auto ctx) {
      constexpr auto x_val = get(ctx, x_);
      constexpr auto y_val = get(ctx, y_);
      return symbolicLessThan(y_val, x_val) &&
             !match(x_val, AnyConstant{}) &&
             !match(y_val, AnyConstant{});
    }
  };

  // Test 1: b * a → a * b (both symbols)
  constexpr auto expr1 = b * a;
  constexpr auto result1 = complexRule.apply(expr1);
  static_assert(match(result1, a * b), "Should reorder symbols");
  std::cout << "  ✓ Complex: b * a → a * b\n";

  // Test 2: 2 * a → 2 * a (constant involved, no change)
  constexpr auto expr2 = 2_c * a;
  constexpr auto result2 = complexRule.apply(expr2);
  static_assert(match(result2, 2_c * a), "Should not reorder with constant");
  std::cout << "  ✓ Complex: 2 * a → 2 * a (no change due to constant)\n";
}

int main() {
  std::cout << "=== Pattern Matching Predicate Tests ===\n\n";

  testBasicRewrite();
  std::cout << "\n";

  testOrderingPredicate();
  std::cout << "\n";

  testMultiplicationOrdering();
  std::cout << "\n";

  testAssociativityOrdering();
  std::cout << "\n";

  testRewriteSystemWithPredicates();
  std::cout << "\n";

  testComplexPredicate();
  std::cout << "\n";

  std::cout << "=== All tests passed! ===\n";

  return 0;
}
