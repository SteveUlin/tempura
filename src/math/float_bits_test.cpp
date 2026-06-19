#include "float_bits.h"

#include <cmath>

#include "unit.h"

using namespace tempura;

// constexpr: the bit surgery evaluates at compile time.
static_assert(ldexpFast(1.0, 3) == 8.0);
static_assert(pow2(-1) == 0.5);
static_assert(exponentOf(8.0) == 3 && exponentOf(1.0) == 0 && exponentOf(0.5) == -1);
static_assert(mantissaOf(1.0) == 0 && mantissaOf(1.5) == (kMantMask + 1) / 2);
static_assert(isNormalExponent(1023) && !isNormalExponent(1024));
static_assert(isSubnormal(5e-310) && !isSubnormal(1.0) && !isSubnormal(0.0));

auto main() -> int {
  "exponent/mantissa extraction"_test = [] {
    expectEq(exponentOf(1024.0), 10);  // 2¹⁰
    expectEq(exponentOf(0.125), -3);
    expectEq(mantissaOf(3.0), kMantMask & rawBits(3.0));  // 3 = 1.1b × 2¹
  };

  "ldexpFast and scalbn agree with std (bit-exact), incl. subnormal/overflow"_test = [] {
    for (double x : {1.0, 1.5, 0.7, 1.0 / 3.0, 2.1e300, 3.3e-300}) {
      for (int n : {-1100, -1074, -1022, -700, -53, -1, 0, 1, 53, 700, 1023, 1100}) {
        expectWithinUlps(tempura::scalbn(x, n), std::scalbn(x, n), 0);  // full-range, subnormal-aware
      }
      for (int k = -1022; k <= 1023; k += 137)  // ldexpFast: normal range only
        expectWithinUlps(ldexpFast(x, k), std::ldexp(x, k), 0);
    }
  };

  "frexp matches std on normals, subnormals, ±0, powers of two"_test = [] {
    for (double x : {1.0, 8.0, 0.5, 0.7, 100.0, -3.5, 5e-310, 2.5e-320}) {
      int e_mine = 0;
      int e_std = 0;
      const double m_mine = tempura::frexp(x, e_mine);
      const double m_std = std::frexp(x, &e_std);
      expectWithinUlps(m_mine, m_std, 0);
      expectEq(e_mine, e_std);
      // round-trips: m·2ᵉ reconstructs x
      expectWithinUlps(tempura::scalbn(m_mine, e_mine), x, 0);
    }
    // ±0: mantissa is ±0, exponent 0
    int e = 99;
    expectWithinUlps(tempura::frexp(0.0, e), 0.0, 0);
    expectEq(e, 0);
    expectWithinUlps(tempura::frexp(-0.0, e), -0.0, 0);
  };

  return TestRegistry::result();
}
