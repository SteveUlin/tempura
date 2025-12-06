#pragma once

#include <cmath>

#include "math/ratio.h"
#include "meta/type_list.h"
#include "meta/utility.h"

// Magnitude - Compile-time symbolic representation of scale factors
//
// Instead of storing 1000 as a single number (overflow risk), we store
// it as 2³ × 5³ (prime factorization). This enables:
//
// 1. No overflow: 10^24 = 2^24 × 5^24 (stored as exponents, not value)
// 2. Exact simplification: (2³ × 5³) / (2² × 5²) = 2¹ × 5¹
// 3. Symbolic constants: π stays symbolic until final evaluation
// 4. Detect lossless conversions at compile time
//
// Examples:
//   using Kilo = Magnitude<PrimePow<2, 3>, PrimePow<5, 3>>;  // 1000
//   using Milli = Magnitude<PrimePow<2, -3>, PrimePow<5, -3>>;  // 0.001
//   using Pi = Magnitude<PiPow<1>>;  // π
//   using TwoPi = Magnitude<PrimePow<2, 1>, PiPow<1>>;  // 2π

namespace tempura {

// ============================================================================
// MagExp - Rational exponent for magnitude factors (n/d)
// ============================================================================

using MagExp = Ratio<int>;

// ============================================================================
// PrimePow - Prime number raised to rational exponent
// ============================================================================

// Constexpr primality check
constexpr auto isPrime(int n) -> bool {
  if (n < 2) return false;
  if (n == 2) return true;
  if (n % 2 == 0) return false;
  for (int i = 3; i * i <= n; i += 2) {
    if (n % i == 0) return false;
  }
  return true;
}

// Check if a number is valid for use as a magnitude base.
// Small numbers must be prime. Large "magic numbers" (>1000000) from physical
// definitions (like 45359237 for the pound) are allowed as single factors
// even if not prime, since their factorization is complex and doesn't
// simplify conversions.
constexpr auto isValidMagnitudeBase(int n) -> bool {
  if (n > 1000000) return true;  // Allow large magic constants
  return isPrime(n);
}

// PrimePow<P, N, D> represents P^(N/D)
// P must be prime (or a large magic constant from physical definitions)
template <int Prime, int Num, int Den = 1>
struct PrimePow {
  static_assert(isValidMagnitudeBase(Prime),
                "PrimePow base must be a prime number (or a large physical constant)");

  static constexpr int kBase = Prime;
  static constexpr MagExp kExponent{Num, Den};

  // Evaluate as double
  static constexpr auto value() -> double {
    if constexpr (kExponent.num == 0) {
      return 1.0;
    } else if constexpr (kExponent.den == 1) {
      // Integer exponent - use repeated multiplication for exactness
      double result = 1.0;
      int exp = kExponent.num > 0 ? kExponent.num : -kExponent.num;
      for (int i = 0; i < exp; ++i) {
        result *= static_cast<double>(Prime);
      }
      return kExponent.num > 0 ? result : 1.0 / result;
    } else {
      // Fractional exponent - use constexpr std::pow (C++26)
      return std::pow(static_cast<double>(Prime),
                      static_cast<double>(kExponent.num) / static_cast<double>(kExponent.den));
    }
  }
};

// ============================================================================
// Symbolic constants
// ============================================================================

// PiPow<N, D> represents π^(N/D)
template <int Num, int Den = 1>
struct PiPow {
  static constexpr int kBase = -1;  // Sentinel for π
  static constexpr MagExp kExponent{Num, Den};

  static constexpr auto value() -> double {
    // π ≈ 3.14159265358979323846
    constexpr double kPi = 3.14159265358979323846;
    if constexpr (kExponent.num == 0) {
      return 1.0;
    } else if constexpr (kExponent.den == 1 && kExponent.num > 0) {
      double result = 1.0;
      for (int i = 0; i < kExponent.num; ++i) {
        result *= kPi;
      }
      return result;
    } else if constexpr (kExponent.den == 1 && kExponent.num < 0) {
      double result = 1.0;
      for (int i = 0; i < -kExponent.num; ++i) {
        result *= kPi;
      }
      return 1.0 / result;
    } else {
      // Fractional π exponent - use constexpr std::pow (C++26)
      return std::pow(kPi,
                      static_cast<double>(kExponent.num) / static_cast<double>(kExponent.den));
    }
  }
};

// ============================================================================
// Type traits for factors
// ============================================================================

template <typename T>
constexpr bool kIsPrimePow = false;

template <int P, int N, int D>
constexpr bool kIsPrimePow<PrimePow<P, N, D>> = true;

template <typename T>
constexpr bool kIsPiPow = false;

template <int N, int D>
constexpr bool kIsPiPow<PiPow<N, D>> = true;

template <typename T>
constexpr bool kIsMagFactor = kIsPrimePow<T> || kIsPiPow<T>;

// Get the base of a factor
template <typename T>
constexpr int kMagFactorBase = T::kBase;

// Get the exponent of a factor
template <typename T>
constexpr MagExp kMagFactorExp = T::kExponent;

// ============================================================================
// Magnitude - Product of prime powers and symbolic constants
// ============================================================================

template <typename... Factors>
struct Magnitude {
  // Evaluate the magnitude as a double
  static constexpr auto value() -> double {
    if constexpr (sizeof...(Factors) == 0) {
      return 1.0;
    } else {
      return (Factors::value() * ...);
    }
  }

  // Check if magnitude equals 1 (identity)
  static constexpr bool kIsOne = sizeof...(Factors) == 0 ||
                                  ((kMagFactorExp<Factors>.num == 0) && ...);
};

// ============================================================================
// MagnitudeType concept
// ============================================================================

template <typename T>
constexpr bool kIsMagnitude = false;

template <typename... Factors>
constexpr bool kIsMagnitude<Magnitude<Factors...>> = true;

template <typename T>
concept MagnitudeType = kIsMagnitude<T>;

// ============================================================================
// Common magnitudes
// ============================================================================

using MagOne = Magnitude<>;  // 1

// ============================================================================
// Helper: Make a PrimePow with combined exponent
// ============================================================================

template <int Base, MagExp E>
struct MakePrimePowImpl {
  using Type = PrimePow<Base, E.num, E.den>;
};

template <MagExp E>
struct MakePrimePowImpl<-1, E> {  // -1 is sentinel for Pi
  using Type = PiPow<E.num, E.den>;
};

template <int Base, MagExp E>
using MakePrimePow = typename MakePrimePowImpl<Base, E>::Type;

// ============================================================================
// Magnitude multiplication
// ============================================================================

namespace detail {

// Merge two sorted factor lists, combining same-base factors
template <typename L1, typename L2>
struct MergeMagnitudes;

// Base cases
template <typename... Fs>
struct MergeMagnitudes<TypeList<Fs...>, TypeList<>> {
  using Type = Magnitude<Fs...>;
};

template <typename... Fs>
struct MergeMagnitudes<TypeList<>, TypeList<Fs...>> {
  using Type = Magnitude<Fs...>;
};

template <>
struct MergeMagnitudes<TypeList<>, TypeList<>> {
  using Type = Magnitude<>;
};

// Recursive case: compare bases
template <typename F1, typename... Rest1, typename F2, typename... Rest2>
struct MergeMagnitudes<TypeList<F1, Rest1...>, TypeList<F2, Rest2...>> {
  static constexpr int kBase1 = kMagFactorBase<F1>;
  static constexpr int kBase2 = kMagFactorBase<F2>;

  // Same base: combine exponents
  struct CombinedCase {
    static constexpr MagExp kNewExp = kMagFactorExp<F1> + kMagFactorExp<F2>;
    // If exponent is 0, skip this factor
    using RestMerge = typename MergeMagnitudes<TypeList<Rest1...>, TypeList<Rest2...>>::Type;

    template <typename Merged, bool KeepFactor>
    struct PrependIf;

    template <typename... Ms>
    struct PrependIf<Magnitude<Ms...>, true> {
      using Type = Magnitude<MakePrimePow<kBase1, kNewExp>, Ms...>;
    };

    template <typename... Ms>
    struct PrependIf<Magnitude<Ms...>, false> {
      using Type = Magnitude<Ms...>;
    };

    using Type = typename PrependIf<RestMerge, (kNewExp.num != 0)>::Type;
  };

  // F1 base smaller: take F1 first
  struct TakeF1Case {
    using RestMerge = typename MergeMagnitudes<TypeList<Rest1...>, TypeList<F2, Rest2...>>::Type;

    template <typename... Ms>
    struct Prepend;

    template <typename... Ms>
    struct Prepend<Magnitude<Ms...>> {
      using Type = Magnitude<F1, Ms...>;
    };

    using Type = typename Prepend<RestMerge>::Type;
  };

  // F2 base smaller: take F2 first
  struct TakeF2Case {
    using RestMerge = typename MergeMagnitudes<TypeList<F1, Rest1...>, TypeList<Rest2...>>::Type;

    template <typename... Ms>
    struct Prepend;

    template <typename... Ms>
    struct Prepend<Magnitude<Ms...>> {
      using Type = Magnitude<F2, Ms...>;
    };

    using Type = typename Prepend<RestMerge>::Type;
  };

  using Type = typename Conditional<
      (kBase1 == kBase2),
      CombinedCase,
      typename Conditional<(kBase1 < kBase2), TakeF1Case, TakeF2Case>::Type>::Type::Type;
};

}  // namespace detail

// MagMultiply<M1, M2> = M1 * M2
template <MagnitudeType M1, MagnitudeType M2>
struct MagMultiplyImpl;

template <typename... F1s, typename... F2s>
struct MagMultiplyImpl<Magnitude<F1s...>, Magnitude<F2s...>> {
  using Type = typename detail::MergeMagnitudes<TypeList<F1s...>, TypeList<F2s...>>::Type;
};

template <MagnitudeType M1, MagnitudeType M2>
using MagMultiply = typename MagMultiplyImpl<M1, M2>::Type;

// ============================================================================
// Magnitude inversion
// ============================================================================

template <typename Factor>
struct NegateFactor;

template <int P, int N, int D>
struct NegateFactor<PrimePow<P, N, D>> {
  using Type = PrimePow<P, -N, D>;
};

template <int N, int D>
struct NegateFactor<PiPow<N, D>> {
  using Type = PiPow<-N, D>;
};

template <MagnitudeType M>
struct MagInverseImpl;

template <typename... Factors>
struct MagInverseImpl<Magnitude<Factors...>> {
  using Type = Magnitude<typename NegateFactor<Factors>::Type...>;
};

template <MagnitudeType M>
using MagInverse = typename MagInverseImpl<M>::Type;

// ============================================================================
// Magnitude division
// ============================================================================

template <MagnitudeType M1, MagnitudeType M2>
using MagDivide = MagMultiply<M1, MagInverse<M2>>;

// ============================================================================
// Magnitude power: Mag^(N/D)
// ============================================================================

// Scale a factor's exponent by N/D
template <typename Factor, int N, int D>
struct ScaleFactor;

template <int P, int E_Num, int E_Den, int N, int D>
struct ScaleFactor<PrimePow<P, E_Num, E_Den>, N, D> {
  // (P^(E_Num/E_Den))^(N/D) = P^(E_Num*N / E_Den*D)
  static constexpr long long new_num = static_cast<long long>(E_Num) * N;
  static constexpr long long new_den = static_cast<long long>(E_Den) * D;
  // Simplify by GCD
  static constexpr auto gcd(long long a, long long b) -> long long {
    a = a < 0 ? -a : a;
    b = b < 0 ? -b : b;
    while (b != 0) {
      auto t = b;
      b = a % b;
      a = t;
    }
    return a;
  }
  static constexpr long long g = gcd(new_num, new_den);
  static constexpr int final_num = static_cast<int>(new_num / g);
  static constexpr int final_den = static_cast<int>(new_den / g);
  // Handle zero exponent case
  using Type = typename Conditional<
      final_num == 0,
      void,  // Will be filtered out
      PrimePow<P, final_num, final_den>>::Type;
  static constexpr bool kIsZero = (final_num == 0);
};

template <int E_Num, int E_Den, int N, int D>
struct ScaleFactor<PiPow<E_Num, E_Den>, N, D> {
  static constexpr long long new_num = static_cast<long long>(E_Num) * N;
  static constexpr long long new_den = static_cast<long long>(E_Den) * D;
  static constexpr auto gcd(long long a, long long b) -> long long {
    a = a < 0 ? -a : a;
    b = b < 0 ? -b : b;
    while (b != 0) {
      auto t = b;
      b = a % b;
      a = t;
    }
    return a;
  }
  static constexpr long long g = gcd(new_num, new_den);
  static constexpr int final_num = static_cast<int>(new_num / g);
  static constexpr int final_den = static_cast<int>(new_den / g);
  using Type = typename Conditional<
      final_num == 0,
      void,
      PiPow<final_num, final_den>>::Type;
  static constexpr bool kIsZero = (final_num == 0);
};

// Filter out void types (zero exponents)
template <typename... Factors>
struct FilterZeroFactors;

template <>
struct FilterZeroFactors<> {
  using Type = Magnitude<>;
};

template <typename First, typename... Rest>
struct FilterZeroFactors<First, Rest...> {
  using RestFiltered = typename FilterZeroFactors<Rest...>::Type;
  template <typename Mag>
  struct Prepend;
  template <typename... Fs>
  struct Prepend<Magnitude<Fs...>> {
    using Type = Magnitude<First, Fs...>;
  };
  using Type = typename Prepend<RestFiltered>::Type;
};

template <typename... Rest>
struct FilterZeroFactors<void, Rest...> {
  using Type = typename FilterZeroFactors<Rest...>::Type;
};

template <MagnitudeType M, int N, int D = 1>
struct MagPowImpl;

template <typename... Factors, int N, int D>
struct MagPowImpl<Magnitude<Factors...>, N, D> {
  using Type = typename FilterZeroFactors<
      typename ScaleFactor<Factors, N, D>::Type...>::Type;
};

template <int N, int D>
struct MagPowImpl<Magnitude<>, N, D> {
  using Type = Magnitude<>;
};

template <MagnitudeType M, int N, int D = 1>
using MagPow = typename MagPowImpl<M, N, D>::Type;

// ============================================================================
// Magnitude equality
// ============================================================================

template <MagnitudeType M1, MagnitudeType M2>
constexpr bool kMagEqual = isSame<M1, M2>;

// ============================================================================
// Magnitude comparison (for conversion factors)
// ============================================================================

// Get conversion factor M1/M2 as a double
template <MagnitudeType M1, MagnitudeType M2>
constexpr double kMagRatio = MagDivide<M1, M2>::value();

// ============================================================================
// Check if magnitude is rational (no Pi or fractional exponents)
// ============================================================================

template <typename Factor>
constexpr bool kIsRationalFactor = kIsPrimePow<Factor> && (kMagFactorExp<Factor>.den == 1);

template <MagnitudeType M>
struct IsRationalMagnitudeImpl;

template <typename... Factors>
struct IsRationalMagnitudeImpl<Magnitude<Factors...>> {
  static constexpr bool value = (kIsRationalFactor<Factors> && ...);
};

template <>
struct IsRationalMagnitudeImpl<Magnitude<>> {
  static constexpr bool value = true;
};

template <MagnitudeType M>
constexpr bool kIsRationalMag = IsRationalMagnitudeImpl<M>::value;

// ============================================================================
// Convenience: Integer magnitudes from prime factorization
// ============================================================================

// Common powers of 10
using Mag10 = Magnitude<PrimePow<2, 1>, PrimePow<5, 1>>;  // 10
using Mag100 = Magnitude<PrimePow<2, 2>, PrimePow<5, 2>>;  // 100
using Mag1000 = Magnitude<PrimePow<2, 3>, PrimePow<5, 3>>;  // 1000
using MagMillion = Magnitude<PrimePow<2, 6>, PrimePow<5, 6>>;  // 10^6
using MagBillion = Magnitude<PrimePow<2, 9>, PrimePow<5, 9>>;  // 10^9

// Inverse powers
using MagTenth = Magnitude<PrimePow<2, -1>, PrimePow<5, -1>>;  // 0.1
using MagHundredth = Magnitude<PrimePow<2, -2>, PrimePow<5, -2>>;  // 0.01
using MagThousandth = Magnitude<PrimePow<2, -3>, PrimePow<5, -3>>;  // 0.001

// Time-related
using Mag60 = Magnitude<PrimePow<2, 2>, PrimePow<3, 1>, PrimePow<5, 1>>;  // 60
using Mag3600 = Magnitude<PrimePow<2, 4>, PrimePow<3, 2>, PrimePow<5, 2>>;  // 3600

// Pi-based
using MagPi = Magnitude<PiPow<1>>;  // π
using MagTwoPi = Magnitude<PrimePow<2, 1>, PiPow<1>>;  // 2π
using MagPiOver2 = Magnitude<PrimePow<2, -1>, PiPow<1>>;  // π/2
using MagPiOver180 = MagDivide<MagPi, Magnitude<PrimePow<2, 2>, PrimePow<3, 2>, PrimePow<5, 1>>>;  // π/180

}  // namespace tempura
