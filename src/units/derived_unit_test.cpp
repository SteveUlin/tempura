#include "units/derived_unit.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// DerivedUnit satisfies UnitType concept
// ============================================================================

static_assert(UnitType<DerivedUnit<Metre>>);
static_assert(UnitType<DerivedUnit<Metre, UnitPer<Second>>>);
static_assert(UnitType<DerivedUnit<Kilometre, UnitPer<Hour>>>);
static_assert(kIsDerivedUnit<DerivedUnit<Metre, UnitPer<Second>>>);

// ============================================================================
// DerivedUnit magnitude computation
// ============================================================================

// m/s has magnitude 1
static_assert(DerivedUnit<Metre, UnitPer<Second>>::kMagnitude == 1.0);

// km/h has magnitude 1000/3600 = 5/18
static_assert(DerivedUnit<Kilometre, UnitPer<Hour>>::kMagnitude == 5.0 / 18.0);

// m * m has magnitude 1
static_assert(DerivedUnit<Metre, Metre>::kMagnitude == 1.0);

// km * km has magnitude 1000000
static_assert(DerivedUnit<Kilometre, Kilometre>::kMagnitude == 1000000.0);

// ============================================================================
// DerivedUnit QuantitySpec computation
// ============================================================================

// m/s -> Speed dimension
static_assert(isSame<DerivedUnit<Metre, UnitPer<Second>>::QuantitySpec::Dimension, Velocity>);

// km/h -> Speed dimension
static_assert(isSame<DerivedUnit<Kilometre, UnitPer<Hour>>::QuantitySpec::Dimension, Velocity>);

// m * m -> Area dimension
static_assert(isSame<DerivedUnit<Metre, Metre>::QuantitySpec::Dimension, Area>);

// ============================================================================
// Unit algebra type aliases
// ============================================================================

// Multiply units
using MxM = UnitMultiply<Metre, Metre>;
static_assert(UnitType<MxM>);
static_assert(isSame<MxM::QuantitySpec::Dimension, Area>);

// Divide units
using MperS = UnitDivide<Metre, Second>;
static_assert(UnitType<MperS>);
static_assert(isSame<MperS::QuantitySpec::Dimension, Velocity>);

using KMperH = UnitDivide<Kilometre, Hour>;
static_assert(UnitType<KMperH>);
static_assert(KMperH::kMagnitude == 5.0 / 18.0);

// ============================================================================
// UnitPower
// ============================================================================

// s^2
using S2 = UnitPower<Second, 2>;
static_assert(kIsUnitPower<S2>);
static_assert(S2::exponent == Exp{2});

// m/s^2 (acceleration)
using MperS2 = DerivedUnit<Metre, UnitPer<UnitPower<Second, 2>>>;
static_assert(UnitType<MperS2>);
static_assert(isSame<MperS2::QuantitySpec::Dimension, Acceleration>);

// ============================================================================
// Compatibility between derived and named units
// ============================================================================

// DerivedUnit<m, Per<s>> is compatible with MetrePerSecond
static_assert(kCompatibleUnits<MperS, MetrePerSecond>);
static_assert(kCompatibleUnits<KMperH, MetrePerSecond>);
static_assert(kCompatibleUnits<KMperH, KilometrePerHour>);

// Conversion factor: km/h to m/s = 5/18
// Note: kConversionFactor uses type-level Magnitude which isn't computed for DerivedUnit yet
// So we test the runtime magnitudes directly
static_assert(KMperH::kMagnitude / MperS::kMagnitude == 5.0 / 18.0);

// ============================================================================
// DerivedUnit symbol generation
// ============================================================================

// m/s -> "m/s"
static_assert(DerivedUnit<Metre, UnitPer<Second>>::kSymbol == FixedString{"m/s"});

// km/h -> "km/h"
static_assert(DerivedUnit<Kilometre, UnitPer<Hour>>::kSymbol == FixedString{"km/h"});

// m * m -> "m·m"
static_assert(DerivedUnit<Metre, Metre>::kSymbol == FixedString{"m·m"});

// m/s^2 -> "m/s2"
static_assert(DerivedUnit<Metre, UnitPer<UnitPower<Second, 2>>>::kSymbol == FixedString{"m/s2"});

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "derived unit satisfies UnitType"_test = [] {
    expectTrue(UnitType<DerivedUnit<Metre, UnitPer<Second>>>);
    expectTrue(kIsDerivedUnit<DerivedUnit<Metre>>);
  };

  "derived unit magnitude"_test = [] {
    // m/s = 1
    constexpr auto mps_mag = DerivedUnit<Metre, UnitPer<Second>>::kMagnitude;
    expectEq(mps_mag, 1.0);

    // km/h = 5/18
    constexpr auto kmh_mag = DerivedUnit<Kilometre, UnitPer<Hour>>::kMagnitude;
    expectEq(kmh_mag, 5.0 / 18.0);
  };

  "derived unit quantity spec"_test = [] {
    // m/s is velocity
    expectTrue((isSame<MperS::QuantitySpec::Dimension, Velocity>));

    // m*m is area
    expectTrue((isSame<MxM::QuantitySpec::Dimension, Area>));

    // m/s^2 is acceleration
    expectTrue((isSame<MperS2::QuantitySpec::Dimension, Acceleration>));
  };

  "unit algebra"_test = [] {
    using Speed = UnitDivide<Kilometre, Hour>;
    expectTrue(UnitType<Speed>);
    expectTrue((isSame<Speed::QuantitySpec::Dimension, Velocity>));
  };

  "compatibility with named units"_test = [] {
    expectTrue((kCompatibleUnits<MperS, MetrePerSecond>));
    expectTrue((kCompatibleUnits<KMperH, KilometrePerHour>));
  };

  return TestRegistry::result();
}
