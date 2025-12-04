#include "modular/number_theory.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // ===========================================================================
  // GCD Tests
  // ===========================================================================

  "gcd basic"_test = [] {
    static_assert(gcd(12u, 18u) == 6u);
    static_assert(gcd(48u, 18u) == 6u);
    static_assert(gcd(100u, 35u) == 5u);
  };

  "gcd coprime numbers"_test = [] {
    static_assert(gcd(7u, 11u) == 1u);
    static_assert(gcd(13u, 17u) == 1u);
    static_assert(gcd(15u, 28u) == 1u);
  };

  "gcd with zero"_test = [] {
    static_assert(gcd(0u, 5u) == 5u);
    static_assert(gcd(5u, 0u) == 5u);
    static_assert(gcd(0u, 0u) == 0u);
  };

  "gcd same numbers"_test = [] {
    static_assert(gcd(42u, 42u) == 42u);
    static_assert(gcd(1u, 1u) == 1u);
  };

  "gcd one divides other"_test = [] {
    static_assert(gcd(12u, 4u) == 4u);
    static_assert(gcd(100u, 25u) == 25u);
  };

  "gcd large numbers"_test = [] {
    static_assert(gcd(1000000007ull, 998244353ull) == 1ull);  // Both prime
    static_assert(gcd(1000000000ull, 500000000ull) == 500000000ull);
  };

  // ===========================================================================
  // LCM Tests
  // ===========================================================================

  "lcm basic"_test = [] {
    static_assert(lcm(4u, 6u) == 12u);
    static_assert(lcm(3u, 5u) == 15u);
    static_assert(lcm(12u, 18u) == 36u);
  };

  "lcm coprime numbers"_test = [] {
    static_assert(lcm(7u, 11u) == 77u);
    static_assert(lcm(3u, 4u) == 12u);
  };

  "lcm with one"_test = [] {
    static_assert(lcm(1u, 5u) == 5u);
    static_assert(lcm(5u, 1u) == 5u);
  };

  "lcm with zero"_test = [] {
    static_assert(lcm(0u, 5u) == 0u);
    static_assert(lcm(5u, 0u) == 0u);
  };

  "lcm same numbers"_test = [] {
    static_assert(lcm(42u, 42u) == 42u);
  };

  // ===========================================================================
  // Modular Inverse Tests
  // ===========================================================================

  "modInverse basic"_test = [] {
    // 3 * 5 = 15 = 1 (mod 7)
    static_assert(modInverse(3ull, 7ull) == 5ull);

    // 5 * 9 = 45 = 1 (mod 11)
    static_assert(modInverse(5ull, 11ull) == 9ull);

    // 2 * 4 = 8 = 1 (mod 7)
    static_assert(modInverse(2ull, 7ull) == 4ull);
  };

  "modInverse no inverse"_test = [] {
    // gcd(2, 4) = 2 != 1, no inverse
    static_assert(modInverse(2ull, 4ull) == 0ull);

    // gcd(6, 9) = 3 != 1, no inverse
    static_assert(modInverse(6ull, 9ull) == 0ull);
  };

  "modInverse of 1"_test = [] {
    static_assert(modInverse(1ull, 7ull) == 1ull);
    static_assert(modInverse(1ull, 100ull) == 1ull);
  };

  "modInverse verification"_test = [] {
    // Verify: a * inv(a) = 1 (mod m)
    constexpr auto verify = [](unsigned long long a, unsigned long long m) {
      unsigned long long inv = modInverse(a, m);
      return (a * inv) % m == 1;
    };

    static_assert(verify(3, 7));
    static_assert(verify(5, 11));
    static_assert(verify(7, 13));
    static_assert(verify(123, 1000000007));
  };

  // ===========================================================================
  // Miller-Rabin Primality Tests
  // ===========================================================================

  "isPrime small primes"_test = [] {
    static_assert(isPrime(2ull));
    static_assert(isPrime(3ull));
    static_assert(isPrime(5ull));
    static_assert(isPrime(7ull));
    static_assert(isPrime(11ull));
    static_assert(isPrime(13ull));
    static_assert(isPrime(17ull));
    static_assert(isPrime(19ull));
    static_assert(isPrime(23ull));
    static_assert(isPrime(29ull));
    static_assert(isPrime(31ull));
    static_assert(isPrime(37ull));
    static_assert(isPrime(41ull));
    static_assert(isPrime(43ull));
    static_assert(isPrime(47ull));
  };

  "isPrime small composites"_test = [] {
    static_assert(!isPrime(0ull));
    static_assert(!isPrime(1ull));
    static_assert(!isPrime(4ull));
    static_assert(!isPrime(6ull));
    static_assert(!isPrime(8ull));
    static_assert(!isPrime(9ull));
    static_assert(!isPrime(10ull));
    static_assert(!isPrime(12ull));
    static_assert(!isPrime(15ull));
    static_assert(!isPrime(21ull));
    static_assert(!isPrime(25ull));
    static_assert(!isPrime(49ull));
  };

  "isPrime medium primes"_test = [] {
    static_assert(isPrime(97ull));
    static_assert(isPrime(101ull));
    static_assert(isPrime(127ull));
    static_assert(isPrime(131ull));
    static_assert(isPrime(997ull));
    static_assert(isPrime(1009ull));
  };

  "isPrime large primes"_test = [] {
    static_assert(isPrime(1000000007ull));
    static_assert(isPrime(998244353ull));
    static_assert(isPrime(1000000009ull));

    // Mersenne primes
    static_assert(isPrime(127ull));                  // 2^7 - 1
    static_assert(isPrime(8191ull));                 // 2^13 - 1
    static_assert(isPrime(131071ull));               // 2^17 - 1
    static_assert(isPrime(524287ull));               // 2^19 - 1
    static_assert(isPrime(2147483647ull));           // 2^31 - 1
  };

  "isPrime large composites"_test = [] {
    // Products of primes
    static_assert(!isPrime(1000000001ull));  // 7 * 142857143
    static_assert(!isPrime(1000000011ull));  // 3^2 * 239 * 4649
    static_assert(!isPrime(999999999ull));   // 3^4 * 37 * 333667

    // Carmichael numbers (would fool Fermat test)
    static_assert(!isPrime(561ull));    // = 3 * 11 * 17
    static_assert(!isPrime(1105ull));   // = 5 * 13 * 17
    static_assert(!isPrime(1729ull));   // = 7 * 13 * 19 (Hardy-Ramanujan)
    static_assert(!isPrime(2465ull));   // = 5 * 17 * 29
    static_assert(!isPrime(2821ull));   // = 7 * 13 * 31
  };

  "isPrime squares of primes"_test = [] {
    static_assert(!isPrime(4ull));      // 2^2
    static_assert(!isPrime(9ull));      // 3^2
    static_assert(!isPrime(25ull));     // 5^2
    static_assert(!isPrime(49ull));     // 7^2
    static_assert(!isPrime(121ull));    // 11^2
    static_assert(!isPrime(169ull));    // 13^2
  };

  "isPrime unsigned int overload"_test = [] {
    static_assert(isPrime(7u));
    static_assert(isPrime(1000000007u));
    static_assert(!isPrime(4u));
    static_assert(!isPrime(1000000001u));
  };

  // ===========================================================================
  // Euler Totient Tests
  // ===========================================================================

  "eulerTotient of 1"_test = [] {
    static_assert(eulerTotient(1u) == 1u);
  };

  "eulerTotient of primes"_test = [] {
    // φ(p) = p - 1 for prime p
    static_assert(eulerTotient(2u) == 1u);
    static_assert(eulerTotient(3u) == 2u);
    static_assert(eulerTotient(5u) == 4u);
    static_assert(eulerTotient(7u) == 6u);
    static_assert(eulerTotient(11u) == 10u);
    static_assert(eulerTotient(13u) == 12u);
  };

  "eulerTotient of prime powers"_test = [] {
    // φ(p^k) = p^(k-1) * (p - 1)
    static_assert(eulerTotient(4u) == 2u);    // 2^2: 2^1 * 1 = 2
    static_assert(eulerTotient(8u) == 4u);    // 2^3: 2^2 * 1 = 4
    static_assert(eulerTotient(9u) == 6u);    // 3^2: 3^1 * 2 = 6
    static_assert(eulerTotient(27u) == 18u);  // 3^3: 3^2 * 2 = 18
    static_assert(eulerTotient(25u) == 20u);  // 5^2: 5^1 * 4 = 20
  };

  "eulerTotient of composites"_test = [] {
    // φ(6) = φ(2) * φ(3) = 1 * 2 = 2  ({1, 5})
    static_assert(eulerTotient(6u) == 2u);

    // φ(10) = φ(2) * φ(5) = 1 * 4 = 4  ({1, 3, 7, 9})
    static_assert(eulerTotient(10u) == 4u);

    // φ(12) = φ(4) * φ(3) = 2 * 2 = 4  ({1, 5, 7, 11})
    static_assert(eulerTotient(12u) == 4u);

    // φ(20) = φ(4) * φ(5) = 2 * 4 = 8
    static_assert(eulerTotient(20u) == 8u);

    // φ(100) = φ(4) * φ(25) = 2 * 20 = 40
    static_assert(eulerTotient(100u) == 40u);
  };

  // ===========================================================================
  // Extended GCD Tests
  // ===========================================================================

  "extGcd basic"_test = [] {
    // gcd(12, 18) = 6
    constexpr auto result = extGcd(12ull, 18ull);
    static_assert(result.gcd == 6ull);
  };

  "extGcd coprime"_test = [] {
    // gcd(7, 11) = 1
    constexpr auto result = extGcd(7ull, 11ull);
    static_assert(result.gcd == 1ull);
  };

  "extGcd with zero"_test = [] {
    constexpr auto result = extGcd(5ull, 0ull);
    static_assert(result.gcd == 5ull);
    static_assert(result.x == 1ull);
    static_assert(result.y == 0ull);
  };

  // ===========================================================================
  // findFirstFactor Tests
  // ===========================================================================

  "findFirstFactor edge cases"_test = [] {
    expectEq(findFirstFactor(0ull), 0ull);
    expectEq(findFirstFactor(1ull), 1ull);
  };

  "findFirstFactor small primes"_test = [] {
    expectEq(findFirstFactor(2ull), 2ull);
    expectEq(findFirstFactor(3ull), 3ull);
    expectEq(findFirstFactor(5ull), 5ull);
    expectEq(findFirstFactor(7ull), 7ull);
    expectEq(findFirstFactor(97ull), 97ull);
  };

  "findFirstFactor composites"_test = [] {
    expectEq(findFirstFactor(4ull), 2ull);
    expectEq(findFirstFactor(6ull), 2ull);
    expectEq(findFirstFactor(9ull), 3ull);
    expectEq(findFirstFactor(15ull), 3ull);
    expectEq(findFirstFactor(49ull), 7ull);
    expectEq(findFirstFactor(121ull), 11ull);
  };

  "findFirstFactor large numbers"_test = [] {
    expectEq(findFirstFactor(1000000007ull), 1000000007ull);  // Prime
    expectEq(findFirstFactor(1000000001ull), 7ull);           // 7 × 142857143
    expectEq(findFirstFactor(999999999989ull), 999999999989ull);  // Large prime
  };

  "findFirstFactor unsigned int overload"_test = [] {
    expectEq(findFirstFactor(6u), 2ull);
    expectEq(findFirstFactor(15u), 3ull);
    expectEq(findFirstFactor(97u), 97ull);
  };

  return TestRegistry::result();
}
