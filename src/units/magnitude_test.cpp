#include "units/magnitude.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// PrimePow value computation
// ============================================================================

static_assert(PrimePow<2, 0>::value() == 1.0);
static_assert(PrimePow<2, 1>::value() == 2.0);
static_assert(PrimePow<2, 2>::value() == 4.0);
static_assert(PrimePow<2, 3>::value() == 8.0);
static_assert(PrimePow<2, -1>::value() == 0.5);
static_assert(PrimePow<2, -2>::value() == 0.25);

static_assert(PrimePow<5, 1>::value() == 5.0);
static_assert(PrimePow<5, 2>::value() == 25.0);
static_assert(PrimePow<5, 3>::value() == 125.0);

static_assert(PrimePow<3, 2>::value() == 9.0);
static_assert(PrimePow<7, 1>::value() == 7.0);

// ============================================================================
// PiPrimePow value computation
// ============================================================================

// Note: Pi is approximately 3.14159...
// We can't use == for floating point, so we test in runtime tests

// ============================================================================
// Magnitude value computation
// ============================================================================

// Empty magnitude = 1
static_assert(Magnitude<>::value() == 1.0);
static_assert(MagOne::value() == 1.0);

// Single factor
static_assert(Magnitude<PrimePow<2, 3>>::value() == 8.0);
static_assert(Magnitude<PrimePow<5, 2>>::value() == 25.0);

// Multiple factors (2^3 * 5^3 = 8 * 125 = 1000)
static_assert(Mag1000::value() == 1000.0);
static_assert(Mag100::value() == 100.0);
static_assert(Mag10::value() == 10.0);

// Inverse magnitudes
static_assert(MagThousandth::value() == 0.001);
static_assert(MagHundredth::value() == 0.01);
static_assert(MagTenth::value() == 0.1);

// Time magnitudes
static_assert(Mag60::value() == 60.0);
static_assert(Mag3600::value() == 3600.0);

// ============================================================================
// Magnitude multiplication
// ============================================================================

// 10 * 10 = 100
static_assert(isSame<MagMultiply<Mag10, Mag10>, Mag100>);

// 10 * 100 = 1000
static_assert(isSame<MagMultiply<Mag10, Mag100>, Mag1000>);

// 1000 * 0.001 = 1
static_assert(isSame<MagMultiply<Mag1000, MagThousandth>, MagOne>);

// Verify multiplication value
static_assert(MagMultiply<Mag10, Mag60>::value() == 600.0);

// ============================================================================
// Magnitude inversion
// ============================================================================

static_assert(isSame<MagInverse<Mag1000>, MagThousandth>);
static_assert(isSame<MagInverse<MagThousandth>, Mag1000>);
static_assert(isSame<MagInverse<MagOne>, MagOne>);

// ============================================================================
// Magnitude division
// ============================================================================

// 1000 / 10 = 100
static_assert(isSame<MagDivide<Mag1000, Mag10>, Mag100>);

// 1000 / 100 = 10
static_assert(isSame<MagDivide<Mag1000, Mag100>, Mag10>);

// 1000 / 1000 = 1
static_assert(isSame<MagDivide<Mag1000, Mag1000>, MagOne>);

// km/h magnitude: 1000 / 3600 = 1000 / (4 * 900) = 1000 / (4 * 9 * 100)
// = 2^3 * 5^3 / (2^4 * 3^2 * 5^2) = 5 / (2 * 9) = 5/18
using MagKmPerH = MagDivide<Mag1000, Mag3600>;
// 5/18 ≈ 0.2777...
// Let's verify the factors: 1000 = 2^3 * 5^3, 3600 = 2^4 * 3^2 * 5^2
// 1000/3600 = 2^(3-4) * 3^(0-2) * 5^(3-2) = 2^-1 * 3^-2 * 5^1 = 5/(2*9) = 5/18
static_assert(isSame<MagKmPerH, Magnitude<PrimePow<2, -1>, PrimePow<3, -2>, PrimePow<5, 1>>>);

// ============================================================================
// Rational magnitude detection
// ============================================================================

static_assert(kIsRationalMag<MagOne>);
static_assert(kIsRationalMag<Mag1000>);
static_assert(kIsRationalMag<MagThousandth>);
static_assert(kIsRationalMag<Mag3600>);

// Pi-based magnitudes are not rational
static_assert(!kIsRationalMag<MagPi>);
static_assert(!kIsRationalMag<MagTwoPi>);

// ============================================================================
// MagnitudeType concept
// ============================================================================

static_assert(MagnitudeType<Magnitude<>>);
static_assert(MagnitudeType<Mag1000>);
static_assert(MagnitudeType<MagPi>);
static_assert(!MagnitudeType<int>);
static_assert(!MagnitudeType<PrimePow<2, 1>>);

// ============================================================================
// IsOne check
// ============================================================================

static_assert(MagOne::kIsOne);
static_assert(!Mag10::kIsOne);
static_assert(!MagPi::kIsOne);

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "power values"_test = [] {
    expectEq(PrimePow<2, 3>::value(), 8.0);
    expectEq(PrimePow<5, 3>::value(), 125.0);
    expectEq(PrimePow<2, -1>::value(), 0.5);
  };

  "magnitude values"_test = [] {
    expectEq(Mag1000::value(), 1000.0);
    expectEq(MagThousandth::value(), 0.001);
    expectEq(Mag60::value(), 60.0);
    expectEq(Mag3600::value(), 3600.0);
  };

  "pi values"_test = [] {
    constexpr double kPi = 3.14159265358979323846;
    expectNear(MagPi::value(), kPi, 1e-15);
    expectNear(MagTwoPi::value(), 2.0 * kPi, 1e-15);
    expectNear(MagPiOver2::value(), kPi / 2.0, 1e-15);
  };

  "magnitude multiplication"_test = [] {
    // 10 * 10 = 100
    expectEq(MagMultiply<Mag10, Mag10>::value(), 100.0);

    // 1000 * 0.001 = 1
    expectEq(MagMultiply<Mag1000, MagThousandth>::value(), 1.0);
  };

  "magnitude division"_test = [] {
    // 1000 / 3600 = 5/18
    expectNear(MagKmPerH::value(), 5.0 / 18.0, 1e-15);

    // km/h to m/s conversion factor
    expectNear(MagKmPerH::value(), 1000.0 / 3600.0, 1e-15);
  };

  "pi/180 for degree to radian"_test = [] {
    constexpr double kPi = 3.14159265358979323846;
    // π/180 is the conversion factor from degrees to radians
    expectNear(MagPiOver180::value(), kPi / 180.0, 1e-15);
  };

  "extreme magnitudes don't overflow"_test = [] {
    // 10^24 would overflow long long, but prime factorization handles it
    using MagYotta = Magnitude<PrimePow<2, 24>, PrimePow<5, 24>>;  // 10^24
    using MagYocto = Magnitude<PrimePow<2, -24>, PrimePow<5, -24>>;  // 10^-24

    // These evaluate correctly
    expectNear(MagYotta::value(), 1e24, 1e10);  // Large tolerance for large numbers
    expectNear(MagYocto::value(), 1e-24, 1e-38);

    // Multiplication gives exactly 1
    using Product = MagMultiply<MagYotta, MagYocto>;
    static_assert(Product::kIsOne);
  };

  return TestRegistry::result();
}
