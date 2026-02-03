#include "symbolic4/constants.h"

#include "symbolic4/fraction_ops.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/operators.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

inline constexpr Symbol<struct X> x{};

auto main() -> int {
  // ============================================================================
  // _c suffix tests (double constants)
  // ============================================================================

  "_c suffix creates double constants"_test = [] {
    static_assert(std::is_same_v<decltype(42_c), Constant<42.0>>);
    static_assert(std::is_same_v<decltype(0_c), Constant<0.0>>);
    static_assert(std::is_same_v<decltype(7_c), Constant<7.0>>);
    static_assert(std::is_same_v<decltype(3.14_c), Constant<3.14>>);
    static_assert(std::is_same_v<decltype(0.5_c), Constant<0.5>>);
  };

  "negation creates expression"_test = [] {
    constexpr auto neg = -4_c;
    static_assert(!is_constant_v<decltype(neg)>);
    static_assert(is_expression_v<decltype(neg)>);
    expectEq(evaluate(neg, BinderPack{}), -4.0);
  };

  "_c in symbolic expressions"_test = [] {
    auto expr = 2_c * x + 1_c;
    expectNear(evaluate(expr, BinderPack{x = 5.0}), 11.0, 1e-10);
  };

  // ============================================================================
  // _z suffix tests (exact integer → Fraction)
  // ============================================================================

  "_z suffix creates Fraction<N, 1>"_test = [] {
    static_assert(std::is_same_v<decltype(5_z), Fraction<5, 1>>);
    static_assert(std::is_same_v<decltype(0_z), Fraction<0, 1>>);
    static_assert(std::is_same_v<decltype(42_z), Fraction<42, 1>>);
    static_assert(std::is_same_v<decltype(100_z), Fraction<100, 1>>);
  };

  "_z preserves exactness through Fraction arithmetic"_test = [] {
    // 5_z + 3_z = Fraction<5,1> + Fraction<3,1> = Fraction<8,1>
    constexpr auto sum = 5_z + 3_z;
    static_assert(is_fraction_v<decltype(sum)>);
    static_assert(decltype(sum)::numerator == 8);
    static_assert(decltype(sum)::denominator == 1);
  };

  "_z in symbolic expressions"_test = [] {
    auto expr = 2_z * x + 1_z;
    static_assert(is_expression_v<decltype(expr)>);
    expectNear(evaluate(expr, BinderPack{x = 5.0}), 11.0, 1e-10);
  };

  // ============================================================================
  // Fraction arithmetic tests
  // ============================================================================

  "Fraction + Fraction"_test = [] {
    // 1/2 + 1/3 = 5/6
    constexpr auto result = Fraction<1, 2>{} + Fraction<1, 3>{};
    static_assert(decltype(result)::numerator == 5);
    static_assert(decltype(result)::denominator == 6);

    // 1/4 + 1/4 = 1/2 (auto-reduced)
    constexpr auto result2 = Fraction<1, 4>{} + Fraction<1, 4>{};
    static_assert(decltype(result2)::numerator == 1);
    static_assert(decltype(result2)::denominator == 2);
  };

  "Fraction - Fraction"_test = [] {
    // 1/2 - 1/3 = 1/6
    constexpr auto result = Fraction<1, 2>{} - Fraction<1, 3>{};
    static_assert(decltype(result)::numerator == 1);
    static_assert(decltype(result)::denominator == 6);

    // 3/4 - 1/4 = 1/2
    constexpr auto result2 = Fraction<3, 4>{} - Fraction<1, 4>{};
    static_assert(decltype(result2)::numerator == 1);
    static_assert(decltype(result2)::denominator == 2);
  };

  "Fraction * Fraction"_test = [] {
    // 2/3 * 3/4 = 1/2 (auto-reduced from 6/12)
    constexpr auto result = Fraction<2, 3>{} * Fraction<3, 4>{};
    static_assert(decltype(result)::numerator == 1);
    static_assert(decltype(result)::denominator == 2);

    // 1/2 * 1/2 = 1/4
    constexpr auto result2 = Fraction<1, 2>{} * Fraction<1, 2>{};
    static_assert(decltype(result2)::numerator == 1);
    static_assert(decltype(result2)::denominator == 4);
  };

  "Fraction / Fraction"_test = [] {
    // (1/2) / (2/3) = 3/4
    constexpr auto result = Fraction<1, 2>{} / Fraction<2, 3>{};
    static_assert(decltype(result)::numerator == 3);
    static_assert(decltype(result)::denominator == 4);

    // (1/2) / (1/2) = 1
    constexpr auto result2 = Fraction<1, 2>{} / Fraction<1, 2>{};
    static_assert(decltype(result2)::numerator == 1);
    static_assert(decltype(result2)::denominator == 1);
  };

  "Fraction negation"_test = [] {
    constexpr auto neg = -Fraction<3, 4>{};
    static_assert(decltype(neg)::numerator == -3);
    static_assert(decltype(neg)::denominator == 4);
  };

  // ============================================================================
  // Integer Constant / Integer Constant → Fraction (KEY FEATURE!)
  // ============================================================================

  "integer Constant division produces Fraction"_test = [] {
    // This is the key fix: 2/3 stays exact as Fraction<2,3>
    constexpr auto result = Constant<2>{} / Constant<3>{};
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 2);
    static_assert(decltype(result)::denominator == 3);

    // 6/4 = 3/2 (auto-reduced)
    constexpr auto result2 = Constant<6>{} / Constant<4>{};
    static_assert(decltype(result2)::numerator == 3);
    static_assert(decltype(result2)::denominator == 2);
  };

  // ============================================================================
  // Mixed Integer Constant and Fraction operations
  // ============================================================================

  "Integer Constant + Fraction"_test = [] {
    // 2 + 1/3 = 7/3
    constexpr auto result = Constant<2>{} + Fraction<1, 3>{};
    static_assert(is_fraction_v<decltype(result)>);
    static_assert(decltype(result)::numerator == 7);
    static_assert(decltype(result)::denominator == 3);

    // 1/2 + 1 = 3/2
    constexpr auto result2 = Fraction<1, 2>{} + Constant<1>{};
    static_assert(decltype(result2)::numerator == 3);
    static_assert(decltype(result2)::denominator == 2);
  };

  "Integer Constant * Fraction"_test = [] {
    // 2 * (1/3) = 2/3
    constexpr auto result = Constant<2>{} * Fraction<1, 3>{};
    static_assert(decltype(result)::numerator == 2);
    static_assert(decltype(result)::denominator == 3);

    // (3/4) * 2 = 3/2
    constexpr auto result2 = Fraction<3, 4>{} * Constant<2>{};
    static_assert(decltype(result2)::numerator == 3);
    static_assert(decltype(result2)::denominator == 2);
  };

  "Integer Constant / Fraction and Fraction / Integer Constant"_test = [] {
    // 2 / (1/3) = 6
    constexpr auto result = Constant<2>{} / Fraction<1, 3>{};
    static_assert(decltype(result)::numerator == 6);
    static_assert(decltype(result)::denominator == 1);

    // (3/4) / 2 = 3/8
    constexpr auto result2 = Fraction<3, 4>{} / Constant<2>{};
    static_assert(decltype(result2)::numerator == 3);
    static_assert(decltype(result2)::denominator == 8);
  };

  // ============================================================================
  // Type traits tests
  // ============================================================================

  "is_integer_v for Fractions"_test = [] {
    static_assert(is_integer_v<Fraction<5, 1>>);
    static_assert(is_integer_v<Fraction<0, 1>>);
    static_assert(!is_integer_v<Fraction<1, 2>>);
    static_assert(!is_integer_v<Fraction<3, 4>>);
  };

  "is_zero_fraction_v"_test = [] {
    static_assert(is_zero_fraction_v<Fraction<0, 1>>);
    static_assert(is_zero_fraction_v<Fraction<0, 5>>);  // 0/5 reduces to 0/1
    static_assert(!is_zero_fraction_v<Fraction<1, 1>>);
    static_assert(!is_zero_fraction_v<Fraction<1, 2>>);
  };

  "is_one_fraction_v"_test = [] {
    static_assert(is_one_fraction_v<Fraction<1, 1>>);
    static_assert(is_one_fraction_v<Fraction<3, 3>>);  // 3/3 reduces to 1/1
    static_assert(!is_one_fraction_v<Fraction<0, 1>>);
    static_assert(!is_one_fraction_v<Fraction<2, 1>>);
    static_assert(!is_one_fraction_v<Fraction<1, 2>>);
  };

  // ============================================================================
  // Evaluation of Fractions
  // ============================================================================

  "Fraction evaluates to correct double"_test = [] {
    expectNear(evaluate(Fraction<1, 2>{}, BinderPack{}), 0.5, 1e-10);
    expectNear(evaluate(Fraction<1, 3>{}, BinderPack{}), 1.0 / 3.0, 1e-10);
    expectNear(evaluate(Fraction<3, 4>{}, BinderPack{}), 0.75, 1e-10);
  };

  "Fraction in symbolic expression evaluates correctly"_test = [] {
    // (1/2) * x
    auto expr = Fraction<1, 2>{} * x;
    expectNear(evaluate(expr, BinderPack{x = 10.0}), 5.0, 1e-10);

    // x + 1/4
    auto expr2 = x + Fraction<1, 4>{};
    expectNear(evaluate(expr2, BinderPack{x = 0.75}), 1.0, 1e-10);
  };

  // ============================================================================
  // _f suffix tests (string literal fractions)
  // ============================================================================

  "_f suffix creates Fraction from string"_test = [] {
    static_assert(std::is_same_v<decltype("1/2"_f), Fraction<1, 2>>);
    static_assert(std::is_same_v<decltype("3/4"_f), Fraction<3, 4>>);
    static_assert(std::is_same_v<decltype("1/3"_f), Fraction<1, 3>>);
    static_assert(std::is_same_v<decltype("5/6"_f), Fraction<5, 6>>);
  };

  "_f suffix handles negative numerator"_test = [] {
    static_assert(std::is_same_v<decltype("-1/2"_f), Fraction<-1, 2>>);
    static_assert(std::is_same_v<decltype("-3/4"_f), Fraction<-3, 4>>);
    static_assert(decltype("-5/6"_f)::numerator == -5);
    static_assert(decltype("-5/6"_f)::denominator == 6);
  };

  "_f suffix handles integer (no denominator)"_test = [] {
    static_assert(std::is_same_v<decltype("5"_f), Fraction<5, 1>>);
    static_assert(std::is_same_v<decltype("42"_f), Fraction<42, 1>>);
    static_assert(std::is_same_v<decltype("-7"_f), Fraction<-7, 1>>);
  };

  "_f suffix auto-reduces"_test = [] {
    // 2/4 reduces to 1/2
    static_assert(decltype("2/4"_f)::numerator == 1);
    static_assert(decltype("2/4"_f)::denominator == 2);

    // 6/9 reduces to 2/3
    static_assert(decltype("6/9"_f)::numerator == 2);
    static_assert(decltype("6/9"_f)::denominator == 3);

    // -4/8 reduces to -1/2
    static_assert(decltype("-4/8"_f)::numerator == -1);
    static_assert(decltype("-4/8"_f)::denominator == 2);
  };

  "_f suffix in arithmetic"_test = [] {
    // 1/2 + 1/3 = 5/6
    constexpr auto sum = "1/2"_f + "1/3"_f;
    static_assert(decltype(sum)::numerator == 5);
    static_assert(decltype(sum)::denominator == 6);

    // 1/2 * 2/3 = 1/3
    constexpr auto prod = "1/2"_f * "2/3"_f;
    static_assert(decltype(prod)::numerator == 1);
    static_assert(decltype(prod)::denominator == 3);
  };

  "_f suffix in symbolic expressions"_test = [] {
    auto expr = "1/2"_f * x + "1/4"_f;
    expectNear(evaluate(expr, BinderPack{x = 2.0}), 1.25, 1e-10);
  };

  return TestRegistry::result();
}
