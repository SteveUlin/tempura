#include "math/ratio.h"

#include "unit.h"

using namespace tempura;

// Compile-time tests
static_assert(Ratio{1, 2}.num == 1);
static_assert(Ratio{1, 2}.den == 2);
static_assert(Ratio{2, 4}.num == 1);  // Auto-reduces
static_assert(Ratio{2, 4}.den == 2);
static_assert(Ratio{-1, 2}.num == -1);
static_assert(Ratio{1, -2}.num == -1);  // Sign normalization
static_assert(Ratio{1, -2}.den == 2);
static_assert(Ratio{-1, -2}.num == 1);  // Double negative
static_assert(Ratio{6, 9} == Ratio{2, 3});
static_assert(Ratio{1, 2} + Ratio{1, 3} == Ratio{5, 6});
static_assert(Ratio{1, 2} - Ratio{1, 3} == Ratio{1, 6});
static_assert(Ratio{2, 3} * Ratio{3, 4} == Ratio{1, 2});
static_assert(Ratio{1, 2} / Ratio{1, 4} == Ratio{2, 1});
static_assert(Ratio{1, 2} < Ratio{2, 3});
static_assert(Ratio{3, 4} > Ratio{1, 2});

auto main() -> int {
  // ===========================================================================
  // Construction and Reduction
  // ===========================================================================

  "ratio construction"_test = [] {
    constexpr Ratio r1{3, 4};
    static_assert(r1.num == 3);
    static_assert(r1.den == 4);

    constexpr Ratio r2{6, 8};  // Should reduce to 3/4
    static_assert(r2.num == 3);
    static_assert(r2.den == 4);

    constexpr Ratio r3{0, 5};  // Zero
    static_assert(r3.num == 0);
    static_assert(r3.den == 1);
  };

  "ratio sign normalization"_test = [] {
    constexpr Ratio r1{-1, 2};
    static_assert(r1.num == -1);
    static_assert(r1.den == 2);

    constexpr Ratio r2{1, -2};  // Negative moves to numerator
    static_assert(r2.num == -1);
    static_assert(r2.den == 2);

    constexpr Ratio r3{-1, -2};  // Double negative = positive
    static_assert(r3.num == 1);
    static_assert(r3.den == 2);
  };

  "ratio from integer"_test = [] {
    constexpr Ratio r{5};
    static_assert(r.num == 5);
    static_assert(r.den == 1);
  };

  // ===========================================================================
  // Arithmetic
  // ===========================================================================

  "ratio addition"_test = [] {
    static_assert(Ratio{1, 2} + Ratio{1, 2} == Ratio{1, 1});
    static_assert(Ratio{1, 2} + Ratio{1, 3} == Ratio{5, 6});
    static_assert(Ratio{1, 4} + Ratio{1, 4} == Ratio{1, 2});
    static_assert(Ratio{-1, 2} + Ratio{1, 2} == Ratio{0, 1});
  };

  "ratio subtraction"_test = [] {
    static_assert(Ratio{1, 2} - Ratio{1, 3} == Ratio{1, 6});
    static_assert(Ratio{3, 4} - Ratio{1, 4} == Ratio{1, 2});
    static_assert(Ratio{1, 2} - Ratio{1, 2} == Ratio{0, 1});
    static_assert(Ratio{1, 3} - Ratio{1, 2} == Ratio{-1, 6});
  };

  "ratio multiplication"_test = [] {
    static_assert(Ratio{1, 2} * Ratio{1, 2} == Ratio{1, 4});
    static_assert(Ratio{2, 3} * Ratio{3, 4} == Ratio{1, 2});
    static_assert(Ratio{-1, 2} * Ratio{2, 3} == Ratio{-1, 3});
    static_assert(Ratio{5, 1} * Ratio{1, 5} == Ratio{1, 1});
  };

  "ratio division"_test = [] {
    static_assert(Ratio{1, 2} / Ratio{1, 4} == Ratio{2, 1});
    static_assert(Ratio{3, 4} / Ratio{3, 2} == Ratio{1, 2});
    static_assert(Ratio{1, 1} / Ratio{2, 1} == Ratio{1, 2});
  };

  "ratio negation"_test = [] {
    static_assert(-Ratio{1, 2} == Ratio{-1, 2});
    static_assert(-Ratio{-3, 4} == Ratio{3, 4});
  };

  // ===========================================================================
  // Mixed arithmetic with integers
  // ===========================================================================

  "ratio plus integer"_test = [] {
    static_assert(Ratio{1, 2} + 1 == Ratio{3, 2});
    static_assert(2 + Ratio{1, 3} == Ratio{7, 3});
  };

  "ratio minus integer"_test = [] {
    static_assert(Ratio{5, 2} - 2 == Ratio{1, 2});
    static_assert(3 - Ratio{1, 2} == Ratio{5, 2});
  };

  "ratio times integer"_test = [] {
    static_assert(Ratio{1, 2} * 3 == Ratio{3, 2});
    static_assert(4 * Ratio{1, 3} == Ratio{4, 3});
  };

  "ratio divided by integer"_test = [] {
    static_assert(Ratio{3, 4} / 2 == Ratio{3, 8});
    static_assert(3 / Ratio{2, 1} == Ratio{3, 2});
  };

  // ===========================================================================
  // Comparisons
  // ===========================================================================

  "ratio equality"_test = [] {
    static_assert(Ratio{1, 2} == Ratio{1, 2});
    static_assert(Ratio{2, 4} == Ratio{1, 2});
    static_assert(Ratio{1, 2} != Ratio{1, 3});
  };

  "ratio ordering"_test = [] {
    static_assert(Ratio{1, 3} < Ratio{1, 2});
    static_assert(Ratio{2, 3} > Ratio{1, 2});
    static_assert(Ratio{1, 2} <= Ratio{1, 2});
    static_assert(Ratio{1, 2} >= Ratio{1, 2});
    static_assert(Ratio{-1, 2} < Ratio{1, 2});
  };

  // ===========================================================================
  // Utility methods
  // ===========================================================================

  "ratio isZero"_test = [] {
    static_assert(Ratio{0, 1}.isZero());
    static_assert(Ratio{0, 5}.isZero());
    static_assert(!Ratio{1, 2}.isZero());
  };

  "ratio isInteger"_test = [] {
    static_assert(Ratio{5, 1}.isInteger());
    static_assert(Ratio{4, 2}.isInteger());  // 4/2 = 2/1
    static_assert(!Ratio{1, 2}.isInteger());
  };

  "ratio abs"_test = [] {
    static_assert(Ratio{-1, 2}.abs() == Ratio{1, 2});
    static_assert(Ratio{3, 4}.abs() == Ratio{3, 4});
  };

  "ratio reciprocal"_test = [] {
    static_assert(Ratio{2, 3}.reciprocal() == Ratio{3, 2});
    static_assert(Ratio{-1, 4}.reciprocal() == Ratio{-4, 1});
  };

  "ratio to double"_test = [] {
    constexpr double d = static_cast<double>(Ratio{1, 2});
    expectNear(d, 0.5, 1e-10);

    constexpr double d2 = static_cast<double>(Ratio{1, 3});
    expectNear(d2, 0.333333333, 1e-6);
  };

  return TestRegistry::result();
}
