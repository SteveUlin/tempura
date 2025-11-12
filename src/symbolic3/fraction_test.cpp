#include "symbolic3/fraction.h"

#include <cassert>

#include "symbolic3/core.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  "Fraction GCD reduction"_test = [] {
    // Test automatic GCD reduction
    constexpr auto f1 = Fraction<4, 6>{};
    static_assert(f1.numerator == 2, "4/6 should reduce to 2/3");
    static_assert(f1.denominator == 3, "4/6 should reduce to 2/3");

    constexpr auto f2 = Fraction<10, 15>{};
    static_assert(f2.numerator == 2, "10/15 should reduce to 2/3");
    static_assert(f2.denominator == 3, "10/15 should reduce to 2/3");

    constexpr auto f3 = Fraction<7, 1>{};
    static_assert(f3.numerator == 7, "7/1 should stay 7/1");
    static_assert(f3.denominator == 1, "7/1 should stay 7/1");
  };

  "Fraction sign normalization"_test = [] {
    // Sign should always be in numerator
    constexpr auto f1 = Fraction<-3, 4>{};
    static_assert(f1.numerator == -3, "Negative numerator preserved");
    static_assert(f1.denominator == 4, "Denominator positive");

    constexpr auto f2 = Fraction<3, -4>{};
    static_assert(f2.numerator == -3, "Negative moved to numerator");
    static_assert(f2.denominator == 4, "Denominator made positive");

    constexpr auto f3 = Fraction<-3, -4>{};
    static_assert(f3.numerator == 3, "Double negative becomes positive");
    static_assert(f3.denominator == 4, "Denominator positive");
  };

  "Fraction addition"_test = [] {
    // 1/2 + 1/3 = 5/6
    constexpr auto sum = Fraction<1, 2>{} + Fraction<1, 3>{};
    static_assert(sum.numerator == 5, "1/2 + 1/3 = 5/6");
    static_assert(sum.denominator == 6, "1/2 + 1/3 = 5/6");

    // 1/4 + 1/4 = 1/2 (should reduce)
    constexpr auto sum2 = Fraction<1, 4>{} + Fraction<1, 4>{};
    static_assert(sum2.numerator == 1, "1/4 + 1/4 = 1/2");
    static_assert(sum2.denominator == 2, "1/4 + 1/4 = 1/2");
  };

  "Fraction subtraction"_test = [] {
    // 1/2 - 1/3 = 1/6
    constexpr auto diff = Fraction<1, 2>{} - Fraction<1, 3>{};
    static_assert(diff.numerator == 1, "1/2 - 1/3 = 1/6");
    static_assert(diff.denominator == 6, "1/2 - 1/3 = 1/6");

    // 3/4 - 1/4 = 1/2 (should reduce)
    constexpr auto diff2 = Fraction<3, 4>{} - Fraction<1, 4>{};
    static_assert(diff2.numerator == 1, "3/4 - 1/4 = 1/2");
    static_assert(diff2.denominator == 2, "3/4 - 1/4 = 1/2");
  };

  "Fraction multiplication"_test = [] {
    // 1/2 * 1/3 = 1/6
    constexpr auto prod = Fraction<1, 2>{} * Fraction<1, 3>{};
    static_assert(prod.numerator == 1, "1/2 * 1/3 = 1/6");
    static_assert(prod.denominator == 6, "1/2 * 1/3 = 1/6");

    // 2/3 * 3/4 = 1/2 (should reduce)
    constexpr auto prod2 = Fraction<2, 3>{} * Fraction<3, 4>{};
    static_assert(prod2.numerator == 1, "2/3 * 3/4 = 1/2");
    static_assert(prod2.denominator == 2, "2/3 * 3/4 = 1/2");
  };

  "Fraction division"_test = [] {
    // (1/2) / (1/3) = 3/2
    constexpr auto quot = Fraction<1, 2>{} / Fraction<1, 3>{};
    static_assert(quot.numerator == 3, "(1/2) / (1/3) = 3/2");
    static_assert(quot.denominator == 2, "(1/2) / (1/3) = 3/2");

    // (2/3) / (4/3) = 1/2 (should reduce)
    constexpr auto quot2 = Fraction<2, 3>{} / Fraction<4, 3>{};
    static_assert(quot2.numerator == 1, "(2/3) / (4/3) = 1/2");
    static_assert(quot2.denominator == 2, "(2/3) / (4/3) = 1/2");
  };

  "Fraction negation"_test = [] {
    constexpr auto f = Fraction<3, 4>{};
    constexpr auto neg = -f;
    static_assert(neg.numerator == -3, "-(3/4) = -3/4");
    static_assert(neg.denominator == 4, "-(3/4) = -3/4");
  };

  "Fraction with Constants"_test = [] {
    // Fraction + Constant
    constexpr auto sum = Fraction<1, 2>{} + Constant<1>{};
    static_assert(sum.numerator == 3, "1/2 + 1 = 3/2");
    static_assert(sum.denominator == 2, "1/2 + 1 = 3/2");

    // Constant + Fraction
    constexpr auto sum2 = Constant<2>{} + Fraction<1, 3>{};
    static_assert(sum2.numerator == 7, "2 + 1/3 = 7/3");
    static_assert(sum2.denominator == 3, "2 + 1/3 = 7/3");

    // Fraction * Constant
    constexpr auto prod = Fraction<2, 3>{} * Constant<3>{};
    static_assert(prod.numerator == 2, "2/3 * 3 = 2");
    static_assert(prod.denominator == 1, "2/3 * 3 = 2");
  };

  "Fraction literal suffix"_test = [] {
    // Test _frac literal
    constexpr auto one = 1_frac;
    static_assert(one.numerator == 1, "1_frac = 1/1");
    static_assert(one.denominator == 1, "1_frac = 1/1");

    constexpr auto five = 5_frac;
    static_assert(five.numerator == 5, "5_frac = 5/1");
    static_assert(five.denominator == 1, "5_frac = 5/1");

    // Build fraction from literals
    constexpr auto half = 1_frac / 2_frac;
    static_assert(half.numerator == 1, "1_frac / 2_frac = 1/2");
    static_assert(half.denominator == 2, "1_frac / 2_frac = 1/2");

    constexpr auto two_thirds = 2_frac / 3_frac;
    static_assert(two_thirds.numerator == 2, "2_frac / 3_frac = 2/3");
    static_assert(two_thirds.denominator == 3, "2_frac / 3_frac = 2/3");
  };

  "Common fraction constants"_test = [] {
    static_assert(half.numerator == 1, "half = 1/2");
    static_assert(half.denominator == 2, "half = 1/2");

    static_assert(third.numerator == 1, "third = 1/3");
    static_assert(third.denominator == 3, "third = 1/3");

    static_assert(quarter.numerator == 1, "quarter = 1/4");
    static_assert(quarter.denominator == 4, "quarter = 1/4");

    static_assert(two_thirds.numerator == 2, "two_thirds = 2/3");
    static_assert(two_thirds.denominator == 3, "two_thirds = 2/3");
  };

  "Fraction equality"_test = [] {
    static_assert(Fraction<1, 2>{} == Fraction<2, 4>{},
                  "1/2 == 2/4 (after reduction)");
    static_assert(Fraction<3, 6>{} == Fraction<1, 2>{},
                  "3/6 == 1/2 (after reduction)");
    static_assert(!(Fraction<1, 2>{} == Fraction<1, 3>{}), "1/2 != 1/3");
  };

  "Fraction comparison"_test = [] {
    static_assert(Fraction<1, 3>{} < Fraction<1, 2>{}, "1/3 < 1/2");
    static_assert(Fraction<2, 3>{} > Fraction<1, 2>{}, "2/3 > 1/2");
    static_assert(Fraction<1, 2>{} <= Fraction<2, 4>{}, "1/2 <= 2/4");
    static_assert(Fraction<1, 2>{} >= Fraction<2, 4>{}, "1/2 >= 2/4");
  };

  "Fraction to_double conversion"_test = [] {
    constexpr auto half_val = Fraction<1, 2>::to_double();
    static_assert(half_val == 0.5, "1/2 converts to 0.5");

    constexpr auto quarter_val = Fraction<1, 4>::to_double();
    static_assert(quarter_val == 0.25, "1/4 converts to 0.25");

    constexpr auto two_thirds_val = Fraction<2, 3>::to_double();
    // Check approximation (floating point)
    assert(two_thirds_val > 0.666 && two_thirds_val < 0.667);
  };

  return 0;
}
