#include "modular/montgomery.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // ===========================================================================
  // Montgomery64 Tests
  // ===========================================================================

  "Montgomery64 pow basic"_test = [] {
    Montgomery64 mont(17);

    // 3^0 = 1
    static_assert(Montgomery64(17).pow(3, 0) == 1);

    // 3^1 = 3
    static_assert(Montgomery64(17).pow(3, 1) == 3);

    // 3^2 = 9
    static_assert(Montgomery64(17).pow(3, 2) == 9);

    // 3^4 = 81 mod 17 = 13
    static_assert(Montgomery64(17).pow(3, 4) == 13);
  };

  "Montgomery64 pow Fermat"_test = [] {
    // a^(p-1) = 1 (mod p) for prime p
    static_assert(Montgomery64(17).pow(3, 16) == 1);
    static_assert(Montgomery64(17).pow(5, 16) == 1);
    static_assert(Montgomery64(17).pow(7, 16) == 1);
  };

  "Montgomery64 pow large prime"_test = [] {
    constexpr Montgomery64 mont(998244353);

    // a^(p-1) = 1 for any a != 0
    static_assert(mont.pow(2, 998244352) == 1);
    static_assert(mont.pow(123456789, 998244352) == 1);
  };

  "Montgomery64 conversion roundtrip"_test = [] {
    constexpr Montgomery64 mont(101);

    constexpr auto test = [](unsigned long long x) {
      Montgomery64 m(101);
      unsigned long long x_mont = m.toMont(x);
      return m.fromMont(x_mont) == x;
    };

    static_assert(test(0));
    static_assert(test(1));
    static_assert(test(42));
    static_assert(test(100));
  };

  "Montgomery64 multiplication"_test = [] {
    constexpr Montgomery64 mont(101);

    // a * b mod n via Montgomery
    constexpr auto test_mul = [](unsigned long long a, unsigned long long b,
                                 unsigned long long expected) {
      Montgomery64 m(101);
      unsigned long long a_mont = m.toMont(a);
      unsigned long long b_mont = m.toMont(b);
      unsigned long long c_mont = m.mul(a_mont, b_mont);
      return m.fromMont(c_mont) == expected;
    };

    static_assert(test_mul(3, 7, 21));    // 3 * 7 = 21
    static_assert(test_mul(50, 3, 49));   // 150 mod 101 = 49
    static_assert(test_mul(42, 37, 39));  // 1554 mod 101 = 39
  };

  // ===========================================================================
  // Montgomery32 Tests
  // ===========================================================================

  "Montgomery32 Fermat"_test = [] {
    static_assert(Montgomery32(101).pow(7, 100) == 1);
  };

  // ===========================================================================
  // Montgomery128 Tests
  // ===========================================================================

  "Montgomery128 basic"_test = [] {
    static_assert(Montgomery128(UInt128{101}).pow(UInt128{7}, UInt128{100}) ==
                  UInt128{1});
  };

  "Montgomery128 Mersenne prime"_test = [] {
    // 2^61 - 1 = 2305843009213693951 (Mersenne prime M61)
    constexpr UInt128 m61 = UInt128{2305843009213693951ull};
    static_assert(Montgomery128(m61).pow(UInt128{12345}, m61 - UInt128{1}) ==
                  UInt128{1});
  };

  // ===========================================================================
  // Montgomery256 Tests
  // ===========================================================================

  "Montgomery256 basic"_test = [] {
    static_assert(Montgomery256(UInt256{101}).pow(UInt256{7}, UInt256{100}) ==
                  UInt256{1});
  };

  "Montgomery256 Mersenne prime"_test = [] {
    // 2^127 - 1 (Mersenne prime M127)
    constexpr UInt256 m127 = (UInt256{1} << 127) - UInt256{1};
    static_assert(Montgomery256(m127).pow(UInt256{2}, m127 - UInt256{1}) ==
                  UInt256{1});
  };

  // ===========================================================================
  // Montgomery512 Tests
  // ===========================================================================

  "Montgomery512 basic"_test = [] {
    static_assert(Montgomery512(UInt512{101}).pow(UInt512{7}, UInt512{100}) ==
                  UInt512{1});
  };

  // ===========================================================================
  // MontgomeryBigInt Tests
  // ===========================================================================

  "MontgomeryBigInt alias"_test = [] {
    static_assert(
        MontgomeryBigInt<128>(UInt128{17}).pow(UInt128{3}, UInt128{16}) ==
        UInt128{1});
  };

  // ===========================================================================
  // DynamicMontgomery Tests
  // ===========================================================================

  "DynamicMontgomery basic"_test = [] {
    DynamicMontgomery mont(DynamicUInt{101});
    expectEq(mont.pow(DynamicUInt{7}, DynamicUInt{100}), DynamicUInt{1});
  };

  "DynamicMontgomery Fermat small prime"_test = [] {
    DynamicMontgomery mont(DynamicUInt{17});
    // a^(p-1) = 1 mod p
    expectEq(mont.pow(DynamicUInt{3}, DynamicUInt{16}), DynamicUInt{1});
    expectEq(mont.pow(DynamicUInt{5}, DynamicUInt{16}), DynamicUInt{1});
    expectEq(mont.pow(DynamicUInt{7}, DynamicUInt{16}), DynamicUInt{1});
  };

  "DynamicMontgomery pow values"_test = [] {
    DynamicMontgomery mont(DynamicUInt{17});
    // 3^0 = 1
    expectEq(mont.pow(DynamicUInt{3}, DynamicUInt{0}), DynamicUInt{1});
    // 3^1 = 3
    expectEq(mont.pow(DynamicUInt{3}, DynamicUInt{1}), DynamicUInt{3});
    // 3^2 = 9
    expectEq(mont.pow(DynamicUInt{3}, DynamicUInt{2}), DynamicUInt{9});
    // 3^4 = 81 mod 17 = 13
    expectEq(mont.pow(DynamicUInt{3}, DynamicUInt{4}), DynamicUInt{13});
  };

  "DynamicMontgomery large prime"_test = [] {
    // 998244353 = 2^23 * 7 * 17 + 1
    DynamicMontgomery mont(DynamicUInt{998244353});
    expectEq(mont.pow(DynamicUInt{2}, DynamicUInt{998244352}), DynamicUInt{1});
  };

  "DynamicMontgomery conversion roundtrip"_test = [] {
    DynamicMontgomery mont(DynamicUInt{101});
    DynamicUInt x{42};
    DynamicUInt x_mont = mont.toMont(x);
    DynamicUInt x_back = mont.fromMont(x_mont);
    expectEq(x_back, x);
  };

  "DynamicMontgomery multiplication"_test = [] {
    DynamicMontgomery mont(DynamicUInt{101});
    // 42 * 37 mod 101 = 1554 mod 101 = 39
    DynamicUInt a_mont = mont.toMont(DynamicUInt{42});
    DynamicUInt b_mont = mont.toMont(DynamicUInt{37});
    DynamicUInt c_mont = mont.mul(a_mont, b_mont);
    expectEq(mont.fromMont(c_mont), DynamicUInt{39});
  };

  return TestRegistry::result();
}
