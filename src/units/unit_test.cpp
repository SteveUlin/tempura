#include "units/unit_type.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// UnitType concept
// ============================================================================

static_assert(UnitType<Metre>);
static_assert(UnitType<Kilometre>);
static_assert(UnitType<Second>);
static_assert(UnitType<Hour>);
static_assert(UnitType<Newton>);
static_assert(!UnitType<int>);
static_assert(!UnitType<QtyLength>);  // QuantitySpec, not Unit

// ============================================================================
// Magnitude values (now doubles)
// ============================================================================

static_assert(Metre::kMagnitude == 1.0);
static_assert(Kilometre::kMagnitude == 1000.0);
static_assert(Centimetre::kMagnitude == 0.01);
static_assert(Millimetre::kMagnitude == 0.001);

static_assert(Second::kMagnitude == 1.0);
static_assert(Minute::kMagnitude == 60.0);
static_assert(Hour::kMagnitude == 3600.0);

static_assert(Kilogram::kMagnitude == 1.0);
static_assert(Gram::kMagnitude == 0.001);

// ============================================================================
// Unit compatibility
// ============================================================================

// Same dimension = compatible
static_assert(kCompatibleUnits<Metre, Kilometre>);
static_assert(kCompatibleUnits<Metre, Centimetre>);
static_assert(kCompatibleUnits<Metre, Millimetre>);
static_assert(kCompatibleUnits<Second, Hour>);
static_assert(kCompatibleUnits<Second, Minute>);
static_assert(kCompatibleUnits<Kilogram, Gram>);

// Different dimension = not compatible
static_assert(!kCompatibleUnits<Metre, Second>);
static_assert(!kCompatibleUnits<Metre, Kilogram>);
static_assert(!kCompatibleUnits<Second, Newton>);
static_assert(!kCompatibleUnits<MetrePerSecond, Metre>);

// ============================================================================
// Conversion factors (now doubles)
// ============================================================================

// km -> m = 1000 (1 km = 1000 m)
static_assert(kConversionFactor<Kilometre, Metre> == 1000.0);

// m -> km = 1/1000 (1 m = 0.001 km)
static_assert(kConversionFactor<Metre, Kilometre> == 0.001);

// cm -> m = 1/100
static_assert(kConversionFactor<Centimetre, Metre> == 0.01);

// m -> cm = 100
static_assert(kConversionFactor<Metre, Centimetre> == 100.0);

// hour -> second = 3600
static_assert(kConversionFactor<Hour, Second> == 3600.0);

// minute -> second = 60
static_assert(kConversionFactor<Minute, Second> == 60.0);

// km/h -> m/s = 1000/3600 = 5/18 ≈ 0.277...
// 5.0/18.0 = 0.27777... - use approximate comparison
constexpr double kKmhToMps = 5.0 / 18.0;
static_assert(kConversionFactor<KilometrePerHour, MetrePerSecond> == kKmhToMps);

// m/s -> km/h = 3600/1000 = 18/5 = 3.6
static_assert(kConversionFactor<MetrePerSecond, KilometrePerHour> == 3.6);

// ============================================================================
// Derived unit magnitudes
// ============================================================================

static_assert(Newton::kMagnitude == 1.0);
static_assert(Kilonewton::kMagnitude == 1000.0);

static_assert(Joule::kMagnitude == 1.0);
static_assert(Kilojoule::kMagnitude == 1000.0);

static_assert(Watt::kMagnitude == 1.0);
static_assert(Kilowatt::kMagnitude == 1000.0);

static_assert(Pascal::kMagnitude == 1.0);
static_assert(Bar::kMagnitude == 100000.0);

// ============================================================================
// Dimensionless units
// ============================================================================

static_assert(UnitType<One>);
static_assert(UnitType<Percent>);
static_assert(One::kMagnitude == 1.0);
static_assert(Percent::kMagnitude == 0.01);
static_assert(kConversionFactor<Percent, One> == 0.01);

// ============================================================================
// QuantitySpec association
// ============================================================================

static_assert(isSame<Metre::QuantitySpec, QtyLength>);
static_assert(isSame<Second::QuantitySpec, QtyTime>);
static_assert(isSame<Kilogram::QuantitySpec, QtyMass>);
static_assert(isSame<Newton::QuantitySpec, QtyForce>);
static_assert(isSame<MetrePerSecond::QuantitySpec, QtySpeed>);

// ============================================================================
// SI Prefix templates
// ============================================================================

// Kilo<Metre> is equivalent to the manually-defined Kilometre
static_assert(UnitType<Kilo<Metre>>);
static_assert(Kilo<Metre>::kMagnitude == 1000.0);
static_assert(kCompatibleUnits<Kilo<Metre>, Kilometre>);
static_assert(kConversionFactor<Kilo<Metre>, Kilometre> == 1.0);

// Works with any base unit
static_assert(UnitType<Kilo<Second>>);
static_assert(Kilo<Second>::kMagnitude == 1000.0);

static_assert(UnitType<Mega<Watt>>);
static_assert(Mega<Watt>::kMagnitude == 1000000.0);

static_assert(UnitType<Giga<Hertz>>);
static_assert(Giga<Hertz>::kMagnitude == 1000000000.0);

// Small prefixes
static_assert(UnitType<Milli<Metre>>);
static_assert(Milli<Metre>::kMagnitude == 0.001);

static_assert(UnitType<Micro<Second>>);
static_assert(Micro<Second>::kMagnitude == 0.000001);

static_assert(UnitType<Nano<Second>>);
static_assert(Nano<Second>::kMagnitude == 0.000000001);

// Prefix symbol concatenation
static_assert(Kilo<Metre>::kSymbol == FixedString{"km"});
static_assert(Milli<Second>::kSymbol == FixedString{"ms"});
static_assert(Mega<Watt>::kSymbol == FixedString{"MW"});
static_assert(Nano<Metre>::kSymbol == FixedString{"nm"});

// Conversion between prefixed and manual units
static_assert(kConversionFactor<Kilo<Metre>, Metre> == 1000.0);
static_assert(kConversionFactor<Milli<Second>, Second> == 0.001);

// ============================================================================
// Angle units with symbolic Pi
// ============================================================================

static_assert(UnitType<Radian>);
static_assert(UnitType<Degree>);
static_assert(UnitType<Turn>);
static_assert(Radian::kMagnitude == 1.0);

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "UnitType concept"_test = [] {
    expectTrue(UnitType<Metre>);
    expectTrue(UnitType<Kilometre>);
    expectTrue(UnitType<Newton>);
    expectFalse(UnitType<int>);
  };

  "unit compatibility"_test = [] {
    expectTrue((kCompatibleUnits<Metre, Kilometre>));
    expectTrue((kCompatibleUnits<Second, Hour>));
    expectFalse((kCompatibleUnits<Metre, Second>));
  };

  "conversion factors"_test = [] {
    // km to m
    expectEq(kConversionFactor<Kilometre, Metre>, 1000.0);

    // m to cm
    expectEq(kConversionFactor<Metre, Centimetre>, 100.0);

    // km/h to m/s (5/18)
    expectNear(kConversionFactor<KilometrePerHour, MetrePerSecond>, 5.0 / 18.0, 1e-15);
  };

  "SI prefix templates"_test = [] {
    // Kilo<Metre> works like Kilometre
    expectTrue(UnitType<Kilo<Metre>>);
    expectEq(Kilo<Metre>::kMagnitude, 1000.0);

    // Milli<Second> works
    expectTrue(UnitType<Milli<Second>>);
    expectEq(Milli<Second>::kMagnitude, 0.001);

    // Symbol concatenation works
    expectTrue((Kilo<Metre>::kSymbol == FixedString{"km"}));
    expectTrue((Nano<Metre>::kSymbol == FixedString{"nm"}));
  };

  "angle units"_test = [] {
    // Check that degree converts to radians correctly
    // 1 degree = π/180 radians
    constexpr double kPi = 3.14159265358979323846;
    expectNear(Degree::kMagnitude, kPi / 180.0, 1e-15);

    // 1 turn = 2π radians
    expectNear(Turn::kMagnitude, 2.0 * kPi, 1e-14);

    // Conversion: 180 degrees = π radians
    expectNear(kConversionFactor<Degree, Radian> * 180.0, kPi, 1e-14);
  };

  return TestRegistry::result();
}
