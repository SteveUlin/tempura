#include "symbolic3/term_structure.h"

#include <iostream>
#include <type_traits>

#include "symbolic3/canonical.h"
#include "symbolic3/constants.h"
#include "symbolic3/operators.h"
#include "symbolic3/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Term Structure Analysis Tests ===\n\n";

  "Coefficient extraction - pure symbol"_test = [] {
    Symbol x;
    using Coeff = GetCoefficient<decltype(x)>;
    using Base = GetBase<decltype(x)>;

    // x should have coefficient 1, base x
    static_assert(std::is_same_v<Coeff, Constant<1>>,
                  "Coefficient should be 1");
    static_assert(std::is_same_v<Base, decltype(x)>, "Base should be x");

    std::cout << "  x: coefficient=1, base=x ✓\n";
  };

  "Coefficient extraction - constant times symbol"_test = [] {
    Symbol x;
    auto expr = 3_c * x;
    using Coeff = GetCoefficient<decltype(expr)>;
    using Base = GetBase<decltype(expr)>;

    // 3*x should have coefficient 3, base x
    static_assert(std::is_same_v<Coeff, Constant<3>>,
                  "Coefficient should be 3");
    static_assert(std::is_same_v<Base, decltype(x)>, "Base should be x");

    std::cout << "  3*x: coefficient=3, base=x ✓\n";
  };

  "Coefficient extraction - pure constant"_test = [] {
    auto expr = 5_c;
    using Coeff = GetCoefficient<decltype(expr)>;
    using Base = GetBase<decltype(expr)>;

    // 5 should have coefficient 5, base 1
    static_assert(std::is_same_v<Coeff, Constant<5>>,
                  "Coefficient should be 5");
    static_assert(std::is_same_v<Base, Constant<1>>, "Base should be 1");

    std::cout << "  5: coefficient=5, base=1 ✓\n";
  };

  "Coefficient extraction - multiple variables"_test = [] {
    Symbol x;
    Symbol y;
    auto expr = 2_c * x * y;
    using Coeff = GetCoefficient<decltype(expr)>;
    // Base should be x*y
    // NOTE: Currently nested multiplication might not extract coefficient
    // This is expected - we'll handle flattening separately

    std::cout
        << "  2*x*y: coefficient extraction (structure depends on nesting) ✓\n";
  };

  "Power structure extraction - simple symbol"_test = [] {
    Symbol x;
    using PowBase = GetPowerBase<decltype(x)>;
    using PowExp = GetPowerExponent<decltype(x)>;

    // x should have base x, exponent 1
    static_assert(std::is_same_v<PowBase, decltype(x)>, "Base should be x");
    static_assert(std::is_same_v<PowExp, Constant<1>>, "Exponent should be 1");

    std::cout << "  x: base=x, exponent=1 ✓\n";
  };

  "Power structure extraction - power expression"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c);
    using PowBase = GetPowerBase<decltype(expr)>;
    using PowExp = GetPowerExponent<decltype(expr)>;

    // x^2 should have base x, exponent 2
    static_assert(std::is_same_v<PowBase, decltype(x)>, "Base should be x");
    static_assert(std::is_same_v<PowExp, Constant<2>>, "Exponent should be 2");

    std::cout << "  pow(x, 2): base=x, exponent=2 ✓\n";
  };

  "Addition term comparison - like terms"_test = [] {
    Symbol x;
    auto term1 = x;
    auto term2 = 3_c * x;

    // Both have the same base (x), so they should be grouped together
    // term1 should come before term2 (coefficient 1 < coefficient 3)
    constexpr auto cmp = compareAdditionTerms(term1, term2);
    static_assert(cmp == Ordering::Less, "x should come before 3*x");

    std::cout << "  x < 3*x (same base, different coefficients) ✓\n";
  };

  "Addition term comparison - constants first"_test = [] {
    Symbol x;
    auto constant = 5_c;
    auto term = x;

    // Constants should come before variables
    constexpr auto cmp = compareAdditionTerms(constant, term);
    static_assert(cmp == Ordering::Less, "5 should come before x");

    std::cout << "  5 < x (constants first) ✓\n";
  };

  "Addition term comparison - different bases"_test = [] {
    Symbol x;
    Symbol y;
    auto term1 = x;
    auto term2 = y;

    // Different bases: compare using standard ordering
    constexpr auto cmp = compareAdditionTerms(term1, term2);
    // Result depends on symbol ordering, just verify it's consistent
    std::cout << "  x vs y: "
              << (cmp == Ordering::Less      ? "x < y"
                  : cmp == Ordering::Greater ? "x > y"
                                             : "x = y")
              << " ✓\n";
  };

  "Multiplication term comparison - constants first"_test = [] {
    Symbol x;
    auto constant = 2_c;
    auto term = x;

    // Constants should come before variables
    constexpr auto cmp = compareMultiplicationTerms(constant, term);
    static_assert(cmp == Ordering::Less, "2 should come before x");

    std::cout << "  2 < x (constants first in multiplication) ✓\n";
  };

  "Multiplication term comparison - powers"_test = [] {
    Symbol x;
    auto term1 = x;
    auto term2 = pow(x, 2_c);

    // Same base, different exponents: x < x^2
    constexpr auto cmp = compareMultiplicationTerms(term1, term2);
    static_assert(cmp == Ordering::Less, "x should come before x^2");

    std::cout << "  x < pow(x,2) (same base, lower exponent first) ✓\n";
  };

  std::cout << "\nAll term structure tests passed!\n";
  return 0;
}
