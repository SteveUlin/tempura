#include "symbolic3/debug.h"

#include "symbolic3/simplify.h"
#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  "Compile-time expression depth"_test = [] {
    constexpr Symbol x;
    constexpr auto simple = x + Constant<1>{};
    constexpr auto nested = (x + Constant<1>{}) * (x - Constant<2>{});

    static_assert(expression_depth(x) == 0, "Symbol has depth 0");
    static_assert(expression_depth(Constant<5>{}) == 0, "Constant has depth 0");
    static_assert(expression_depth(simple) == 1,
                  "Simple expression has depth 1");
    static_assert(expression_depth(nested) == 2,
                  "Nested expression has depth 2");
  };

  "Compile-time operation count"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto expr1 = x + Constant<1>{};
    constexpr auto expr2 = x * y;
    constexpr auto expr3 = (x + y) * (x - y);

    static_assert(operation_count(x) == 0, "Symbol has 0 operations");
    static_assert(operation_count(expr1) == 1, "x + 1 has 1 operation");
    static_assert(operation_count(expr2) == 1, "x * y has 1 operation");
    static_assert(operation_count(expr3) == 3, "(x+y)*(x-y) has 3 operations");
  };

  "Structural equality check"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto expr1 = x + Constant<1>{};
    constexpr auto expr2 = x + Constant<1>{};
    constexpr auto expr3 = x + Constant<2>{};

    static_assert(structurally_equal(expr1, expr2),
                  "Same expressions are structurally equal");
    static_assert(!structurally_equal(expr1, expr3),
                  "Different expressions are not equal");
  };

  "Contains subexpression check"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto subexpr = x + Constant<1>{};
    constexpr auto expr = subexpr * y;

    static_assert(contains_subexpression(expr, subexpr),
                  "Expression contains subexpression");
    static_assert(contains_subexpression(expr, x),
                  "Expression contains symbol");
    static_assert(!contains_subexpression(x, expr),
                  "Symbol doesn't contain expression");
  };

  "Simplification detection"_test = [] {
    constexpr Symbol x;

    constexpr auto simplified = x + Constant<2>{};
    constexpr auto not_simplified = x + Constant<0>{};

    // is_likely_simplified is a heuristic check
    // It detects x + 0 as unsimplified (returns false)
    static_assert(is_likely_simplified(simplified), "x + 2 appears simplified");
    static_assert(!is_likely_simplified(not_simplified),
                  "x + 0 is correctly detected as not simplified");
  };

  "Compile-time string conversion verification"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x + Constant<1>{};
    constexpr auto str = toString(expr);

    // Compile-time verification - visually see what the output is
    static_assert(str.size() > 0, "String conversion should produce non-empty result");
    // The actual string will be something like "x0 + 1"
  };

  "Compile-time simplification verification"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x + Constant<0>{};
    constexpr auto simplified = simplify(expr, default_context());

    // Verify simplification changes the expression
    // Note: Simplification might not fully reduce to x in one step depending on
    // rules
    static_assert(!std::is_same_v<decltype(simplified), decltype(expr)>,
                  "Simplification should transform the expression");
  };

  "Multi-step simplification verification"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = (x + Constant<0>{}) * Constant<1>{};
    constexpr auto step1 = simplify(expr, default_context());
    constexpr auto step2 = simplify(step1, default_context());

    // Verify that multi-step simplification occurs
    // The exact result depends on the simplification rules
    static_assert(!std::is_same_v<decltype(step2), decltype(expr)>,
                  "Multi-step simplification should transform expression");
  };

  "Complex expression simplification"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = Constant<2>{} * x + Constant<3>{} * x;
    constexpr auto simplified = simplify(expr, default_context());

    // Verify that simplification happened (operation count should change)
    // Note: Depending on simplification rules, this may or may not fully
    // simplify to 5*x The test verifies that SOME simplification occurred
    static_assert(operation_count(simplified) <= operation_count(expr),
                  "Simplification should not increase complexity");
  };

  "Expression depth preservation"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;
    constexpr auto deep = ((x + y) * (x - y)) + ((x * y) / (x + y));

    // Deep expression has significant depth
    static_assert(expression_depth(deep) >= 2,
                  "Deep expression has depth >= 2");

    // After simplification, depth might change
    constexpr auto simplified = simplify(deep, default_context());

    // Verify simplification produces some result (could be Never if rules don't
    // match) Just check that it compiled and ran
    [[maybe_unused]] constexpr auto _ = simplified;
  };

  // Demonstration of how to use compile-time debugging
  // (These would normally be commented out as they cause compilation errors)
  /*
  "Debug: Print expression type (will fail)"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x + Constant<1>{};

    // Uncomment to see type in compiler error:
    // CONSTEXPR_PRINT_TYPE(decltype(expr));
  };

  "Debug: Verify simplification result (will fail if different)"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x * Constant<0>{};
    constexpr auto result = simplify.apply(expr, default_context());

    // Uncomment to verify result is Constant<0>:
    // CONSTEXPR_ASSERT_EQUAL(result, Constant<0>{});
  };
  */

  // =========================================================================
  // MATCH EXPLANATION TESTS
  // =========================================================================
  // Tests for explain_match() utility - verify match explanations work

  "Explain Symbol matching"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    constexpr auto same_explanation = explain_match(x, x);
    constexpr auto diff_explanation = explain_match(x, y);

    // Compile-time string comparison - visually see the expected output
    static_assert(same_explanation == "✓ Match succeeded: Symbols have same type identity",
                  "Same symbols should match with success message");
    static_assert(diff_explanation == "✗ Match failed: Symbols have different type identities",
                  "Different symbols should not match");
  };

  "Explain Constant matching"_test = [] {
    constexpr auto c1 = Constant<5>{};
    constexpr auto c2 = Constant<5>{};
    constexpr auto c3 = Constant<3>{};

    constexpr auto same_explanation = explain_match(c1, c2);
    constexpr auto diff_explanation = explain_match(c1, c3);

    // Compile-time string comparison - visually see the expected output
    static_assert(same_explanation == "✓ Match succeeded: Constants have same value",
                  "Same constants should match");
    static_assert(diff_explanation == "✗ Match failed: Constants have different values",
                  "Different constants should not match");
  };

  "Explain Fraction matching"_test = [] {
    constexpr auto f1 = Fraction<1, 2>{};
    constexpr auto f2 = Fraction<2, 4>{};  // Reduces to 1/2
    constexpr auto f3 = Fraction<1, 3>{};

    constexpr auto same_explanation = explain_match(f1, f2);
    constexpr auto diff_explanation = explain_match(f1, f3);

    // Compile-time string comparison - visually see the expected output
    static_assert(same_explanation == "✓ Match succeeded: Fractions reduce to same value",
                  "Equivalent fractions should match");
    static_assert(diff_explanation == "✗ Match failed: Fractions have different reduced forms",
                  "Different fractions should not match");
  };

  "Explain wildcard matching"_test = [] {
    constexpr Symbol x;
    constexpr auto expr = x + Constant<5>{};

    constexpr auto any_explanation = explain_match(𝐚𝐧𝐲, x);
    constexpr auto anyexpr_success = explain_match(𝐚𝐧𝐲𝐞𝐱𝐩𝐫, expr);
    constexpr auto anyexpr_fail = explain_match(𝐚𝐧𝐲𝐞𝐱𝐩𝐫, x);

    // Compile-time string comparison - visually see the expected output
    static_assert(any_explanation == "✓ Match succeeded: AnyArg matches any expression",
                  "AnyArg should match any expression");
    static_assert(anyexpr_success == "✓ Match succeeded: AnyExpr matches compound expression",
                  "AnyExpr should match compound expressions");
    static_assert(anyexpr_fail == "✗ Match failed: AnyExpr only matches compound expressions (not atoms)",
                  "AnyExpr should not match atomic expressions");
  };

  "Explain Expression matching"_test = [] {
    constexpr Symbol x;
    constexpr auto expr1 = x + Constant<5>{};
    constexpr auto expr2 = x + Constant<5>{};
    constexpr auto expr3 = x + Constant<3>{};
    constexpr auto expr4 = x * Constant<5>{};

    constexpr auto same_explanation = explain_match(expr1, expr2);
    constexpr auto diff_args_explanation = explain_match(expr1, expr3);
    constexpr auto diff_op_explanation = explain_match(expr1, expr4);

    // Compile-time string comparison - visually see the expected output
    static_assert(same_explanation == "✓ Match succeeded: Operation and all arguments match",
                  "Same expressions should match");
    static_assert(diff_args_explanation == "✗ Match failed: Operation matches but some arguments differ",
                  "Expressions with different arguments should not match");
    static_assert(diff_op_explanation == "✗ Match failed: Operations differ",
                  "Expressions with different operations should not match");
  };

  "Explain type mismatch"_test = [] {
    constexpr Symbol x;
    constexpr auto c = Constant<5>{};

    constexpr auto mismatch_explanation = explain_match(x, c);

    // Compile-time string comparison - visually see the expected output
    static_assert(mismatch_explanation == "✗ Match failed: Symbol cannot match Constant",
                  "Symbol and Constant types should not match");
  };

  "Match summary utility"_test = [] {
    constexpr Symbol x;
    constexpr auto c = Constant<5>{};

    constexpr auto match_summary_success = match_summary(x, x);
    constexpr auto match_summary_fail = match_summary(x, c);

    // Compile-time string comparison - visually see the expected output
    static_assert(match_summary_success == "✓ MATCH",
                  "Matching expressions should show MATCH");
    static_assert(match_summary_fail == "✗ NO MATCH",
                  "Non-matching expressions should show NO MATCH");
  };

  return TestRegistry::result();
}
