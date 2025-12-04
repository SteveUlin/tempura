#include "modular/mod_int.h"

#include "unit.h"

using namespace tempura;

// Compile-time tests
static_assert(ModInt<unsigned long long, 7>::kMod == 7);
static_assert(ModInt<unsigned long long, 7>(10).value == 3);  // 10 mod 7 = 3
static_assert(ModInt<unsigned long long, 7>::raw(3).value == 3);

// Arithmetic
static_assert(
    (ModInt<unsigned long long, 7>(3) + ModInt<unsigned long long, 7>(5))
        .value == 1);  // 8 mod 7
static_assert(
    (ModInt<unsigned long long, 7>(3) - ModInt<unsigned long long, 7>(5))
        .value == 5);  // -2 mod 7
static_assert(
    (ModInt<unsigned long long, 7>(3) * ModInt<unsigned long long, 7>(4))
        .value == 5);                                            // 12 mod 7
static_assert((-ModInt<unsigned long long, 7>(3)).value == 4);  // -3 mod 7

// Exponentiation
static_assert(ModInt<unsigned long long, 7>(3).pow(0).value == 1);
static_assert(ModInt<unsigned long long, 7>(3).pow(1).value == 3);
static_assert(ModInt<unsigned long long, 7>(3).pow(2).value == 2);  // 9 mod 7
static_assert(
    ModInt<unsigned long long, 7>(3).pow(6).value == 1);  // Fermat: 3^6=1 mod 7

// Inverse (7 is prime, so Fermat works)
static_assert(
    ModInt<unsigned long long, 7>(3).inv().value == 5);  // 3*5 = 15 = 1 mod 7
static_assert((ModInt<unsigned long long, 7>(3) *
               ModInt<unsigned long long, 7>(3).inv())
                  .value == 1);

auto main() -> int {
  // ===========================================================================
  // Basic Construction and Properties
  // ===========================================================================

  "ModInt construction from value"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(Mint(0).value == 0);
    static_assert(Mint(3).value == 3);
    static_assert(Mint(7).value == 0);   // 7 mod 7 = 0
    static_assert(Mint(10).value == 3);  // 10 mod 7 = 3
    static_assert(Mint(100).value == 2); // 100 mod 7 = 2
  };

  "ModInt raw construction"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(Mint::raw(0).value == 0);
    static_assert(Mint::raw(3).value == 3);
    static_assert(Mint::raw(6).value == 6);
    // Note: raw(7) would be invalid (v >= Mod), but not checked
  };

  "ModInt type aliases work"_test = [] {
    static_assert(Mod998244353::kMod == 998244353);
    static_assert(Mod1000000007::kMod == 1000000007);

    constexpr Mod998244353 a(123456789);
    static_assert(a.value == 123456789);
  };

  // ===========================================================================
  // Comparison
  // ===========================================================================

  "ModInt equality"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(Mint(3) == Mint(3));
    static_assert(Mint(10) == Mint(3));  // Both reduce to 3
    static_assert(!(Mint(3) == Mint(4)));
  };

  // ===========================================================================
  // Addition
  // ===========================================================================

  "ModInt addition basic"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((Mint(2) + Mint(3)).value == 5);
    static_assert((Mint(5) + Mint(5)).value == 3);  // 10 mod 7 = 3
    static_assert((Mint(0) + Mint(6)).value == 6);
  };

  "ModInt addition wrap-around"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((Mint(6) + Mint(1)).value == 0);  // 7 mod 7 = 0
    static_assert((Mint(6) + Mint(6)).value == 5);  // 12 mod 7 = 5
  };

  "ModInt += operator"_test = [] {
    constexpr auto result = [] {
      ModInt<unsigned long long, 7> a(3);
      a += ModInt<unsigned long long, 7>(5);
      return a;
    }();
    static_assert(result.value == 1);  // 8 mod 7 = 1
  };

  // ===========================================================================
  // Subtraction
  // ===========================================================================

  "ModInt subtraction basic"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((Mint(5) - Mint(3)).value == 2);
    static_assert((Mint(6) - Mint(6)).value == 0);
  };

  "ModInt subtraction wrap-around"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((Mint(3) - Mint(5)).value == 5);  // -2 mod 7 = 5
    static_assert((Mint(0) - Mint(1)).value == 6);  // -1 mod 7 = 6
  };

  "ModInt unary minus"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((-Mint(0)).value == 0);
    static_assert((-Mint(1)).value == 6);  // -1 mod 7 = 6
    static_assert((-Mint(3)).value == 4);  // -3 mod 7 = 4
  };

  "ModInt -= operator"_test = [] {
    constexpr auto result = [] {
      ModInt<unsigned long long, 7> a(3);
      a -= ModInt<unsigned long long, 7>(5);
      return a;
    }();
    static_assert(result.value == 5);  // -2 mod 7 = 5
  };

  // ===========================================================================
  // Multiplication
  // ===========================================================================

  "ModInt multiplication basic"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((Mint(2) * Mint(3)).value == 6);
    static_assert((Mint(3) * Mint(4)).value == 5);  // 12 mod 7 = 5
    static_assert((Mint(0) * Mint(5)).value == 0);
    static_assert((Mint(6) * Mint(6)).value == 1);  // 36 mod 7 = 1
  };

  "ModInt multiplication large values"_test = [] {
    using Mint = ModInt<unsigned long long, 998244353>;

    // Test that overflow is handled correctly
    constexpr Mint a(500000000);
    constexpr Mint b(500000000);
    constexpr auto c = a * b;

    // 500000000^2 = 250000000000000000, mod 998244353 = 678139901
    static_assert(c.value == 678139901);
  };

  "ModInt *= operator"_test = [] {
    constexpr auto result = [] {
      ModInt<unsigned long long, 7> a(3);
      a *= ModInt<unsigned long long, 7>(4);
      return a;
    }();
    static_assert(result.value == 5);  // 12 mod 7 = 5
  };

  // ===========================================================================
  // Exponentiation
  // ===========================================================================

  "ModInt pow basic"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(Mint(2).pow(0).value == 1);
    static_assert(Mint(2).pow(1).value == 2);
    static_assert(Mint(2).pow(2).value == 4);
    static_assert(Mint(2).pow(3).value == 1);  // 8 mod 7 = 1
    static_assert(Mint(2).pow(10).value == 2); // 1024 mod 7 = 2
  };

  "ModInt pow Fermat's Little Theorem"_test = [] {
    // For prime p: a^(p-1) = 1 (mod p) for a != 0
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(Mint(1).pow(6).value == 1);
    static_assert(Mint(2).pow(6).value == 1);
    static_assert(Mint(3).pow(6).value == 1);
    static_assert(Mint(4).pow(6).value == 1);
    static_assert(Mint(5).pow(6).value == 1);
    static_assert(Mint(6).pow(6).value == 1);
  };

  "ModInt pow large exponent"_test = [] {
    using Mint = ModInt<unsigned long long, 998244353>;

    // 2^23 mod 998244353
    // 998244353 = 2^23 * 7 * 17 + 1, so 2^23 = (998244353 - 1) / (7 * 17) / 1
    // Actually, let's just compute: 2^23 = 8388608
    static_assert(Mint(2).pow(23).value == 8388608);

    // a^(p-1) = 1 for prime p
    static_assert(Mint(123456789).pow(998244352).value == 1);
  };

  // ===========================================================================
  // Modular Inverse
  // ===========================================================================

  "ModInt inv using Fermat"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // 3 * 5 = 15 = 1 (mod 7), so inv(3) = 5
    static_assert(Mint(3).inv().value == 5);
    static_assert((Mint(3) * Mint(3).inv()).value == 1);

    // Check all non-zero elements
    static_assert((Mint(1) * Mint(1).inv()).value == 1);
    static_assert((Mint(2) * Mint(2).inv()).value == 1);
    static_assert((Mint(3) * Mint(3).inv()).value == 1);
    static_assert((Mint(4) * Mint(4).inv()).value == 1);
    static_assert((Mint(5) * Mint(5).inv()).value == 1);
    static_assert((Mint(6) * Mint(6).inv()).value == 1);
  };

  "ModInt inv specific values"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // inv(1) = 1
    static_assert(Mint(1).inv().value == 1);
    // inv(2) = 4 (2 * 4 = 8 = 1 mod 7)
    static_assert(Mint(2).inv().value == 4);
    // inv(6) = 6 (6 * 6 = 36 = 1 mod 7)
    static_assert(Mint(6).inv().value == 6);
  };

  // ===========================================================================
  // Division
  // ===========================================================================

  "ModInt division"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // 6 / 2 = 6 * inv(2) = 6 * 4 = 24 mod 7 = 3
    static_assert((Mint(6) / Mint(2)).value == 3);

    // 1 / 3 = inv(3) = 5
    static_assert((Mint(1) / Mint(3)).value == 5);

    // Verify: (a / b) * b = a
    static_assert(((Mint(5) / Mint(3)) * Mint(3)).value == 5);
  };

  "ModInt /= operator"_test = [] {
    constexpr auto result = [] {
      ModInt<unsigned long long, 7> a(6);
      a /= ModInt<unsigned long long, 7>(2);
      return a;
    }();
    static_assert(result.value == 3);
  };

  // ===========================================================================
  // Extended GCD Inverse
  // ===========================================================================

  "ModInt invExtGcd"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // Should give same results as Fermat for prime modulus
    static_assert(Mint(3).invExtGcd().value == 5);
    static_assert((Mint(3) * Mint(3).invExtGcd()).value == 1);
  };

  // ===========================================================================
  // Utility Functions
  // ===========================================================================

  "ModInt val() and isZero()"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(Mint(0).val() == 0);
    static_assert(Mint(3).val() == 3);

    static_assert(Mint(0).isZero());
    static_assert(Mint(7).isZero());  // 7 mod 7 = 0
    static_assert(!Mint(3).isZero());
  };

  // ===========================================================================
  // Different Value Types
  // ===========================================================================

  "ModInt with unsigned int"_test = [] {
    using Mint = ModInt<unsigned int, 7u>;

    static_assert(Mint(10u).value == 3u);
    static_assert((Mint(3u) + Mint(5u)).value == 1u);
    static_assert((Mint(3u) * Mint(4u)).value == 5u);
    static_assert(Mint(3u).pow(6u).value == 1u);
  };

  // ===========================================================================
  // Algebraic Properties
  // ===========================================================================

  "ModInt commutativity"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert((Mint(2) + Mint(5)) == (Mint(5) + Mint(2)));
    static_assert((Mint(2) * Mint(5)) == (Mint(5) * Mint(2)));
  };

  "ModInt associativity"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    static_assert(((Mint(2) + Mint(3)) + Mint(4)) ==
                  (Mint(2) + (Mint(3) + Mint(4))));
    static_assert(((Mint(2) * Mint(3)) * Mint(4)) ==
                  (Mint(2) * (Mint(3) * Mint(4))));
  };

  "ModInt distributivity"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // a * (b + c) = a*b + a*c
    constexpr Mint a(2), b(3), c(4);
    static_assert((a * (b + c)) == (a * b + a * c));
  };

  "ModInt identity elements"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // Additive identity: a + 0 = a
    static_assert((Mint(5) + Mint(0)) == Mint(5));

    // Multiplicative identity: a * 1 = a
    static_assert((Mint(5) * Mint(1)) == Mint(5));
  };

  "ModInt inverse elements"_test = [] {
    using Mint = ModInt<unsigned long long, 7>;

    // Additive inverse: a + (-a) = 0
    static_assert((Mint(5) + (-Mint(5))) == Mint(0));

    // Multiplicative inverse: a * a^(-1) = 1
    static_assert((Mint(5) * Mint(5).inv()) == Mint(1));
  };

  return TestRegistry::result();
}
