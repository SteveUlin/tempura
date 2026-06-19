#pragma once

#include <bit>
#include <cassert>
#include <cstdint>

namespace tempura {

// IEEE-754 binary64 bit surgery, the foundation for argument reduction (extract the
// exponent/mantissa) and reconstruction (build 2^k). All constexpr via std::bit_cast —
// never a union (which is UB in C++): bit_cast copies bytes into a fresh object, so there
// is no aliasing to violate. A double is (-1)ˢ·(1+m)·2^(e−1023): 1 sign + 11 exponent +
// 52 mantissa bits.

inline constexpr std::uint64_t kSignMask = 0x8000000000000000ULL;
inline constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
inline constexpr std::uint64_t kMantMask = 0x000FFFFFFFFFFFFFULL;
inline constexpr int kExpBias = 1023;
inline constexpr int kExpShift = 52;
inline constexpr std::uint64_t kExpFieldMax = 0x7FF;

constexpr auto rawBits(double x) -> std::uint64_t { return std::bit_cast<std::uint64_t>(x); }

// Unbiased exponent of a NORMAL double. PRECONDITION: x is normal (not 0/subnormal/inf/nan)
// — the caller on the fast path guarantees it; use frexp for the general decomposition.
constexpr auto exponentOf(double x) -> int {
  return static_cast<int>((rawBits(x) >> kExpShift) & kExpFieldMax) - kExpBias;
}
// The 52 stored mantissa bits (the implicit leading 1 is NOT included).
constexpr auto mantissaOf(double x) -> std::uint64_t { return rawBits(x) & kMantMask; }

constexpr auto isSubnormal(double x) -> bool {
  const auto b = rawBits(x);
  return (b & kExpMask) == 0 && (b & kMantMask) != 0;
}

// k indexes a representable 2^k as a NORMAL double (the range ldexpFast handles directly).
constexpr auto isNormalExponent(int k) -> bool { return k >= -1022 && k <= 1023; }

// Build m·2^k by writing the exponent field directly — branch-free and exact, but ONLY for
// k in the normal range (asserted, fail-loud per the codebase rule; the general, subnormal-
// and overflow-aware form is scalbn). This is the hot path a kernel uses after bounding k.
constexpr auto ldexpFast(double m, int k) -> double {
  assert(isNormalExponent(k) && "ldexpFast: exponent out of normal range — use scalbn");
  return m * std::bit_cast<double>(static_cast<std::uint64_t>(kExpBias + k) << kExpShift);
}
constexpr auto pow2(int k) -> double { return ldexpFast(1.0, k); }

// x·2^n for any n, subnormal- and overflow-correct (musl's split-scale: each multiply stays
// representable so the subnormal range doesn't double-round).
constexpr auto scalbn(double x, int n) -> double {
  double y = x;
  if (n > 1023) {
    y *= 0x1p1023;
    n -= 1023;
    if (n > 1023) {
      y *= 0x1p1023;
      n -= 1023;
      if (n > 1023) n = 1023;
    }
  } else if (n < -1022) {
    y *= 0x1p-1022 * 0x1p53;  // push gradually into subnormals, avoiding double rounding
    n += 1022 - 53;
    if (n < -1022) {
      y *= 0x1p-1022 * 0x1p53;
      n += 1022 - 53;
      if (n < -1022) n = -1022;
    }
  }
  return y * std::bit_cast<double>(static_cast<std::uint64_t>(kExpBias + n) << kExpShift);
}

// Decompose x = m·2^e with m ∈ [0.5, 1) (or x itself for 0/inf/nan). Mirrors std::frexp.
constexpr auto frexp(double x, int& e) -> double {
  auto bits = rawBits(x);
  const int field = static_cast<int>((bits >> kExpShift) & kExpFieldMax);
  if (field == 0) {            // zero or subnormal
    if (x != 0.0) {
      const double r = frexp(x * 0x1p64, e);  // normalize, then correct the exponent
      e -= 64;
      return r;
    }
    e = 0;
    return x;
  }
  if (field == static_cast<int>(kExpFieldMax)) return x;  // inf / nan: e is unspecified
  e = field - (kExpBias - 1);                  // bias to land the mantissa in [0.5, 1)
  bits = (bits & (kSignMask | kMantMask)) | (static_cast<std::uint64_t>(kExpBias - 1) << kExpShift);
  return std::bit_cast<double>(bits);
}

}  // namespace tempura
