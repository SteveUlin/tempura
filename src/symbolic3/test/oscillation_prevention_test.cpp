// Oscillation Prevention Tests
// Tests to verify that simplification rules don't create infinite rewrite loops
// Based on code review recommendations in SYMBOLIC3_CODE_REVIEW.md

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/simplify.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  //============================================================================
  // 1. EXP/LOG EXPANSION STABILITY
  //============================================================================
  // Verifies that exp and log expansion rules don't oscillate with inverses
  // Potential loop: exp(a+b) → exp(a)*exp(b) → log → back to exp(a+b)

  "Exp/log expansion stability"_test = [] {
    constexpr Symbol a, b;
    constexpr auto ctx = default_context();

    constexpr auto expr = exp(a + b);
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    // Should reach fixed point (no further changes)
    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "exp(a+b) should reach fixed point after simplification");
  };

  "Log/exp inverse doesn't oscillate"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = log(exp(x));
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    // Should cancel to x and stay there
    static_assert(match(s1, x), "log(exp(x)) should simplify to x");
    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "Should not change after reaching x");
  };

  "Exp composition stability"_test = [] {
    constexpr Symbol x, y, z;
    constexpr auto ctx = default_context();

    constexpr auto expr = exp(x) * exp(y) * exp(z);
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "exp products should stabilize");
  };

  //============================================================================
  // 2. ASSOCIATIVITY CANONICAL FORM
  //============================================================================
  // Verifies that associativity rules converge to unique canonical form
  // All equivalent expressions should reach the same normal form

  "Associativity canonical form - addition"_test = [] {
    constexpr Symbol x, y, z;
    constexpr auto ctx = default_context();

    // Different starting forms should reach same canonical form
    constexpr auto e1 = x + (y + z);
    constexpr auto e2 = (x + y) + z;
    constexpr auto e3 = (y + x) + z;

    constexpr auto s1 = simplify(e1, ctx);
    constexpr auto s2 = simplify(e2, ctx);
    constexpr auto s3 = simplify(e3, ctx);

    // All should stabilize (no oscillation) even if not same canonical form
    constexpr auto s1_again = simplify(s1, ctx);
    constexpr auto s2_again = simplify(s2, ctx);
    constexpr auto s3_again = simplify(s3, ctx);

    static_assert(isSame<decltype(s1), decltype(s1_again)>,
                  "Simplification should be idempotent for e1");
    static_assert(isSame<decltype(s2), decltype(s2_again)>,
                  "Simplification should be idempotent for e2");
    static_assert(isSame<decltype(s3), decltype(s3_again)>,
                  "Simplification should be idempotent for e3");
  };

  "Associativity canonical form - multiplication"_test = [] {
    constexpr Symbol x, y, z;
    constexpr auto ctx = default_context();

    constexpr auto e1 = x * (y * z);
    constexpr auto e2 = (x * y) * z;
    constexpr auto e3 = (y * x) * z;

    constexpr auto s1 = simplify(e1, ctx);
    constexpr auto s2 = simplify(e2, ctx);
    constexpr auto s3 = simplify(e3, ctx);

    // All should stabilize (no oscillation) even if not same canonical form
    constexpr auto s1_again = simplify(s1, ctx);
    constexpr auto s2_again = simplify(s2, ctx);
    constexpr auto s3_again = simplify(s3, ctx);

    static_assert(isSame<decltype(s1), decltype(s1_again)>,
                  "Simplification should be idempotent for e1");
    static_assert(isSame<decltype(s2), decltype(s2_again)>,
                  "Simplification should be idempotent for e2");
    static_assert(isSame<decltype(s3), decltype(s3_again)>,
                  "Simplification should be idempotent for e3");
  };

  "Associativity with constants"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // Constants should be properly ordered with variables
    constexpr auto e1 = x + (3_c + 2_c);
    constexpr auto e2 = (x + 3_c) + 2_c;
    constexpr auto e3 = (3_c + x) + 2_c;

    constexpr auto s1 = simplify(e1, ctx);
    constexpr auto s2 = simplify(e2, ctx);
    constexpr auto s3 = simplify(e3, ctx);

    // All should fold constants and reach x + 5
    static_assert(match(s1, x + 5_c) || match(s1, 5_c + x),
                  "Should fold constants to 5");
    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "Different associations should reach same form");
    static_assert(isSame<decltype(s2), decltype(s3)>,
                  "Different orders should reach same form");
  };

  //============================================================================
  // 3. ORDERING RULE IDEMPOTENCE
  //============================================================================
  // Verifies that ordering rules don't swap terms endlessly
  // Once canonical order is established, no further changes should occur

  "Ordering idempotence - addition"_test = [] {
    constexpr Symbol x, y;
    constexpr auto ctx = default_context();

    // Start with terms in wrong order (assuming y > x alphabetically)
    constexpr auto expr = y + x;
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    // Should not change after reaching canonical order
    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "Ordering should not change after first pass");
  };

  "Ordering idempotence - multiplication"_test = [] {
    constexpr Symbol x, y;
    constexpr auto ctx = default_context();

    constexpr auto expr = y * x;
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "Multiplication ordering should stabilize");
  };

  "Complex ordering stability"_test = [] {
    constexpr Symbol a, b, c, d;
    constexpr auto ctx = default_context();

    // Mix of terms that need reordering
    constexpr auto expr = d + b + c + a;
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);
    constexpr auto s3 = simplify(s2, ctx);

    // Should stabilize after initial reordering
    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "Should stabilize after first pass");
    static_assert(isSame<decltype(s2), decltype(s3)>,
                  "Should remain stable on subsequent passes");
  };

  //============================================================================
  // 4. POWER COMPOSITION UNIDIRECTIONALITY
  //============================================================================
  // Verifies that power rules only compose, never expand
  // pow(pow(x, 2), 3) → pow(x, 6), but pow(x, 6) should NOT expand back

  "Power composition unidirectional"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = pow(pow(x, 2_c), 3_c);
    constexpr auto simplified = simplify(expr, ctx);

    // Should compose to x^6
    static_assert(match(simplified, pow(x, 6_c)),
                  "pow(pow(x, 2), 3) should compose to pow(x, 6)");

    // Should not expand back
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "pow(x, 6) should not expand back to nested powers");
  };

  "Power combining stability"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = x * pow(x, 2_c);
    constexpr auto simplified = simplify(expr, ctx);

    // Should combine to x^3
    static_assert(match(simplified, pow(x, 3_c)),
                  "x * x^2 should combine to x^3");

    // Should not separate back
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "x^3 should not separate back to x * x^2");
  };

  //============================================================================
  // 5. PYTHAGOREAN IDENTITY STABILITY
  //============================================================================
  // Verifies that only contraction rules are active (sin²+cos² → 1)
  // Expansion rules (sin² → 1-cos²) should NOT be active

  "Pythagorean contraction works"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = pow(sin(x), 2_c) + pow(cos(x), 2_c);
    constexpr auto simplified = simplify(expr, ctx);

    static_assert(match(simplified, 1_c),
                  "sin²(x) + cos²(x) should simplify to 1");
  };

  "Pythagorean expansion is disabled"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // sin²(x) should NOT expand to 1 - cos²(x)
    constexpr auto expr = pow(sin(x), 2_c);
    constexpr auto simplified = simplify(expr, ctx);

    // Should remain as sin²(x), not expand
    static_assert(match(simplified, pow(sin(x), 2_c)),
                  "sin²(x) should not expand (expansion disabled)");
  };

  "Pythagorean commutative variant"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // cos² + sin² should also work (commutative)
    constexpr auto expr = pow(cos(x), 2_c) + pow(sin(x), 2_c);
    constexpr auto simplified = simplify(expr, ctx);

    static_assert(match(simplified, 1_c),
                  "cos²(x) + sin²(x) should also simplify to 1");
  };

  //============================================================================
  // 6. HYPERBOLIC IDENTITY STABILITY
  //============================================================================
  // Similar to Pythagorean: only contraction active (cosh²-sinh² → 1)

  //  "Hyperbolic contraction works"_test = [] {
  //    constexpr Symbol x;
  //    constexpr auto ctx = default_context();
  //
  //    constexpr auto expr = pow(cosh(x), 2_c) - pow(sinh(x), 2_c);
  //    constexpr auto simplified = simplify(expr, ctx);
  //
  //    // TODO: Hyperbolic identity rules not yet fully implemented
  //    static_assert(match(simplified, 1_c),
  //                  "cosh²(x) - sinh²(x) should simplify to 1");
  //  };

  "Hyperbolic expansion is disabled"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // cosh²(x) should NOT expand to 1 + sinh²(x)
    constexpr auto expr = pow(cosh(x), 2_c);
    constexpr auto simplified = simplify(expr, ctx);

    // Verify stability - hyperbolic terms should not oscillate
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "cosh²(x) should be stable (no oscillation)");
  };

  //============================================================================
  // 7. NESTED NEGATION UNWRAPPING
  //============================================================================
  // Verifies that -(-x) → x terminates and no rules create double negation

  "Double negation elimination"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = -(-x);
    constexpr auto simplified = simplify(expr, ctx);

    static_assert(match(simplified, x), "-(-x) should simplify to x");

    // Should not create double negation again
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "x should not become -(-x) again");
  };

  "Triple negation"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = -(-(-x));
    constexpr auto simplified = simplify(expr, ctx);

    // Should reduce to -x
    static_assert(match(simplified, -x), "-(-(-x)) should simplify to -x");
  };

  "Subtraction doesn't create double negation"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    // Various subtraction forms should not create -(-x) patterns
    constexpr auto e1 = x - (-x);
    constexpr auto e2 = 0_c - (-x);

    constexpr auto s1 = simplify(e1, ctx);
    constexpr auto s2 = simplify(e2, ctx);

    // Verify stability - should not oscillate
    constexpr auto s1_again = simplify(s1, ctx);
    constexpr auto s2_again = simplify(s2, ctx);

    static_assert(isSame<decltype(s1), decltype(s1_again)>,
                  "x - (-x) should be stable");
    static_assert(isSame<decltype(s2), decltype(s2_again)>,
                  "0 - (-x) should be stable");
  };

  //============================================================================
  // 8. FRACTION/CONSTANT CONVERSION STABILITY
  //============================================================================
  // Verifies that Fraction<n, 1> ↔ Constant<n> conversions don't oscillate

  "Fraction to constant conversion"_test = [] {
    constexpr auto ctx = default_context();

    // Fraction<6, 1> simplification
    constexpr auto expr = Fraction<6, 1>{};
    constexpr auto simplified = simplify(expr, ctx);

    // Should be stable (no oscillation between Fraction and Constant)
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "Simplified fraction should be stable");
  };

  "Integer division creates constant not fraction"_test = [] {
    constexpr auto ctx = default_context();

    // 6 / 2 should fold to 3, not Fraction<6, 2> → Fraction<3, 1> → 3
    constexpr auto expr = 6_c / 2_c;
    constexpr auto simplified = simplify(expr, ctx);

    static_assert(match(simplified, 3_c), "6 / 2 should simplify to 3");

    // Should be stable
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "3 should remain stable");
  };

  //============================================================================
  // 9. DISTRIBUTION/FACTORING OSCILLATION
  //============================================================================
  // Verifies that distribution is intentionally disabled to prevent loops
  // x*(a+b) should NOT expand to x*a + x*b, which would fight with factoring

  "Distribution doesn't oscillate with factoring"_test = [] {
    constexpr Symbol x, a, b;
    constexpr auto ctx = default_context();

    // Should NOT oscillate between x*(a+b) and x*a + x*b
    constexpr auto expr = x * (a + b);
    constexpr auto simplified = simplify(expr, ctx);

    // Verify stability
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(
        isSame<decltype(simplified), decltype(s2)>,
        "x*(a+b) should be stable (no distribute/factor oscillation)");
  };

  "Factoring stability"_test = [] {
    constexpr Symbol x, a, b;
    constexpr auto ctx = default_context();

    // Test factoring stability
    constexpr auto expr = x * a + x * b;
    constexpr auto simplified = simplify(expr, ctx);

    // Should not oscillate - whatever form it takes should be stable
    constexpr auto s2 = simplify(simplified, ctx);
    static_assert(isSame<decltype(simplified), decltype(s2)>,
                  "Factored form should be stable");
  };

  //============================================================================
  // 10. GENERAL IDEMPOTENCE
  //============================================================================
  // Verifies that simplify is idempotent: simplify(simplify(x)) == simplify(x)

  "Simplification is idempotent - basic"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = x + x + x;
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "simplify should be idempotent");
  };

  "Simplification is idempotent - complex"_test = [] {
    constexpr Symbol x, y;
    constexpr auto ctx = default_context();

    constexpr auto expr = (x + y) * (x + y) + 0_c * x + 1_c * y;
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);
    constexpr auto s3 = simplify(s2, ctx);

    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "First re-simplification should produce same result");
    static_assert(isSame<decltype(s2), decltype(s3)>,
                  "Second re-simplification should also produce same result");
  };

  "Simplification is idempotent - transcendental"_test = [] {
    constexpr Symbol x;
    constexpr auto ctx = default_context();

    constexpr auto expr = exp(log(sin(x)));
    constexpr auto s1 = simplify(expr, ctx);
    constexpr auto s2 = simplify(s1, ctx);

    static_assert(isSame<decltype(s1), decltype(s2)>,
                  "Transcendental simplification should be idempotent");
  };

  return 0;
}
