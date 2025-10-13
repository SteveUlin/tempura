#include <cassert>
#include <type_traits>

#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  "Integer division with exact result folds to integer"_test = [] {
    // 6 / 2 = 3 (exact)
    constexpr auto expr = Constant<6>{} / Constant<2>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    // Should fold to Constant<3>
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(result)>, Constant<3>>);
  };

  "Integer division with non-integer result promotes to Fraction"_test = [] {
    // 5 / 2 (non-integer)
    constexpr auto expr = Constant<5>{} / Constant<2>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    // Should become Fraction<5, 2>
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(result)>, Fraction<5, 2>>);
    static_assert(result.numerator == 5);
    static_assert(result.denominator == 2);
  };

  "Division result automatically reduces to lowest terms"_test = [] {
    // 4 / 6 should reduce to 2/3
    constexpr auto expr = Constant<4>{} / Constant<6>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(result)>, Fraction<2, 3>>);
    static_assert(result.numerator == 2);
    static_assert(result.denominator == 3);
  };

  "Division by 1 folds to numerator"_test = [] {
    // 7 / 1 = 7
    constexpr auto expr = Constant<7>{} / Constant<1>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(result)>, Constant<7>>);
  };

  "Negative division handles signs correctly"_test = [] {
    // -5 / 2
    constexpr auto expr = Constant<-5>{} / Constant<2>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(result)>, Fraction<-5, 2>>);
    static_assert(result.numerator == -5);
    static_assert(result.denominator == 2);
  };

  "Division by negative denominator normalizes sign"_test = [] {
    // 5 / -2 should normalize to -5/2
    constexpr auto expr = Constant<5>{} / Constant<-2>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    static_assert(result.numerator == -5);
    static_assert(result.denominator == 2);
  };

  "Fraction literals work correctly"_test = [] {
    // 1_frac / 3_frac creates Fraction<1, 3>
    constexpr auto third = 1_frac / 3_frac;
    static_assert(third.numerator == 1);
    static_assert(third.denominator == 3);

    constexpr auto two_thirds = 2_frac / 3_frac;
    static_assert(two_thirds.numerator == 2);
    static_assert(two_thirds.denominator == 3);
  };

  "Fractions convert to string correctly"_test = [] {
    auto half_str = toStringRuntime(Fraction<1, 2>{});
    assert(half_str == "1/2");

    auto two_thirds_str = toStringRuntime(Fraction<2, 3>{});
    assert(two_thirds_str == "2/3");

    // Integer fractions (denominator = 1) show as integers
    auto five_str = toStringRuntime(Fraction<5, 1>{});
    assert(five_str == "5");
  };

  "Fractions evaluate to doubles correctly"_test = [] {
    constexpr auto half_val = evaluate(Fraction<1, 2>{}, BinderPack{});
    static_assert(half_val == 0.5);

    constexpr auto quarter_val = evaluate(Fraction<1, 4>{}, BinderPack{});
    static_assert(quarter_val == 0.25);

    // Check non-exact conversion
    auto two_thirds_val = evaluate(Fraction<2, 3>{}, BinderPack{});
    assert(two_thirds_val > 0.666 && two_thirds_val < 0.667);
  };

  "Fraction ordering works correctly"_test = [] {
    // Test ordering in simplification contexts
    static_assert(lessThan(Fraction<1, 3>{}, Fraction<1, 2>{}));
    static_assert(lessThan(Fraction<1, 4>{}, Fraction<1, 3>{}));
    static_assert(!lessThan(Fraction<2, 3>{}, Fraction<1, 2>{}));

    // Fractions come before constants in canonical order
    static_assert(lessThan(Fraction<1, 2>{}, Constant<1>{}));
  };

  "Complex expression with fractions simplifies correctly"_test = [] {
    Symbol x;

    // x * (1/2) should remain as x * Fraction<1, 2>
    auto div_expr = Constant<1>{} / Constant<2>{};
    auto frac = promote_division_to_fraction.apply(div_expr, default_context());
    auto expr = x * frac;

    debug_print(expr, "x * (1/2)");
  };

  "Fraction arithmetic combines correctly"_test = [] {
    // (1/2) + (1/3) = 5/6
    constexpr auto sum = Fraction<1, 2>{} + Fraction<1, 3>{};
    static_assert(sum.numerator == 5);
    static_assert(sum.denominator == 6);

    // (1/2) * (2/3) = 1/3
    constexpr auto product = Fraction<1, 2>{} * Fraction<2, 3>{};
    static_assert(product.numerator == 1);
    static_assert(product.denominator == 3);
  };

  "No premature float evaluation"_test = [] {
    // Verify that 1/3 stays exact, not 0.333...
    constexpr auto expr = Constant<1>{} / Constant<3>{};
    constexpr auto result =
        promote_division_to_fraction.apply(expr, default_context());

    // Should be Fraction<1, 3>, not a float constant
    static_assert(
        std::is_same_v<std::remove_cvref_t<decltype(result)>, Fraction<1, 3>>);

    // Verify it converts correctly when needed
    auto numeric = evaluate(result, BinderPack{});
    assert(numeric > 0.333 && numeric < 0.334);
  };

  return 0;
}
