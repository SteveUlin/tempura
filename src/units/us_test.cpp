#include "units/us.h"

#include <sstream>

#include "unit.h"
#include "units/quantity.h"
#include "units/reference.h"

using namespace tempura;
using namespace tempura::us;

// ============================================================================
// Length unit relationships (internal consistency)
// ============================================================================

// 1 foot = 12 inches
static_assert(kConversionFactor<Foot, Inch> == 12.0);

// 1 yard = 3 feet = 36 inches
static_assert(kConversionFactor<Yard, Foot> == 3.0);
static_assert(kConversionFactor<Yard, Inch> == 36.0);

// 1 mile = 5280 feet = 1760 yards
static_assert(kConversionFactor<Mile, Foot> == 5280.0);
static_assert(kConversionFactor<Mile, Yard> == 1760.0);

// ============================================================================
// Length SI conversions (verified at runtime due to FP precision)
// ============================================================================
// 1 inch = 25.4 mm = 0.0254 m
// 1 foot = 0.3048 m
// 1 yard = 0.9144 m
// 1 mile = 1609.344 m

// ============================================================================
// Mass unit relationships
// ============================================================================

// 1 pound = 16 ounces
static_assert(kConversionFactor<Pound, Ounce> == 16.0);

// 1 short ton = 2000 pounds
static_assert(kConversionFactor<ShortTon, Pound> == 2000.0);

// ============================================================================
// Mass SI conversions (verified at runtime due to FP precision)
// ============================================================================
// 1 pound = 0.45359237 kg (exact by definition)
// 1 ounce = 0.028349523125 kg
// 1 short ton = 907.18474 kg

// ============================================================================
// Volume unit relationships
// ============================================================================

// 1 gallon = 4 quarts = 8 pints = 16 cups = 128 fl oz
static_assert(kConversionFactor<Gallon, Quart> == 4.0);
static_assert(kConversionFactor<Gallon, Pint> == 8.0);
static_assert(kConversionFactor<Gallon, Cup> == 16.0);
static_assert(kConversionFactor<Gallon, FluidOunce> == 128.0);

// 1 cup = 8 fl oz = 16 tbsp = 48 tsp
static_assert(kConversionFactor<Cup, FluidOunce> == 8.0);
static_assert(kConversionFactor<Cup, Tablespoon> == 16.0);
static_assert(kConversionFactor<Cup, Teaspoon> == 48.0);

// 1 fl oz = 2 tbsp = 6 tsp
static_assert(kConversionFactor<FluidOunce, Tablespoon> == 2.0);
static_assert(kConversionFactor<FluidOunce, Teaspoon> == 6.0);

// 1 tbsp = 3 tsp
static_assert(kConversionFactor<Tablespoon, Teaspoon> == 3.0);

// ============================================================================
// Volume SI conversions
// ============================================================================

// 1 US gallon = 3.785411784 L = 0.003785411784 m³
constexpr double kGallonM3 = 231.0 * 0.0254 * 0.0254 * 0.0254;  // 231 in³

// ============================================================================
// Speed conversions
// ============================================================================

// mph to m/s: 1 mph = 1609.344/3600 m/s = 0.44704 m/s
constexpr double kMphToMps = 1609.344 / 3600.0;

// ============================================================================
// Cross-system compatibility
// ============================================================================

// US length units are compatible with SI length units
static_assert(kCompatibleUnits<Inch, Metre>);
static_assert(kCompatibleUnits<Foot, Kilometre>);
static_assert(kCompatibleUnits<Mile, Millimetre>);

// US mass units are compatible with SI mass units
static_assert(kCompatibleUnits<Pound, Kilogram>);
static_assert(kCompatibleUnits<Ounce, Gram>);

// US volume units are compatible with SI volume units
static_assert(kCompatibleUnits<Gallon, Litre>);
static_assert(kCompatibleUnits<Cup, Millilitre>);

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "length internal consistency"_test = [] {
    expectEq(kConversionFactor<Foot, Inch>, 12.0);
    expectEq(kConversionFactor<Yard, Foot>, 3.0);
    expectEq(kConversionFactor<Mile, Foot>, 5280.0);
    expectEq(kConversionFactor<Mile, Yard>, 1760.0);
  };

  "length SI conversions"_test = [] {
    expectNear(Inch::kMagnitude, 0.0254, 1e-15);
    expectNear(Foot::kMagnitude, 0.3048, 1e-15);
    expectNear(Yard::kMagnitude, 0.9144, 1e-15);
    expectNear(Mile::kMagnitude, 1609.344, 1e-10);

    // 1 km = 1000/1609.344 miles ≈ 0.621 miles
    expectNear(kConversionFactor<Kilometre, Mile>, 1000.0 / 1609.344, 1e-10);
  };

  "mass internal consistency"_test = [] {
    expectEq(kConversionFactor<Pound, Ounce>, 16.0);
    expectEq(kConversionFactor<ShortTon, Pound>, 2000.0);
  };

  "mass SI conversions"_test = [] {
    expectEq(Pound::kMagnitude, 0.45359237);

    // 1 kg ≈ 2.205 lb
    expectNear(kConversionFactor<Kilogram, Pound>, 1.0 / 0.45359237, 1e-10);
  };

  "volume internal consistency"_test = [] {
    expectEq(kConversionFactor<Gallon, Quart>, 4.0);
    expectEq(kConversionFactor<Gallon, Pint>, 8.0);
    expectEq(kConversionFactor<Gallon, Cup>, 16.0);
    expectEq(kConversionFactor<Gallon, FluidOunce>, 128.0);
    expectEq(kConversionFactor<Cup, Tablespoon>, 16.0);
    expectEq(kConversionFactor<Cup, Teaspoon>, 48.0);
    expectEq(kConversionFactor<Tablespoon, Teaspoon>, 3.0);
  };

  "volume SI conversions"_test = [] {
    // 1 US gallon = 3.785411784 L
    expectNear(Gallon::kMagnitude, kGallonM3, 1e-15);
    expectNear(kConversionFactor<Gallon, Litre>, 3.785411784, 1e-9);

    // 1 cup ≈ 236.588 mL
    expectNear(kConversionFactor<Cup, Millilitre>, 236.5882365, 1e-6);
  };

  "speed conversions"_test = [] {
    // 1 mph ≈ 0.44704 m/s
    expectNear(MilePerHour::kMagnitude, kMphToMps, 1e-10);

    // 60 mph ≈ 26.8224 m/s
    expectNear(kConversionFactor<MilePerHour, MetrePerSecond> * 60.0, 26.8224, 1e-6);
  };

  "quantity arithmetic with US units"_test = [] {
    // 5 feet + 6 inches = 5.5 feet = 66 inches
    auto length1 = Quantity<DefaultRef<Foot>, double>{5.0};
    auto length2 = Quantity<DefaultRef<Inch>, double>{6.0};
    auto total = length1 + length2;
    expectNear(total.template valueIn<DefaultRef<Inch>>(), 66.0, 1e-10);
    expectNear(total.template valueIn<DefaultRef<Foot>>(), 5.5, 1e-10);

    // Convert to metric
    expectNear(total.template valueIn<DefaultRef<Centimetre>>(), 167.64, 1e-10);
  };

  "cooking conversion"_test = [] {
    // Recipe calls for 2 cups, convert to mL
    auto volume = Quantity<DefaultRef<Cup>, double>{2.0};
    expectNear(volume.template valueIn<DefaultRef<Millilitre>>(), 473.176473, 1e-5);

    // 1 tbsp butter ≈ 14.79 mL
    auto tbsp = Quantity<DefaultRef<Tablespoon>, double>{1.0};
    expectNear(tbsp.template valueIn<DefaultRef<Millilitre>>(), 14.7867647825, 1e-6);
  };

  "road trip calculation"_test = [] {
    // Distance: 250 miles, time: 4 hours
    auto distance = Quantity<DefaultRef<Mile>, double>{250.0};
    auto time = Quantity<DefaultRef<Hour>, double>{4.0};
    auto speed = distance / time;

    // Average speed in mph
    expectNear(speed.template valueIn<DefaultRef<MilePerHour>>(), 62.5, 1e-10);

    // Convert to km/h
    expectNear(speed.template valueIn<DefaultRef<KilometrePerHour>>(), 100.584, 1e-3);
  };

  "pretty printing quantities"_test = [] {
    auto length = Quantity<DefaultRef<Foot>, double>{5.5};
    std::ostringstream oss;
    oss << length;
    expectEq(oss.str(), std::string{"5.5 ft"});

    oss.str("");
    auto volume = Quantity<DefaultRef<Cup>, double>{2.0};
    oss << volume;
    expectEq(oss.str(), std::string{"2 cup"});

    oss.str("");
    auto speed = Quantity<DefaultRef<MilePerHour>, double>{65.0};
    oss << speed;
    expectEq(oss.str(), std::string{"65 mph"});
  };

  "pretty printing units"_test = [] {
    std::ostringstream oss;
    oss << Foot{};
    expectEq(oss.str(), std::string{"ft"});

    oss.str("");
    oss << Gallon{};
    expectEq(oss.str(), std::string{"gal"});

    oss.str("");
    oss << FluidOunce{};
    expectEq(oss.str(), std::string{"fl oz"});
  };

  return TestRegistry::result();
}
