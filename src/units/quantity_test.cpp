#include "units/quantity.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// QuantityType concept
// ============================================================================

static_assert(QuantityType<Quantity<DefaultRef<Metre>, double>>);
static_assert(QuantityType<Quantity<DefaultRef<Second>, int>>);
static_assert(!QuantityType<int>);
static_assert(!QuantityType<Metre>);
static_assert(!QuantityType<DefaultRef<Metre>>);

// ============================================================================
// Basic construction
// ============================================================================

static_assert(Quantity<DefaultRef<Metre>, double>{5.0}.count() == 5.0);
static_assert(Quantity<DefaultRef<Second>, int>{10}.count() == 10);

// ============================================================================
// Value * Unit syntax
// ============================================================================

static_assert((5.0 * Metre{}).count() == 5.0);
static_assert((10 * Second{}).count() == 10);

// ============================================================================
// Same-unit arithmetic
// ============================================================================

static_assert((Quantity<DefaultRef<Metre>, double>{3.0} +
               Quantity<DefaultRef<Metre>, double>{2.0})
                  .count() == 5.0);

static_assert((Quantity<DefaultRef<Metre>, double>{5.0} -
               Quantity<DefaultRef<Metre>, double>{2.0})
                  .count() == 3.0);

static_assert((-Quantity<DefaultRef<Metre>, double>{3.0}).count() == -3.0);

// ============================================================================
// Scalar multiplication/division
// ============================================================================

static_assert((Quantity<DefaultRef<Metre>, double>{3.0} * 2.0).count() == 6.0);
static_assert((2.0 * Quantity<DefaultRef<Metre>, double>{3.0}).count() == 6.0);
static_assert((Quantity<DefaultRef<Metre>, double>{6.0} / 2.0).count() == 3.0);

// ============================================================================
// CommonRef selection
// ============================================================================

// CommonRef picks smaller magnitude (more precise)
static_assert(isSame<CommonRef<DefaultRef<Metre>, DefaultRef<Kilometre>>, DefaultRef<Metre>>);
static_assert(isSame<CommonRef<DefaultRef<Kilometre>, DefaultRef<Metre>>, DefaultRef<Metre>>);
static_assert(isSame<CommonRef<DefaultRef<Second>, DefaultRef<Hour>>, DefaultRef<Second>>);

// ============================================================================
// Cross-reference addition
// ============================================================================

// 1 km + 500 m = 1500 m (result in metres, the smaller unit)
static_assert((Quantity<DefaultRef<Kilometre>, double>{1.0} +
               Quantity<DefaultRef<Metre>, double>{500.0})
                  .count() == 1500.0);

// 500 m + 1 km = 1500 m
static_assert((Quantity<DefaultRef<Metre>, double>{500.0} +
               Quantity<DefaultRef<Kilometre>, double>{1.0})
                  .count() == 1500.0);

// 2 km - 500 m = 1500 m
static_assert((Quantity<DefaultRef<Kilometre>, double>{2.0} -
               Quantity<DefaultRef<Metre>, double>{500.0})
                  .count() == 1500.0);

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "quantity construction"_test = [] {
    auto length = Quantity<DefaultRef<Metre>, double>{5.0};
    expectEq(length.count(), 5.0);

    auto time = 10.0 * Second{};
    expectEq(time.count(), 10.0);
  };

  "same-unit arithmetic"_test = [] {
    auto a = 5.0 * Metre{};
    auto b = 3.0 * Metre{};

    expectEq((a + b).count(), 8.0);
    expectEq((a - b).count(), 2.0);
    expectEq((-a).count(), -5.0);
  };

  "scalar multiplication"_test = [] {
    auto length = 3.0 * Metre{};

    expectEq((length * 2.0).count(), 6.0);
    expectEq((2.0 * length).count(), 6.0);
    expectEq((length / 3.0).count(), 1.0);
  };

  "quantity multiplication - dimension changes"_test = [] {
    auto length = 2.0 * Metre{};
    auto width = 3.0 * Metre{};
    auto area = length * width;

    expectEq(area.count(), 6.0);
    // Area dimension
    expectTrue((isSame<decltype(area)::Dimension, Area>));
  };

  "quantity division - dimension changes"_test = [] {
    auto distance = 100.0 * Metre{};
    auto time = 10.0 * Second{};
    auto speed = distance / time;

    expectEq(speed.count(), 10.0);
    // Velocity dimension
    expectTrue((isSame<decltype(speed)::Dimension, Velocity>));
  };

  "conversion with in()"_test = [] {
    auto km = 1.0 * Kilometre{};
    auto m = km.in<DefaultRef<Metre>>();

    expectEq(m.count(), 1000.0);
  };

  "conversion from km to m"_test = [] {
    auto distance_km = 5.0 * Kilometre{};
    auto distance_m = distance_km.in<DefaultRef<Metre>>();

    expectEq(distance_m.count(), 5000.0);
  };

  "conversion from m to km"_test = [] {
    auto distance_m = 5000.0 * Metre{};
    auto distance_km = distance_m.in<DefaultRef<Kilometre>>();

    expectEq(distance_km.count(), 5.0);
  };

  "time conversion"_test = [] {
    auto hours = 2.0 * Hour{};
    auto seconds = hours.in<DefaultRef<Second>>();

    expectEq(seconds.count(), 7200.0);
  };

  "speed calculation and conversion"_test = [] {
    auto distance = 100.0 * Kilometre{};
    auto time = 2.0 * Hour{};
    auto speed_kmh = distance / time;

    expectEq(speed_kmh.count(), 50.0);

    // Convert to m/s: 50 km/h = 50 * 1000 / 3600 m/s = 13.888... m/s
    auto speed_ms = speed_kmh.in<DefaultRef<MetrePerSecond>>();
    expectNear(speed_ms.count(), 50.0 * 1000.0 / 3600.0, 0.001);
  };

  "valueIn extraction"_test = [] {
    auto km = 2.5 * Kilometre{};
    auto metres = km.valueIn<DefaultRef<Metre>>();

    expectEq(metres, 2500.0);
  };

  "comparison same unit"_test = [] {
    auto a = 5.0 * Metre{};
    auto b = 3.0 * Metre{};
    auto c = 5.0 * Metre{};

    expectTrue(a == c);
    expectTrue(a != b);
    expectTrue(b < a);
    expectTrue(a > b);
    expectTrue(b <= a);
    expectTrue(a >= b);
  };

  "comparison different units same dimension"_test = [] {
    auto km = 1.0 * Kilometre{};
    auto m = 1000.0 * Metre{};

    expectTrue(km == m);
    expectFalse(km != m);

    auto m2 = 999.0 * Metre{};
    expectTrue(m2 < km);
    expectTrue(km > m2);
  };

  "scalar / quantity produces inverse"_test = [] {
    auto time = 2.0 * Second{};
    auto freq = 1.0 / time;

    expectEq(freq.count(), 0.5);
    // Frequency dimension
    expectTrue((isSame<decltype(freq)::Dimension, Frequency>));

    // Verify symbol is "1/s"
    using FreqRef = decltype(freq)::Reference;
    expectTrue((FreqRef::Unit::kSymbol == FixedString{"1/s"}));
  };

  "cross-reference addition"_test = [] {
    auto km = 1.0 * Kilometre{};
    auto m = 500.0 * Metre{};

    auto sum = km + m;
    expectEq(sum.count(), 1500.0);
    // Result is in metres (smaller magnitude)
    expectTrue((isSame<decltype(sum)::Reference, DefaultRef<Metre>>));

    auto diff = km - m;
    expectEq(diff.count(), 500.0);
  };

  "cross-reference addition with time"_test = [] {
    auto h = 1.0 * Hour{};
    auto s = 1800.0 * Second{};

    auto sum = h + s;
    expectEq(sum.count(), 5400.0);  // 3600 + 1800 = 5400 seconds
    expectTrue((isSame<decltype(sum)::Reference, DefaultRef<Second>>));
  };

  "approximateEqual"_test = [] {
    // Exact equality
    auto a = 1.0 * Metre{};
    auto b = 1.0 * Metre{};
    expectTrue(approximateEqual(a, b));

    // Cross-unit comparison
    auto km = 1.0 * Kilometre{};
    auto m = 1000.0 * Metre{};
    expectTrue(approximateEqual(km, m));

    // With small difference
    auto m2 = 1000.0000001 * Metre{};
    expectTrue(approximateEqual(km, m2, 1e-6));

    // Should fail with tight tolerance
    auto m3 = 1001.0 * Metre{};
    expectFalse(approximateEqual(km, m3, 1e-6));

    // Absolute tolerance
    auto small1 = 0.0 * Metre{};
    auto small2 = 0.0000001 * Metre{};
    expectTrue(approximateEqual(small1, small2, 1e-9, 1e-6));
  };

  return TestRegistry::result();
}
