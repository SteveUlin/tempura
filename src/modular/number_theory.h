#pragma once

#include "meta/containers.h"
#include "meta/utility.h"
#include "modular/montgomery.h"

// Number theory utilities for modular arithmetic
//
// Includes:
// - Extended Euclidean algorithm
// - Modular inverse
// - Miller-Rabin primality test
// - GCD and LCM

namespace tempura {

// =============================================================================
// Extended Euclidean Algorithm
// =============================================================================
//
// Given integers a and b, find integers x and y such that:
//   ax + by = gcd(a, b)
//
// This is the foundation for computing modular inverses.

template <typename T>
struct ExtGcdResult {
  T gcd;
  T x;  // Coefficient for a (may be negative, represented carefully)
  T y;  // Coefficient for b
  bool x_negative;
  bool y_negative;
};

// Extended GCD for unsigned types
// Returns gcd and coefficients x, y such that a*x - b*y = gcd or a*x + b*y = gcd
// depending on signs
template <typename T>
constexpr auto extGcd(T a, T b) -> ExtGcdResult<T> {
  if (b == T{0}) {
    return {a, T{1}, T{0}, false, false};
  }

  // Iterative extended Euclidean algorithm
  T old_r = a, r = b;
  T old_s = T{1}, s = T{0};
  T old_t = T{0}, t = T{1};

  // Track signs separately since T is unsigned
  bool old_s_neg = false, s_neg = false;
  bool old_t_neg = false, t_neg = false;

  while (r != T{0}) {
    T quotient = old_r / r;

    // Update remainders
    T temp_r = old_r - quotient * r;
    old_r = r;
    r = temp_r;

    // Update s coefficients with sign tracking
    T qs = quotient * s;
    T new_s;
    bool new_s_neg;

    if (old_s_neg == s_neg) {
      if (old_s >= qs) {
        new_s = old_s - qs;
        new_s_neg = old_s_neg;
      } else {
        new_s = qs - old_s;
        new_s_neg = !old_s_neg;
      }
    } else {
      new_s = old_s + qs;
      new_s_neg = old_s_neg;
    }

    old_s = s;
    old_s_neg = s_neg;
    s = new_s;
    s_neg = new_s_neg;

    // Update t coefficients
    T qt = quotient * t;
    T new_t;
    bool new_t_neg;

    if (old_t_neg == t_neg) {
      if (old_t >= qt) {
        new_t = old_t - qt;
        new_t_neg = old_t_neg;
      } else {
        new_t = qt - old_t;
        new_t_neg = !old_t_neg;
      }
    } else {
      new_t = old_t + qt;
      new_t_neg = old_t_neg;
    }

    old_t = t;
    old_t_neg = t_neg;
    t = new_t;
    t_neg = new_t_neg;
  }

  return {old_r, old_s, old_t, old_s_neg, old_t_neg};
}

// =============================================================================
// Modular Inverse
// =============================================================================

// Compute modular inverse of a modulo m
// Returns a^(-1) mod m if gcd(a, m) = 1, otherwise returns 0
template <typename T>
constexpr auto modInverse(T a, T m) -> T {
  auto result = extGcd(a, m);

  if (result.gcd != T{1}) {
    return T{0};  // No inverse exists
  }

  if (result.x_negative) {
    return m - (result.x % m);
  } else {
    return result.x % m;
  }
}

// =============================================================================
// GCD and LCM
// =============================================================================

template <typename T>
constexpr auto gcd(T a, T b) -> T {
  while (b != T{0}) {
    T temp = b;
    b = a % b;
    a = temp;
  }
  return a;
}

template <typename T>
constexpr auto lcm(T a, T b) -> T {
  if (a == T{0} || b == T{0}) return T{0};
  return (a / gcd(a, b)) * b;  // Divide first to avoid overflow
}

// =============================================================================
// Miller-Rabin Primality Test
// =============================================================================
//
// A probabilistic primality test that is deterministic for 64-bit integers
// when using specific witness sets.
//
// Algorithm:
// 1. Write n-1 = 2^s * d where d is odd
// 2. For each witness a:
//    - Compute x = a^d mod n
//    - If x = 1 or x = n-1, continue to next witness
//    - Square x up to s-1 times:
//      - If x = n-1, continue to next witness
//      - If x = 1, n is composite
//    - If we never got n-1, n is composite
// 3. If all witnesses pass, n is (probably) prime
//
// For deterministic results on 64-bit integers, we use these witnesses:
// {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37}
// This is proven sufficient for all n < 2^64.

namespace detail {

// Modular multiplication for unsigned long long using 128-bit intermediate
constexpr auto mulmod(unsigned long long a, unsigned long long b,
                      unsigned long long m) -> unsigned long long {
  return static_cast<unsigned long long>((__uint128_t(a) * b) % m);
}

// Modular exponentiation
constexpr auto powmod(unsigned long long base, unsigned long long exp,
                      unsigned long long mod) -> unsigned long long {
  unsigned long long result = 1;
  base %= mod;
  while (exp > 0) {
    if (exp & 1) {
      result = mulmod(result, base, mod);
    }
    base = mulmod(base, base, mod);
    exp >>= 1;
  }
  return result;
}

// Check if n is a strong probable prime to base a
// PRECONDITION: n > 2, n is odd, 1 < a < n-1
constexpr auto millerRabinWitness(unsigned long long n, unsigned long long a,
                                  unsigned long long d, unsigned long long s)
    -> bool {
  // Compute a^d mod n
  unsigned long long x = powmod(a, d, n);

  // If x = 1 or x = n-1, passes this witness
  if (x == 1 || x == n - 1) {
    return true;
  }

  // Square s-1 times
  for (unsigned long long i = 1; i < s; ++i) {
    x = mulmod(x, x, n);

    if (x == n - 1) {
      return true;
    }
    if (x == 1) {
      return false;  // Found non-trivial sqrt of 1
    }
  }

  return false;
}

// Generate first N primes at compile time using trial division
template <SizeT N>
constexpr auto generateSmallPrimes() -> MinimalArray<unsigned long long, N> {
  MinimalArray<unsigned long long, N> primes{};
  if constexpr (N == 0) return primes;

  primes[0] = 2;
  SizeT count = 1;
  unsigned long long candidate = 3;

  while (count < N) {
    bool is_prime = true;
    for (SizeT i = 0; i < count && primes[i] * primes[i] <= candidate; ++i) {
      if (candidate % primes[i] == 0) {
        is_prime = false;
        break;
      }
    }
    if (is_prime) {
      primes[count++] = candidate;
    }
    candidate += 2;
  }
  return primes;
}

inline constexpr auto kSmallPrimes = generateSmallPrimes<100>();

// Verify generator produces correct primes
static_assert(kSmallPrimes[0] == 2);
static_assert(kSmallPrimes[1] == 3);
static_assert(kSmallPrimes[4] == 11);
static_assert(kSmallPrimes[24] == 97);   // 25th prime
static_assert(kSmallPrimes[99] == 541);  // 100th prime

}  // namespace detail

// Miller-Rabin primality test for unsigned long long
// Returns true if n is prime, false if composite
// Deterministic for all n < 2^64
constexpr auto isPrime(unsigned long long n) -> bool {
  // Handle small cases
  if (n < 2) return false;
  if (n == 2) return true;
  if (n == 3) return true;
  if ((n & 1) == 0) return false;  // Even

  // Handle small primes that might be witnesses
  if (n == 5) return true;
  if (n == 7) return true;
  if (n == 11) return true;
  if (n == 13) return true;
  if (n == 17) return true;
  if (n == 19) return true;
  if (n == 23) return true;
  if (n == 29) return true;
  if (n == 31) return true;
  if (n == 37) return true;

  // Check divisibility by small primes
  if (n % 3 == 0) return false;
  if (n % 5 == 0) return false;
  if (n % 7 == 0) return false;
  if (n % 11 == 0) return false;
  if (n % 13 == 0) return false;

  // Write n-1 = 2^s * d where d is odd
  unsigned long long d = n - 1;
  unsigned long long s = 0;
  while ((d & 1) == 0) {
    d >>= 1;
    ++s;
  }

  // Deterministic witnesses for n < 2^64
  // Source: https://miller-rabin.appspot.com/
  constexpr unsigned long long witnesses[] = {2,  3,  5,  7,  11, 13,
                                              17, 19, 23, 29, 31, 37};

  for (unsigned long long a : witnesses) {
    if (a >= n - 1) continue;  // Skip if witness >= n-1

    if (!detail::millerRabinWitness(n, a, d, s)) {
      return false;
    }
  }

  return true;
}

// Overload for unsigned int
constexpr auto isPrime(unsigned int n) -> bool {
  return isPrime(static_cast<unsigned long long>(n));
}

// =============================================================================
// Euler's Totient Function (for small values)
// =============================================================================

// Compute Euler's totient function φ(n)
// φ(n) = count of integers k where 1 <= k <= n and gcd(k, n) = 1
template <typename T>
constexpr auto eulerTotient(T n) -> T {
  if (n == T{0}) return T{0};
  if (n == T{1}) return T{1};

  T result = n;
  T temp = n;

  // Check for factor of 2
  if (temp % T{2} == T{0}) {
    result = result - result / T{2};
    while (temp % T{2} == T{0}) {
      temp = temp / T{2};
    }
  }

  // Check odd factors
  for (T i = T{3}; i * i <= temp; i = i + T{2}) {
    if (temp % i == T{0}) {
      result = result - result / i;
      while (temp % i == T{0}) {
        temp = temp / i;
      }
    }
  }

  // If temp > 1, then it's a prime factor
  if (temp > T{1}) {
    result = result - result / temp;
  }

  return result;
}

// =============================================================================
// Factorization
// =============================================================================

// Find the smallest prime factor of n using trial division
// Returns n if n is prime (or n <= 1)
constexpr auto findFirstFactor(unsigned long long n) -> unsigned long long {
  if (n <= 1) return n;

  // Check small primes from table
  for (SizeT i = 0; i < detail::kSmallPrimes.size(); ++i) {
    unsigned long long p = detail::kSmallPrimes[i];
    if (p * p > n) return n;  // n is prime
    if (n % p == 0) return p;
  }

  // Continue with 6k±1 wheel after table ends (541 = 6×90+1)
  // Next candidates: 6×91-1=545, 6×91+1=547, 6×92-1=551, ...
  unsigned long long i = 545;
  while (i * i <= n) {
    if (n % i == 0) return i;
    if (n % (i + 2) == 0) return i + 2;
    i += 6;
  }

  return n;  // n is prime
}

// Overload for unsigned int
constexpr auto findFirstFactor(unsigned int n) -> unsigned long long {
  return findFirstFactor(static_cast<unsigned long long>(n));
}

// =============================================================================
// Static assertions
// =============================================================================

// GCD tests
static_assert(gcd(12u, 18u) == 6);
static_assert(gcd(17u, 31u) == 1);
static_assert(gcd(0u, 5u) == 5);
static_assert(gcd(100u, 0u) == 100);

// LCM tests
static_assert(lcm(4u, 6u) == 12);
static_assert(lcm(3u, 5u) == 15);

// Modular inverse tests
static_assert(modInverse(3ull, 7ull) == 5);   // 3 * 5 = 15 = 1 (mod 7)
static_assert(modInverse(5ull, 11ull) == 9);  // 5 * 9 = 45 = 1 (mod 11)
static_assert(modInverse(2ull, 4ull) == 0);   // No inverse (gcd = 2)

// Primality tests
static_assert(isPrime(2ull));
static_assert(isPrime(3ull));
static_assert(isPrime(5ull));
static_assert(isPrime(7ull));
static_assert(isPrime(11ull));
static_assert(isPrime(13ull));
static_assert(isPrime(17ull));
static_assert(isPrime(19ull));
static_assert(isPrime(23ull));
static_assert(isPrime(97ull));
static_assert(isPrime(101ull));
static_assert(isPrime(1000000007ull));
static_assert(isPrime(998244353ull));

static_assert(!isPrime(0ull));
static_assert(!isPrime(1ull));
static_assert(!isPrime(4ull));
static_assert(!isPrime(6ull));
static_assert(!isPrime(9ull));
static_assert(!isPrime(15ull));
static_assert(!isPrime(100ull));
static_assert(!isPrime(1000000001ull));  // = 7 × 142857143

// Euler totient tests
static_assert(eulerTotient(1u) == 1);
static_assert(eulerTotient(2u) == 1);
static_assert(eulerTotient(6u) == 2);   // {1, 5}
static_assert(eulerTotient(7u) == 6);   // Prime: φ(p) = p-1
static_assert(eulerTotient(10u) == 4);  // {1, 3, 7, 9}
static_assert(eulerTotient(12u) == 4);  // {1, 5, 7, 11}

// findFirstFactor tests
static_assert(findFirstFactor(0ull) == 0);
static_assert(findFirstFactor(1ull) == 1);
static_assert(findFirstFactor(2ull) == 2);   // Prime returns itself
static_assert(findFirstFactor(3ull) == 3);
static_assert(findFirstFactor(4ull) == 2);
static_assert(findFirstFactor(6ull) == 2);
static_assert(findFirstFactor(15ull) == 3);
static_assert(findFirstFactor(49ull) == 7);
static_assert(findFirstFactor(97ull) == 97);  // Prime
static_assert(findFirstFactor(100ull) == 2);
static_assert(findFirstFactor(1000000007ull) == 1000000007);  // Large prime
static_assert(findFirstFactor(1000000001ull) == 7);           // = 7 × 142857143

}  // namespace tempura
