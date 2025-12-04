#pragma once

#include "bigint/bigint.h"
#include "bigint/dynamic_bigint.h"
#include "meta/utility.h"

// Montgomery multiplication for efficient modular arithmetic
//
// Montgomery form transforms numbers into a special representation where
// modular multiplication can be performed without division. This is
// particularly efficient for repeated multiplications (e.g., exponentiation).
//
// Key insight: Division by powers of 2 is cheap (just bit shifts), but
// division by arbitrary N is expensive. Montgomery form lets us do
// "division by R" (a power of 2) instead of "division by N".
//
// For a modulus N and R = 2^k (where R > N and gcd(R, N) = 1):
//   - Montgomery form of x: x' = xR mod N
//   - Montgomery multiplication: REDC(a' * b') = abR mod N
//   - The REDC function eliminates the factor of R efficiently
//
// Setup cost: Computing R, R^2 mod N, and N^(-1) mod R
// Per-multiply cost: ~2 multiplications + some additions (no division!)
//
// Best for: Modular exponentiation, repeated multiplications with same modulus
// Not worth it for: Single multiplication

namespace tempura {

// =============================================================================
// Montgomery<T> - Montgomery multiplication context for modulus N
// =============================================================================
//
// T should be an unsigned integer type (uint32_t, uint64_t, etc.)
// For uint64_t, we use __uint128_t for intermediate products.

template <typename T, typename Wide>
struct Montgomery {

  T mod;      // The modulus N (must be odd)
  T r;        // R mod N, where R = 2^bits
  T r2;       // R^2 mod N (for converting to Montgomery form)
  T n_inv;    // -N^(-1) mod R (for REDC)

  static constexpr SizeT kBits = sizeof(T) * 8;

  // --- Construction ---
  //
  // PRECONDITION: n is odd and n > 1
  // (Montgomery requires gcd(R, N) = 1, which holds iff N is odd)

  constexpr explicit Montgomery(T n) : mod(n) {
    // R = 2^kBits, but we can't represent it directly in T
    // R mod N: since R = 2^kBits, R mod N = (2^kBits) mod N
    // We compute this as: -N mod 2^kBits gives us what we need
    // Actually: R mod N = 2^kBits mod N

    // Compute R mod N
    // R = 2^64 for uint64_t. We can compute 2^64 mod N as:
    // 2^64 mod N = (2^64 - N*floor(2^64/N)) = 2^64 - N*q where q = floor(2^64/N)
    // But 2^64 can't be represented. Instead:
    // 2^64 mod N = ((2^64 - 1) mod N + 1) mod N = (T(-1) mod N + 1) mod N
    // But T(-1) = max value, and (max % N + 1) might overflow...
    //
    // Simpler approach: R mod N = (0 - N) mod N in two's complement?
    // No, that's 0.
    //
    // Let's compute differently:
    // R mod N where R = 2^k
    // Start with r = 1, then square k times, taking mod N each time?
    // That's O(k) which is fine for setup.

    // Actually, for R = 2^64:
    // 2^64 mod N = 2^64 - N * floor(2^64 / N)
    // We can compute floor(2^64 / N) as ((2^64 - 1) / N) or similar
    // Let's use: 2^64 mod N = (2^63 mod N) * 2 mod N, applied twice from 2^32

    r = computeRModN(n);

    // R^2 mod N: just square R mod N
    r2 = mulmod(r, r, n);

    // Compute N^(-1) mod R using extended binary GCD
    // We need n_inv such that N * n_inv = -1 (mod R)
    // i.e., N * n_inv + 1 = 0 (mod R)
    // i.e., N * n_inv = R - 1 = -1 (mod R)
    // Actually, we want: N * n_inv = -1 (mod R)
    // So n_inv = -N^(-1) mod R
    n_inv = computeNegInvModR(n);
  }

  // --- Convert to/from Montgomery form ---

  // Convert x to Montgomery form: x' = xR mod N
  constexpr auto toMont(T x) const -> T {
    // x' = x * R mod N = x * R mod N
    // We compute this as REDC(x * R^2) = x * R^2 * R^(-1) = x * R
    return redc(static_cast<Wide>(x) * static_cast<Wide>(r2));
  }

  // Convert x' from Montgomery form: x = x'R^(-1) mod N
  constexpr auto fromMont(T x_mont) const -> T {
    // REDC(x') = x' * R^(-1) mod N = x
    return redc(static_cast<Wide>(x_mont));
  }

  // --- Montgomery multiplication ---

  // Multiply two Montgomery-form numbers: REDC(a' * b') = (ab)R mod N
  constexpr auto mul(T a_mont, T b_mont) const -> T {
    return redc(static_cast<Wide>(a_mont) * static_cast<Wide>(b_mont));
  }

  // Square a Montgomery-form number
  constexpr auto sqr(T a_mont) const -> T {
    return redc(static_cast<Wide>(a_mont) * static_cast<Wide>(a_mont));
  }

  // --- Modular exponentiation ---

  // Compute base^exp mod N using Montgomery multiplication
  // Input: base in normal form (not Montgomery)
  // Output: result in normal form
  constexpr auto pow(T base, T exp) const -> T {
    if (exp == T{0}) {
      return T{1} % mod;
    }

    // Convert to Montgomery form
    T result = toMont(T{1});  // 1 in Montgomery form = R mod N
    T b = toMont(base % mod);

    while (exp > T{0}) {
      if ((exp & T{1}) != T{0}) {
        result = mul(result, b);
      }
      b = sqr(b);
      exp >>= 1;
    }

    // Convert back from Montgomery form
    return fromMont(result);
  }

  // --- REDC (Montgomery Reduction) ---
  //
  // Given T (a Wide value, i.e., product of two T values),
  // compute T * R^(-1) mod N without division.
  //
  // Algorithm:
  //   m = (T mod R) * N' mod R      // N' = -N^(-1) mod R
  //   t = (T + m*N) / R
  //   if t >= N: t = t - N
  //   return t
  //
  // Key insight: m*N makes (T + m*N) divisible by R, so division is exact.

  constexpr auto redc(Wide t) const -> T {
    // m = (t mod R) * n_inv mod R
    // Since R = 2^kBits, "mod R" is just taking low bits
    T t_lo = static_cast<T>(t);
    T m = t_lo * n_inv;  // Automatically mod R due to fixed-width

    // t + m*N, then divide by R (right shift)
    Wide mn = static_cast<Wide>(m) * static_cast<Wide>(mod);
    Wide sum = t + mn;
    T result = static_cast<T>(sum >> kBits);

    // Conditional subtraction
    if (result >= mod) {
      result -= mod;
    }

    return result;
  }

 private:
  // Compute R mod N where R = 2^kBits
  static constexpr auto computeRModN(T n) -> T {
    // We compute 2^kBits mod n by repeated doubling
    // Start with 2^(kBits/2) which fits in T, then double kBits/2 times
    constexpr SizeT kHalfBits = kBits / 2;
    T r = (T{1} << kHalfBits) % n;
    for (SizeT i = 0; i < kHalfBits; ++i) {
      r = (r << 1);
      if (r >= n) r -= n;
    }
    return r;
  }

  // Compute -N^(-1) mod R using Hensel lifting
  // We need x such that N * x = -1 (mod R)
  //
  // Start with x = 1 (valid mod 2 since N is odd)
  // Then lift: x = x * (2 - N * x) doubles the precision each step
  static constexpr auto computeNegInvModR(T n) -> T {
    // Newton-Hensel iteration for modular inverse
    // x_{i+1} = x_i * (2 - n * x_i) mod 2^(2^i)
    // After log2(kBits) iterations, we have inverse mod 2^kBits

    T x = T{1};  // Valid: n * 1 = n = 1 (mod 2) since n is odd

    // Each iteration doubles the number of correct bits
    // We need ceil(log2(kBits)) iterations
    // For 64 bits: 6 iterations (2^6 = 64)
    // For 32 bits: 5 iterations (2^5 = 32)
    // For 128 bits: 7 iterations (2^7 = 128)
    constexpr auto log2Ceil = [](SizeT n) {
      SizeT result = 0;
      while ((SizeT{1} << result) < n) ++result;
      return result;
    };
    constexpr int iterations = log2Ceil(kBits);

    for (int i = 0; i < iterations; ++i) {
      x = x * (T{2} - n * x);  // Automatically mod R due to overflow
    }

    // x is now N^(-1) mod R
    // We want -N^(-1) mod R = R - x = -x (in two's complement)
    return T{0} - x;
  }

  // Regular modular multiplication for setup
  static constexpr auto mulmod(T a, T b, T n) -> T {
    return static_cast<T>((static_cast<Wide>(a) * static_cast<Wide>(b)) % static_cast<Wide>(n));
  }
};

// Type aliases for common configurations
using Montgomery32 = Montgomery<unsigned int, unsigned long long>;
using Montgomery64 = Montgomery<unsigned long long, __uint128_t>;
using Montgomery128 = Montgomery<UInt128, UInt256>;
using Montgomery256 = Montgomery<UInt256, UInt512>;
using Montgomery512 = Montgomery<UInt512, UInt1024>;

// BigInt Montgomery - uses double-width for intermediate products
template <SizeT Bits>
using MontgomeryBigInt = Montgomery<UInt<Bits>, UInt<Bits * 2>>;

// =============================================================================
// DynamicMontgomery - Montgomery multiplication for DynamicUInt
// =============================================================================
//
// Unlike the fixed-width Montgomery<T, Wide>, this class handles arbitrary
// precision integers where the bit width is determined at runtime from the
// modulus.

class DynamicMontgomery {
 public:
  DynamicUInt mod;    // The modulus N (must be odd)
  DynamicUInt r;      // R mod N, where R = 2^bits
  DynamicUInt r2;     // R^2 mod N (for converting to Montgomery form)
  DynamicUInt n_inv;  // -N^(-1) mod R (for REDC)
  SizeT bits;         // Number of bits in R (rounded up to limb boundary)

  // PRECONDITION: n is odd and n > 1
  explicit DynamicMontgomery(const DynamicUInt& n)
      : mod(n), bits(computeBits(n)) {
    r = computeRModN(n, bits);
    r2 = (r * r) % n;
    n_inv = computeNegInvModR(n, bits);
  }

  // Convert x to Montgomery form: x' = xR mod N
  auto toMont(const DynamicUInt& x) const -> DynamicUInt {
    return redc(x * r2);
  }

  // Convert x' from Montgomery form: x = x'R^(-1) mod N
  auto fromMont(const DynamicUInt& x_mont) const -> DynamicUInt {
    return redc(x_mont);
  }

  // Multiply two Montgomery-form numbers
  auto mul(const DynamicUInt& a_mont, const DynamicUInt& b_mont) const
      -> DynamicUInt {
    return redc(a_mont * b_mont);
  }

  // Square a Montgomery-form number
  auto sqr(const DynamicUInt& a_mont) const -> DynamicUInt {
    return redc(a_mont * a_mont);
  }

  // Compute base^exp mod N using Montgomery multiplication
  auto pow(const DynamicUInt& base, const DynamicUInt& exp) const
      -> DynamicUInt {
    if (exp.isZero()) {
      return DynamicUInt{1} % mod;
    }

    DynamicUInt result = toMont(DynamicUInt{1});
    DynamicUInt b = toMont(base % mod);
    DynamicUInt e = exp;

    while (!e.isZero()) {
      if (e.bit(0)) {
        result = mul(result, b);
      }
      b = sqr(b);
      e = e >> 1;
    }

    return fromMont(result);
  }

  // Montgomery reduction: t * R^(-1) mod N
  auto redc(const DynamicUInt& t) const -> DynamicUInt {
    // t_lo = t mod R (mask to 'bits' bits)
    DynamicUInt t_lo = maskLowBits(t, bits);

    // m = t_lo * n_inv mod R
    DynamicUInt m = maskLowBits(t_lo * n_inv, bits);

    // (t + m*N) >> bits
    DynamicUInt sum = t + m * mod;
    DynamicUInt result = sum >> bits;

    // Conditional subtraction
    if (result >= mod) {
      result = result - mod;
    }

    return result;
  }

 private:
  // Compute bits for R: round up modulus bits to next limb boundary
  static auto computeBits(const DynamicUInt& n) -> SizeT {
    SizeT n_bits = n.numBits();
    // Round up to next multiple of 64
    return ((n_bits + 63) / 64) * 64;
  }

  // Mask to keep only the low 'bits' bits
  static auto maskLowBits(const DynamicUInt& x, SizeT num_bits) -> DynamicUInt {
    SizeT num_limbs = num_bits / 64;
    if (x.numLimbs() <= num_limbs) {
      return x;
    }
    // Build mask and apply
    DynamicUInt result{0};
    for (SizeT i = 0; i < num_limbs && i < x.numLimbs(); ++i) {
      result = result | (DynamicUInt{x.limb(i)} << (i * 64));
    }
    return result;
  }

  // Compute R mod N where R = 2^bits
  static auto computeRModN(const DynamicUInt& n, SizeT num_bits) -> DynamicUInt {
    // R = 2^bits, compute R mod n by repeated doubling
    DynamicUInt r{1};
    for (SizeT i = 0; i < num_bits; ++i) {
      r = r << 1;
      if (r >= n) {
        r = r - n;
      }
    }
    return r;
  }

  // Compute -N^(-1) mod R using Hensel lifting
  static auto computeNegInvModR(const DynamicUInt& n, SizeT num_bits)
      -> DynamicUInt {
    DynamicUInt x{1};

    // Newton-Hensel: x = x * (2 - n * x) mod R
    // Each iteration doubles correct bits
    SizeT iterations = 0;
    for (SizeT b = 1; b < num_bits; b *= 2) {
      ++iterations;
    }

    for (SizeT i = 0; i < iterations; ++i) {
      DynamicUInt two{2};
      DynamicUInt nx = maskLowBits(n * x, num_bits);
      DynamicUInt diff = two - nx;
      // Handle underflow: if nx > 2, we need (R + 2 - nx) mod R
      if (nx > two) {
        // diff = R - (nx - 2) = R + 2 - nx
        DynamicUInt r_val = DynamicUInt{1} << num_bits;
        diff = r_val - nx + two;
      }
      x = maskLowBits(x * diff, num_bits);
    }

    // Return -x mod R = R - x
    DynamicUInt r_val = DynamicUInt{1} << num_bits;
    return r_val - x;
  }
};

}  // namespace tempura
