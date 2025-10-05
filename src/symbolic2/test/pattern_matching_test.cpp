// Test for pattern matching functionality

#include "../pattern_matching.h"
#include "../operators.h"
#include "../constants.h"
#include "../core.h"
#include "../to_string.h"
#include <print>

using namespace tempura;

// Helper to print expressions with their string representation
template <Symbolic T>
void printExpr(const char* label, T expr) {
  auto str = toString(expr);
  std::println("  {} = {}", label, str.chars_);
}

int main() {
  std::println("=== Pattern Matching Tests ===\n");

  // Test 1: Pattern variables work with operators
  {
    std::println("Test 1: Can create patterns with pattern variables");
    constexpr auto pattern = pow(x_, n_);
    constexpr auto expr = pow(Symbol{}, 2_c);
    constexpr bool matches = match(pattern, expr);
    std::println("  pow(x_, n_) matches pow(Symbol, 2_c): {}", matches);
    std::println("  ✓ Pattern creation works\n");
  }

  // Test 2: Pattern variables match anything
  {
    std::println("Test 2: Pattern variables match any expression");
    constexpr auto s = Symbol{};
    static_assert(match(x_, s), "Pattern variable should match symbol");
    static_assert(match(x_, 5_c), "Pattern variable should match constant");
    static_assert(match(x_, s + 2_c), "Pattern variable should match expression");
    std::println("  ✓ Pattern variables match anything\n");
  }

  // Test 3: Patterns with specific constants
  {
    std::println("Test 3: Patterns with specific values");
    constexpr auto pattern = pow(x_, 0_c);
    constexpr auto s = Symbol{};

    constexpr bool match1 = match(pattern, pow(s, 0_c));
    constexpr bool match2 = match(pattern, pow(s, 1_c));
    constexpr bool match3 = match(pattern, pow(s, 2_c));

    std::println("  pow(x_, 0_c) matches pow(s, 0_c): {} (expect 1)", match1);
    std::println("  pow(x_, 0_c) matches pow(s, 1_c): {} (expect 0)", match2);
    std::println("  pow(x_, 0_c) matches pow(s, 2_c): {} (expect 0)", match3);

    if (match1 && !match2 && !match3) {
      std::println("  ✓ Specific constant matching works\n");
    } else {
      std::println("  ✗ FAILED - specific constant matching broken\n");
      return 1;
    }
  }

  // Test 4: Nested patterns
  {
    std::println("Test 4: Nested patterns");
    constexpr auto pattern = pow(pow(x_, a_), b_);
    constexpr auto s = Symbol{};
    constexpr auto expr = pow(pow(s, 2_c), 3_c);

    constexpr bool matches = match(pattern, expr);
    std::println("  pow(pow(x_, a_), b_) matches pow(pow(s, 2), 3): {}", matches);

    if (matches) {
      std::println("  ✓ Nested patterns work\n");
    } else {
      std::println("  ✗ FAILED - nested patterns broken\n");
      return 1;
    }
  }

  // Test 5: Substitution (basic)
  {
    std::println("Test 5: Basic substitution");
    constexpr auto s1 = Symbol{};
    constexpr auto s2 = Symbol{};

    // Substitute x_ with s1 (new API: pass PatternVar, value pairs)
    constexpr auto result1 = substitute(x_, x_, s1);
    constexpr bool is_s1 = std::same_as<decltype(result1), decltype(s1)>;
    std::println("  substitute(x_, x_, s1) returns s1: {}", is_s1);

    // Substitute in expression (pass PatternVar, value pairs)
    constexpr auto expr = x_ + y_;
    constexpr auto result2 = substitute(expr, x_, s1, y_, s2);
    // Should be s1 + s2
    using Result2Type = std::remove_cvref_t<decltype(result2)>;
    using ExpectedType = std::remove_cvref_t<decltype(s1 + s2)>;
    constexpr bool matches = std::same_as<Result2Type, ExpectedType>;
    std::println("  substitute(x_ + y_, x_, s1, y_, s2) creates expression: {}",
              matches ? "✓" : "✗");
    std::println("");
  }

  // Test 6: Rewrite rules
  {
    std::println("Test 6: Rewrite rules");
    // Use CTAD (Class Template Argument Deduction) instead of decltype
    constexpr Rewrite PowerZero{pow(x_, 0_c), 1_c};

    constexpr auto s = Symbol{};
    constexpr auto expr = pow(s, 0_c);

    constexpr bool matches = PowerZero.matches(expr);
    std::println("  PowerZero matches pow(s, 0_c): {}", matches);

    if (matches) {
      constexpr auto result = PowerZero.apply(expr);
      // Note: decltype(1_c) is const-qualified (prvalue from literal)
      // So we need to remove cv-qualifiers for type comparison
      using ResultType = std::remove_cvref_t<decltype(result)>;
      using ExpectedType = std::remove_cvref_t<decltype(1_c)>;
      constexpr bool is_one = std::same_as<ResultType, ExpectedType>;

      std::println("  PowerZero::apply returns 1_c: {}", is_one);

      if (is_one) {
        std::println("  ✓ Rewrite rules work\n");
      } else {
        std::println("  ✗ FAILED - apply didn't return correct type\n");
        return 1;
      }
    } else {
      std::println("  ✗ FAILED - pattern didn't match\n");
      return 1;
    }
  }

  // Test 7: Rewrite system
  {
    std::println("Test 7: Rewrite system");
    // No decltype needed! Constructor + CTAD does everything
    constexpr auto PowerRules = RewriteSystem{
      Rewrite{pow(x_, 0_c), 1_c},
      Rewrite{pow(x_, 1_c), x_}
    };

    constexpr auto s = Symbol{};
    constexpr auto expr1 = pow(s, 0_c);
    constexpr auto expr2 = pow(s, 1_c);
    constexpr auto expr3 = pow(s, 2_c);

    constexpr auto result1 = PowerRules.apply(expr1);
    constexpr auto result2 = PowerRules.apply(expr2);
    constexpr auto result3 = PowerRules.apply(expr3);

    constexpr bool test1 = std::same_as<std::remove_cvref_t<decltype(result1)>, std::remove_cvref_t<decltype(1_c)>>;
    // Test 2: pow(s, 1) should be replaced with s (the value bound to x_), not x_ itself
    constexpr bool test2 = std::same_as<std::remove_cvref_t<decltype(result2)>, std::remove_cvref_t<decltype(s)>>;
    constexpr bool test3 = std::same_as<std::remove_cvref_t<decltype(result3)>, std::remove_cvref_t<decltype(expr3)>>;

    std::println("  pow(s, 0) → 1_c: {}", test1);
    std::println("  pow(s, 1) → s (substituted): {}", test2);
    std::println("  pow(s, 2) → unchanged: {}", test3);

    if (test1 && test2 && test3) {
      std::println("  ✓ Rewrite system works\n");
    } else {
      std::println("  ✗ FAILED - rewrite system broken\n");
      return 1;
    }
  }

  // Test 8: Complex transformations with expression printing
  {
    std::println("Test 8: Complex algebraic transformations\n");

    // Rule: x + x → 2 * x (using CTAD)
    constexpr Rewrite DoubleAddition{x_ + x_, 2_c * x_};

    constexpr auto a = Symbol{};
    constexpr auto expr_add = a + a;
    constexpr auto result_add = DoubleAddition.apply(expr_add);

    std::println("  Transformation: x + x → 2 * x");
    printExpr("    Input ", expr_add);
    printExpr("    Output", result_add);

    // Check if transformation worked correctly
    constexpr auto expected = 2_c * a;
    constexpr bool correct_add = std::same_as<
      std::remove_cvref_t<decltype(result_add)>,
      std::remove_cvref_t<decltype(expected)>
    >;
    std::println("    Result matches 2*x: {}", correct_add ? "✓" : "✗");

    std::println("");

    // Rule: x * x → x^2 (using CTAD)
    constexpr Rewrite SquareRule{x_ * x_, pow(x_, 2_c)};

    constexpr auto b = Symbol{};
    constexpr auto expr_mul = b * b;
    constexpr auto result_mul = SquareRule.apply(expr_mul);

    std::println("  Transformation: x * x → x^2");
    printExpr("    Input ", expr_mul);
    printExpr("    Output", result_mul);

    constexpr auto expected_sq = pow(b, 2_c);
    constexpr bool correct_mul = std::same_as<
      std::remove_cvref_t<decltype(result_mul)>,
      std::remove_cvref_t<decltype(expected_sq)>
    >;
    std::println("    Result matches x^2: {}", correct_mul ? "✓" : "✗");

    std::println("");

    // Combined rewrite system for algebraic simplification
    // No decltype needed anywhere! Pure CTAD with constructor
    constexpr auto AlgebraRules = RewriteSystem{
      Rewrite{x_ + 0_c, x_},           // x + 0 → x
      Rewrite{x_ * 0_c, 0_c},          // x * 0 → 0
      Rewrite{x_ * 1_c, x_},           // x * 1 → x
      Rewrite{x_ + x_, 2_c * x_},      // x + x → 2*x
      Rewrite{pow(x_, 0_c), 1_c},      // x^0 → 1
      Rewrite{pow(x_, 1_c), x_}        // x^1 → x
    };

    constexpr auto c = Symbol{};

    // Test various expressions
    constexpr auto test_expr1 = c + 0_c;
    constexpr auto test_expr2 = c * 1_c;
    constexpr auto test_expr3 = c + c;

    constexpr auto simplified1 = AlgebraRules.apply(test_expr1);
    constexpr auto simplified2 = AlgebraRules.apply(test_expr2);
    constexpr auto simplified3 = AlgebraRules.apply(test_expr3);

    std::println("  Algebraic simplification system:");
    printExpr("    c + 0 →", simplified1);
    printExpr("    c * 1 →", simplified2);
    printExpr("    c + c →", simplified3);

    constexpr bool all_correct =
      std::same_as<std::remove_cvref_t<decltype(simplified1)>, std::remove_cvref_t<decltype(c)>> &&
      std::same_as<std::remove_cvref_t<decltype(simplified2)>, std::remove_cvref_t<decltype(c)>> &&
      std::same_as<std::remove_cvref_t<decltype(simplified3)>, std::remove_cvref_t<decltype(2_c * c)>>;

    if (correct_add && correct_mul && all_correct) {
      std::println("  ✓ Complex transformations work!\n");
    } else {
      std::println("  ✗ FAILED - some transformations incorrect\n");
      return 1;
    }
  }

  std::println("=== All Tests Passed! ===");
  return 0;
}
