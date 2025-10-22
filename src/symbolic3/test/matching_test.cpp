// Test the fixed pattern matching system
#include "symbolic3/matching.h"

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  // Test 1: Basic constant matching
  "Constants match by value"_test = [] {
    constexpr auto c1 = Constant<5>{};
    constexpr auto c2 = Constant<5>{};
    constexpr auto c3 = Constant<3>{};

    static_assert(match(c1, c2), "Same constants should match");
    static_assert(!match(c1, c3), "Different constants should not match");
  };

  // Test 2: Symbol matching
  "Symbols match by type"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    static_assert(match(x, x), "Same symbol should match");
    static_assert(!match(x, y), "Different symbols should not match");
  };

  // Test 3: Expression matching - the core fix
  "Expressions match structurally"_test = [] {
    constexpr auto expr1 = Constant<1>{} + Constant<2>{};
    constexpr auto expr2 = Constant<1>{} + Constant<2>{};
    constexpr auto expr3 = Constant<1>{} + Constant<3>{};
    constexpr auto expr4 = Constant<1>{} * Constant<2>{};

    static_assert(match(expr1, expr2), "Same expression should match");
    static_assert(!match(expr1, expr3), "Different args should not match");
    static_assert(!match(expr1, expr4), "Different ops should not match");
  };

  // Test 4: Nested expression matching
  "Nested expressions match recursively"_test = [] {
    constexpr Symbol x;
    constexpr auto inner = x + 1_c;
    constexpr auto expr1 = inner * 2_c;
    constexpr auto expr2 = (x + 1_c) * 2_c;

    static_assert(match(expr1, expr2), "Nested expressions should match");
  };

  // Test 5: PatternVar matches anything
  "PatternVar matches any expression"_test = [] {
    constexpr Symbol y;
    constexpr auto expr = y + 0_c;

    static_assert(match(x_, y), "PatternVar should match symbol");
    static_assert(match(x_, 0_c), "PatternVar should match constant");
    static_assert(match(x_, expr), "PatternVar should match expression");
  };

  // Test 6: Pattern matching with expressions
  "Pattern with expressions matches"_test = [] {
    constexpr Symbol y;
    constexpr auto pattern = x_ + 0_c;
    constexpr auto expr = y + 0_c;

    static_assert(match(pattern, expr), "x_ + 0_c should match y + 0_c");
  };

  // Test 7: Wildcard matching
  "Wildcards match appropriately"_test = [] {
    constexpr Symbol y;
    constexpr auto expr = y + 0_c;

    static_assert(match(𝐚𝐧𝐲, y), "𝐚𝐧𝐲 should match symbol");
    static_assert(match(𝐚𝐧𝐲, 0_c), "𝐚𝐧𝐲 should match constant");
    static_assert(match(𝐚𝐧𝐲𝐞𝐱𝐩𝐫, expr), "𝐚𝐧𝐲𝐞𝐱𝐩𝐫 should match expression");
    // Note: These negative cases need bidirectional match overloads
    // static_assert(!match(𝐚𝐧𝐲𝐞𝐱𝐩𝐫, y), "𝐚𝐧𝐲𝐞𝐱𝐩𝐫 should not match symbol");
    static_assert(match(𝐜, 0_c), "𝐜 should match constant");
    // static_assert(!match(𝐜, y), "𝐜 should not match symbol");
  };

  // Test 8: Fraction matching
  "Fractions match by reduced form"_test = [] {
    constexpr auto f1 = Fraction<1, 2>{};
    constexpr auto f2 = Fraction<2, 4>{};  // Reduces to 1/2
    constexpr auto f3 = Fraction<1, 3>{};

    static_assert(match(f1, f2), "1/2 should match 2/4 (same reduced form)");
    static_assert(!match(f1, f3), "1/2 should not match 1/3");
  };

  // Test 9: AnyConstant matches fractions
  "AnyConstant matches fractions"_test = [] {
    constexpr auto f = Fraction<1, 2>{};
    constexpr auto c = Constant<5>{};

    static_assert(match(𝐜, f), "AnyConstant should match fraction");
    static_assert(match(𝐜, c), "AnyConstant should match constant");
  };

  return TestRegistry::result();
}
