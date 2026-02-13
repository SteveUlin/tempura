#include "symbolic4/simplify/canonicalize.h"

#include "symbolic4/constants.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/simplify/ordering.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Define symbols at file scope for constexpr usage
inline constexpr Symbol<struct X> x{};
inline constexpr Symbol<struct Y> y{};
inline constexpr Symbol<struct Z> z{};

auto main() -> int {
  "canonicalize produces same type for commuted addition"_test = [] {
    auto c1 = canonicalize(x + y);
    auto c2 = canonicalize(y + x);
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
  };

  "canonicalize produces same type for commuted multiplication"_test = [] {
    auto c1 = canonicalize(x * y);
    auto c2 = canonicalize(y * x);
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
  };

  "canonicalize preserves non-commutative ops"_test = [] {
    auto c1 = canonicalize(x - y);
    auto c2 = canonicalize(y - x);
    static_assert(!std::is_same_v<decltype(c1), decltype(c2)>);

    auto d1 = canonicalize(x / y);
    auto d2 = canonicalize(y / x);
    static_assert(!std::is_same_v<decltype(d1), decltype(d2)>);
  };

  "canonicalize works recursively"_test = [] {
    // Both inner additions should produce the same canonical form
    auto c1 = canonicalize((x + y) * z);
    auto c2 = canonicalize((y + x) * z);
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
  };

  "canonicalize with constants"_test = [] {
    // Symbol < Constant by our ordering
    static_assert(compare(x, 1_c) == Ordering::Less);

    auto c1 = canonicalize(x + 1_c);
    auto c2 = canonicalize(1_c + x);
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);

    // Canonical form should have x first (smaller)
    // Use std::remove_cvref to handle constexpr const qualification
    static_assert(std::is_same_v<decltype(c1),
                                 Expression<AddOp, std::remove_cvref_t<decltype(x)>,
                                            Constant<1.0>>>);
  };

  "canonicalize idempotent"_test = [] {
    auto e = y + x;
    auto c1 = canonicalize(e);
    auto c2 = canonicalize(c1);
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
  };

  "canonicalize with unary ops"_test = [] {
    auto e1 = -x;
    auto c1 = canonicalize(e1);
    static_assert(std::is_same_v<decltype(e1), decltype(c1)>);

    auto e2 = sin(x);
    auto c2 = canonicalize(e2);
    static_assert(std::is_same_v<decltype(e2), decltype(c2)>);
  };

  "canonicalize nested commutative"_test = [] {
    // (z * y) + (y * x) - both multiplications should be canonicalized
    auto c1 = canonicalize((z * y) + (y * x));
    auto c2 = canonicalize((y * z) + (x * y));
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
  };

  "canonicalize with fractions"_test = [] {
    // Atom < Fraction
    static_assert(compare(x, Fraction<1, 2>{}) == Ordering::Less);

    auto c1 = canonicalize(x * Fraction<1, 2>{});
    auto c2 = canonicalize(Fraction<1, 2>{} * x);
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
  };

  "canonicalize preserves literal values - addition"_test = [] {
    // Create two literals with distinct values
    auto a = 3.14_c;
    auto b = 2.71_c;

    // Create expressions with literals in different orders
    auto e1 = a + b;  // 3.14 + 2.71
    auto e2 = b + a;  // 2.71 + 3.14

    // Canonicalize both
    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Types should be the same after canonicalization
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);

    // But more importantly: the VALUES should be preserved correctly
    // Both should evaluate to the same result (addition is commutative)
    auto bindings = BinderPack{};
    double r1 = evaluate(c1, bindings);
    double r2 = evaluate(c2, bindings);

    expectNear(r1, 3.14 + 2.71, 1e-10);
    expectNear(r2, 3.14 + 2.71, 1e-10);
    expectEq(r1, r2);
  };

  "canonicalize preserves literal values - multiplication"_test = [] {
    auto a = 7.0_c;
    auto b = 3.0_c;

    auto e1 = a * b;  // 7 * 3
    auto e2 = b * a;  // 3 * 7

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);

    auto bindings = BinderPack{};
    expectNear(evaluate(c1, bindings), 21.0, 1e-10);
    expectNear(evaluate(c2, bindings), 21.0, 1e-10);
  };

  "canonicalize preserves literal values - nested expression"_test = [] {
    // More complex: (a * b) + (c * d) with different orderings
    auto a = 2.0_c;
    auto b = 3.0_c;
    auto c = 5.0_c;
    auto d = 7.0_c;

    // (2*3) + (5*7) = 6 + 35 = 41
    auto e1 = (a * b) + (c * d);
    // (3*2) + (7*5) = 6 + 35 = 41 (same values, different input order)
    auto e2 = (b * a) + (d * c);

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Types should be canonicalized to the same form
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);

    // Values should be preserved correctly
    auto bindings = BinderPack{};
    expectNear(evaluate(c1, bindings), 41.0, 1e-10);
    expectNear(evaluate(c2, bindings), 41.0, 1e-10);
  };

  "canonicalize preserves literal values - mixed with symbols"_test = [] {
    auto a = 10.0_c;
    auto b = 2.0_c;

    // a + x + b with different orderings
    auto e1 = a + x + b;  // ((10 + x) + 2)
    auto e2 = b + x + a;  // ((2 + x) + 10)

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Evaluate with x = 5
    auto bindings = BinderPack{x = 5.0};
    double r1 = evaluate(c1, bindings);
    double r2 = evaluate(c2, bindings);

    // Both should give 10 + 5 + 2 = 17
    expectNear(r1, 17.0, 1e-10);
    expectNear(r2, 17.0, 1e-10);
  };

  "canonicalize preserves distinct literal values in same expression"_test = [] {
    // With Constant<V>, each distinct value is a distinct type:
    // Constant<3.14159> ≠ Constant<2.71828>.
    auto pi_approx = 3.14159_c;
    auto e_approx = 2.71828_c;

    // Subtraction is NOT commutative, so swapped operands produce different types
    auto e1 = pi_approx - e_approx;  // 3.14159 - 2.71828 ≈ 0.42331
    auto e2 = e_approx - pi_approx;  // 2.71828 - 3.14159 ≈ -0.42331

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Different types — Sub is non-commutative, operands stay in place
    static_assert(!std::is_same_v<decltype(c1), decltype(c2)>);

    // Values preserved correctly
    auto bindings = BinderPack{};
    double r1 = evaluate(c1, bindings);
    double r2 = evaluate(c2, bindings);

    expectNear(r1, 3.14159 - 2.71828, 1e-10);
    expectNear(r2, 2.71828 - 3.14159, 1e-10);
    expectNear(r1, -r2, 1e-10);  // Should be negatives of each other
  };

  "canonicalize with many literals and symbols - value tracking"_test = [] {
    // Test that values are preserved through canonicalization
    // Note: canonicalize only does binary commutation, NOT associative restructuring
    // So (a + x) + b and (b + x) + a will NOT become the same type
    auto a = 100.0_c;
    auto b = 1.0_c;

    // Build: ((a + x) + b) = ((100 + x) + 1)
    // With x=0: should be 101
    auto e1 = (a + x) + b;
    auto c1 = canonicalize(e1);
    expectNear(evaluate(c1, BinderPack{x = 0.0}), 101.0, 1e-10);
    expectNear(evaluate(c1, BinderPack{x = 5.0}), 106.0, 1e-10);

    // Build with swapped outer: ((b + x) + a) - same tree structure, just swapped
    // This WILL canonicalize to same type because the outer addition is commuted
    auto e2 = (b + x) + a;
    auto c2 = canonicalize(e2);
    expectNear(evaluate(c2, BinderPack{x = 0.0}), 101.0, 1e-10);
    expectNear(evaluate(c2, BinderPack{x = 5.0}), 106.0, 1e-10);

    // Same tree structure with commuted outer → same canonical type
    // (a+x)+b and (b+x)+a should canonicalize to same type
    // because the outer Add gets its children sorted
  };

  "canonicalize stress test - literals must not swap values"_test = [] {
    // Use subtraction to make order matter for verification
    // If literals get confused, we'll compute wrong results
    auto big = 1000.0_c;
    auto small = 1.0_c;

    // (big + x) - (small + y) = (1000 + x) - (1 + y)
    // With x=0, y=0: should be 999
    auto e1 = (big + x) - (small + y);
    auto c1 = canonicalize(e1);
    expectNear(evaluate(c1, BinderPack{x = 0.0, y = 0.0}), 999.0, 1e-10);

    // (small + x) - (big + y) = (1 + x) - (1000 + y)
    // With x=0, y=0: should be -999
    auto e2 = (small + x) - (big + y);
    auto c2 = canonicalize(e2);
    expectNear(evaluate(c2, BinderPack{x = 0.0, y = 0.0}), -999.0, 1e-10);

    // Different types: inner additions canonicalize x before constant,
    // but (x + 1000) - (y + 1) ≠ (x + 1) - (y + 1000) as types
    static_assert(!std::is_same_v<decltype(c1), decltype(c2)>);

    // Values MUST be different — subtraction is non-commutative
    expectTrue(evaluate(c1, BinderPack{x = 0.0, y = 0.0}) > 0);
    expectTrue(evaluate(c2, BinderPack{x = 0.0, y = 0.0}) < 0);
  };

  "canonicalize deeply nested with multiple literals"_test = [] {
    // Deep nesting: ((a * x) + (b * y)) * ((c + x) + (d + y))
    auto a = 2.0_c;
    auto b = 3.0_c;
    auto c = 5.0_c;
    auto d = 7.0_c;

    auto e = ((a * x) + (b * y)) * ((c + x) + (d + y));
    auto can = canonicalize(e);

    // With x=1, y=1:
    // ((2*1) + (3*1)) * ((5+1) + (7+1)) = (2+3) * (6+8) = 5 * 14 = 70
    expectNear(evaluate(can, BinderPack{x = 1.0, y = 1.0}), 70.0, 1e-10);

    // With x=0, y=0:
    // ((2*0) + (3*0)) * ((5+0) + (7+0)) = 0 * 12 = 0
    expectNear(evaluate(can, BinderPack{x = 0.0, y = 0.0}), 0.0, 1e-10);

    // With x=10, y=20:
    // ((2*10) + (3*20)) * ((5+10) + (7+20)) = (20+60) * (15+27) = 80 * 42 = 3360
    expectNear(evaluate(can, BinderPack{x = 10.0, y = 20.0}), 3360.0, 1e-10);
  };

  "canonicalize two literals of same type - ordering edge case"_test = [] {
    // CRITICAL: Two Constant<...> have the SAME TYPE, so compare() returns Equal.
    // When Equal, we DON'T swap. This means a+b and b+a stay in their original order!
    // This is fine because the TYPE is the same and addition is commutative.
    auto a = 100.0_c;
    auto b = 1.0_c;

    auto e1 = a + b;  // 100 + 1
    auto e2 = b + a;  // 1 + 100

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Same type (both Expression<AddOp, Constant<...>, Constant<...>>)
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);

    // Both evaluate correctly (addition is commutative)
    auto bindings = BinderPack{};
    expectNear(evaluate(c1, bindings), 101.0, 1e-10);
    expectNear(evaluate(c2, bindings), 101.0, 1e-10);
  };

  "canonicalize constant vs symbol ordering"_test = [] {
    // When Constant<100.0> meets Symbol<X>, they have DIFFERENT types
    // The ordering depends on TypeId, but should be consistent
    auto a = 100.0_c;

    // Determine the actual ordering
    constexpr auto const_vs_sym = compare(decltype(a){}, x);

    auto e1 = a + x;  // lit + symbol
    auto e2 = x + a;  // symbol + lit

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Both should canonicalize to the SAME type (smaller first)
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);

    // And evaluate correctly
    expectNear(evaluate(c1, BinderPack{x = 5.0}), 105.0, 1e-10);
    expectNear(evaluate(c2, BinderPack{x = 5.0}), 105.0, 1e-10);
  };

  "canonicalize mixed literals and symbols - multiplication"_test = [] {
    // Test that (a * x) * b and (b * x) * a handle literals correctly
    // These have DIFFERENT tree structures and may produce different types
    auto a = 10.0_c;
    auto b = 2.0_c;

    auto e1 = (a * x) * b;  // (10 * x) * 2 = 20x
    auto e2 = (b * x) * a;  // (2 * x) * 10 = 20x

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);

    // Both should evaluate to 20x
    expectNear(evaluate(c1, BinderPack{x = 3.0}), 60.0, 1e-10);
    expectNear(evaluate(c2, BinderPack{x = 3.0}), 60.0, 1e-10);

    // Type might or might not be the same depending on tree structure
    // What matters is correct evaluation
  };

  "canonicalize complex polynomial with multiple literals"_test = [] {
    // User test case: 3.0_c * x * 2.0_c + 1.0_c + 5.0_c * x
    // Parses as: (((3.0_c * x) * 2.0_c) + 1.0_c) + (5.0_c * x)
    // = (3x * 2) + 1 + 5x = 6x + 1 + 5x = 11x + 1
    auto expr = 3.0_c * x * 2.0_c + 1.0_c + 5.0_c * x;
    auto can = canonicalize(expr);

    // With x = 10: 3*10*2 + 1 + 5*10 = 60 + 1 + 50 = 111
    expectNear(evaluate(can, BinderPack{x = 10.0}), 111.0, 1e-10);

    // With x = 0: 0 + 1 + 0 = 1
    expectNear(evaluate(can, BinderPack{x = 0.0}), 1.0, 1e-10);

    // With x = 1: 3*1*2 + 1 + 5*1 = 6 + 1 + 5 = 12
    expectNear(evaluate(can, BinderPack{x = 1.0}), 12.0, 1e-10);

    // Try a variation with different ordering
    auto expr2 = 5.0_c * x + 1.0_c + 3.0_c * x * 2.0_c;
    auto can2 = canonicalize(expr2);

    // Should give the same numerical results
    expectNear(evaluate(can2, BinderPack{x = 10.0}), 111.0, 1e-10);
    expectNear(evaluate(can2, BinderPack{x = 0.0}), 1.0, 1e-10);
    expectNear(evaluate(can2, BinderPack{x = 1.0}), 12.0, 1e-10);
  };

  "canonicalize different orderings produce same canonical result"_test = [] {
    // The acid test: same expression built in different orders
    // must canonicalize to same type AND same runtime value
    auto a = 11.0_c;
    auto b = 13.0_c;
    auto c = 17.0_c;
    auto d = 19.0_c;

    // All of these should canonicalize to the same form:
    auto e1 = (a * x) + (b * y);      // (11x + 13y)
    auto e2 = (b * y) + (a * x);      // (13y + 11x) - commuted
    auto e3 = (x * a) + (y * b);      // (x*11 + y*13) - inner commuted
    auto e4 = (y * b) + (x * a);      // (y*13 + x*11) - both commuted

    auto c1 = canonicalize(e1);
    auto c2 = canonicalize(e2);
    auto c3 = canonicalize(e3);
    auto c4 = canonicalize(e4);

    // All should have the same type
    static_assert(std::is_same_v<decltype(c1), decltype(c2)>);
    static_assert(std::is_same_v<decltype(c1), decltype(c3)>);
    static_assert(std::is_same_v<decltype(c1), decltype(c4)>);

    // All should evaluate to the same value
    auto bindings = BinderPack{x = 3.0, y = 5.0};
    double expected = 11.0 * 3.0 + 13.0 * 5.0;  // 33 + 65 = 98

    expectNear(evaluate(c1, bindings), expected, 1e-10);
    expectNear(evaluate(c2, bindings), expected, 1e-10);
    expectNear(evaluate(c3, bindings), expected, 1e-10);
    expectNear(evaluate(c4, bindings), expected, 1e-10);
  };

  return TestRegistry::result();
}
