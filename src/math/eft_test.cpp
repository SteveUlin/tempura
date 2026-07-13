#include "eft.h"

#include <utility>

#include "unit.h"

using namespace tempura;

// constexpr: 1 + 1e-20 loses the tail in a plain double, but twoSum keeps it exactly.
static_assert(toDouble(twoSum(1.0, 1e-20)) == 1.0);
static_assert(twoSum(1.0, 1e-20).lo == 1e-20);

auto main() -> int {
  "twoSum / twoProduct are exact"_test = [] {
    // hi + lo reproduces a+b (resp. a·b) with no lost bits — checked against long double.
    for (auto [a, b] : {std::pair{1.0, 1e-20}, std::pair{1.0 + 0x1p-52, 1.0 - 0x1p-52},
                        std::pair{3.7, -2.9}}) {
      const DoubleWord s = twoSum(a, b);
      expectEq(static_cast<long double>(s.hi) + s.lo,
               static_cast<long double>(a) + static_cast<long double>(b));
      const DoubleWord p = twoProduct(a, b);
      expectEq(static_cast<long double>(p.hi) + p.lo,
               static_cast<long double>(a) * static_cast<long double>(b));
    }
  };

  "fastTwoSum matches twoSum when |a| ≥ |b|"_test = [] {
    for (auto [a, b] : {std::pair{2.0, 1e-18}, std::pair{-5.0, 0.3}, std::pair{1e100, 1.0}}) {
      const DoubleWord f = fastTwoSum(a, b);
      const DoubleWord s = twoSum(a, b);
      expectEq(f.hi, s.hi);
      expectEq(f.lo, s.lo);
    }
  };

  "double-word ops carry precision a lone double cannot"_test = [] {
    // (1 + 2⁻⁶⁰)·(1 + 2⁻⁶⁰) = 1 + 2⁻⁵⁹ + 2⁻¹²⁰. The 2⁻⁵⁹ term is below ulp(1), so a plain
    // double rounds the product back to 1.0; the double-word keeps it.
    const double eps = 0x1p-60;
    const DoubleWord x = twoSum(1.0, eps);
    const DoubleWord sq = x * 1.0 + eps;  // 1 + 2·2⁻⁶⁰ = 1 + 2⁻⁵⁹, exactly
    expectEq(1.0 + eps + eps, 1.0);       // the lone-double result collapses…
    expectEq(sq.hi, 1.0);
    expectEq(sq.lo, 0x1p-59);             // …the double-word does not
  };

  "toDouble is the only way out"_test = [] {
    // No implicit operator double: extraction is an explicit call, and rounds hi+lo to nearest.
    const DoubleWord x = twoSum(1.0, 0x1p-60);
    expectEq(toDouble(x), 1.0);
  };

  return TestRegistry::result();
}
