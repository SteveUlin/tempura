// Practical examples of displaying compile-time strings for debugging
// This file demonstrates real-world use cases for ShowStaticString

#include <print>
#include "meta/static_string_display.h"
#include "symbolic3/debug.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;
using tempura::StaticString;
using tempura::ShowStaticString;
using tempura::operator""_cts;

// =============================================================================
// USE CASE 1: Debugging why simplification didn't work as expected
// =============================================================================

void debug_simplification_issue() {
  constexpr auto x = Symbol<decltype([] {})>{};

  // You expected this to simplify to x^2, but does it?
  constexpr auto expr = x * x;
  constexpr auto result = simplify(expr, default_context());

  // Display to verify
  constexpr auto result_str = toString(result);

  // Uncomment to see what simplification produced:
  // SHOW_STATIC_STRING(result_str);
  //
  // You'll see: "x0 * x0" (not using power notation)
  // This confirms that power collection requires additional rules
}

// =============================================================================
// USE CASE 2: Debugging complex expression structure
// =============================================================================

void debug_complex_structure() {
  constexpr auto x = Symbol<decltype([] {})>{};
  constexpr auto y = Symbol<decltype([] {})>{};

  // Build a complex nested expression
  constexpr auto f = sin(x * y) / (cos(x) + exp(y));

  // What does the raw expression look like?
  constexpr auto raw_str = toString(f);
  // SHOW_STATIC_STRING(raw_str);
  //
  // Shows: "sin(x0 * x1) / (cos(x0) + exp(x1))"

  // After simplification
  constexpr auto simplified = simplify(f, default_context());
  constexpr auto clean_str = toString(simplified);
  // SHOW_STATIC_STRING(clean_str);
}

// =============================================================================
// USE CASE 3: Comparing before and after simplification
// =============================================================================

void trace_simplification_stages() {
  constexpr auto x = Symbol<decltype([] {})>{};

  constexpr auto expr = x * Constant<1>{} + Constant<0>{};

  // Stage 1: Original
  constexpr auto stage1 = toString(expr);
  // SHOW_STATIC_STRING(stage1);  // "x0 * 1 + 0"

  // Stage 2: After full simplification
  constexpr auto after_simp = simplify(expr, default_context());
  constexpr auto stage2 = toString(after_simp);
  // SHOW_STATIC_STRING(stage2);  // "x0"

  // This helps verify simplification is working correctly
}

// =============================================================================
// USE CASE 4: Debugging custom symbol names
// =============================================================================

void debug_symbol_naming() {
  constexpr auto alpha = Symbol<decltype([] {})>{};
  constexpr auto beta = Symbol<decltype([] {})>{};
  constexpr auto gamma = Symbol<decltype([] {})>{};

  constexpr auto expr = alpha * beta + gamma;

  // Test default naming
  constexpr auto default_name = toString(expr);
  // SHOW_STATIC_STRING(default_name);  // "x0 * x1 + x2"

  // Test custom naming with Greek letters
  constexpr auto ctx1 = makeSymbolNames(
      alpha, "α"_cts,
      beta, "β"_cts,
      gamma, "γ"_cts
  );
  constexpr auto greek = toString(expr, ctx1);
  // SHOW_STATIC_STRING(greek);  // "α * β + γ"

  // Test descriptive names
  constexpr auto ctx2 = makeSymbolNames(
      alpha, "position"_cts,
      beta, "velocity"_cts,
      gamma, "acceleration"_cts
  );
  constexpr auto descriptive = toString(expr, ctx2);
  // SHOW_STATIC_STRING(descriptive);  // "position * velocity + acceleration"

  // Verify the last one matches expected
  static_assert(descriptive == "position * velocity + acceleration"_cts);
}

// =============================================================================
// USE CASE 5: Testing new operator display traits
// =============================================================================

void test_operator_display() {
  constexpr auto x = Symbol<decltype([] {})>{};

  // Testing transcendental functions
  constexpr auto trig_expr = sin(x) + cos(x) + tan(x);
  constexpr auto trig_str = toString(trig_expr);
  // SHOW_STATIC_STRING(trig_str);  // "sin(x0) + cos(x0) + tan(x0)"

  // Testing hyperbolic functions
  constexpr auto hyp_expr = sinh(x) * cosh(x) / tanh(x);
  constexpr auto hyp_str = toString(hyp_expr);
  // SHOW_STATIC_STRING(hyp_str);  // "sinh(x0) * cosh(x0) / tanh(x0)"

  // Testing operator precedence
  constexpr auto prec_expr = x + x * x;  // Should NOT add parens around x + x
  constexpr auto prec_str = toString(prec_expr);
  // SHOW_STATIC_STRING(prec_str);  // "x0 + x0 * x0" (correct precedence)

  constexpr auto prec_expr2 = (x + x) * x;  // Should have parens
  constexpr auto prec_str2 = toString(prec_expr2);
  // SHOW_STATIC_STRING(prec_str2);  // "(x0 + x0) * x0" (parens preserved)
}

// =============================================================================
// USE CASE 6: Comparing expressions for equality
// =============================================================================

void compare_expression_equality() {
  constexpr auto x = Symbol<decltype([] {})>{};

  // Two algebraically equivalent expressions
  constexpr auto expr1 = x + x;
  constexpr auto expr2 = Constant<2>{} * x;

  // Are they structurally equal?
  constexpr bool same_type = std::is_same_v<decltype(expr1), decltype(expr2)>;
  static_assert(!same_type);  // No, different structure

  // What do they look like?
  constexpr auto str1 = toString(expr1);
  constexpr auto str2 = toString(expr2);

  // SHOW_STATIC_STRING(str1);  // "x0 + x0"
  // SHOW_STATIC_STRING(str2);  // "2 * x0"

  // After simplification, do they become equal?
  constexpr auto simp1 = simplify(expr1, default_context());
  constexpr auto simp2 = simplify(expr2, default_context());

  constexpr auto simp_str1 = toString(simp1);
  constexpr auto simp_str2 = toString(simp2);

  // SHOW_STATIC_STRING(simp_str1);
  // SHOW_STATIC_STRING(simp_str2);
}

// =============================================================================
// MAIN - Demonstrates all use cases
// =============================================================================

int main() {
  // All these functions are valid when ShowStaticString calls are commented
  debug_simplification_issue();
  debug_complex_structure();
  trace_simplification_stages();
  debug_symbol_naming();
  test_operator_display();
  compare_expression_equality();

  std::print("All examples completed successfully.\n");
  std::print("Uncomment SHOW_STATIC_STRING lines to see compile-time string display.\n");

  return 0;
}
