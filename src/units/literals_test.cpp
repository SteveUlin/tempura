#include "units/units.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::literals;

// ============================================================================
// Static tests for literals
// ============================================================================

// Length literals
static_assert(QuantityType<decltype(1.0_m)>);
static_assert(QuantityType<decltype(1_m)>);
static_assert(QuantityType<decltype(1.0_km)>);
static_assert(QuantityType<decltype(1.0_cm)>);
static_assert(QuantityType<decltype(1.0_mm)>);

// Time literals
static_assert(QuantityType<decltype(1.0_s)>);
static_assert(QuantityType<decltype(1.0_ms)>);
static_assert(QuantityType<decltype(1.0_us)>);
static_assert(QuantityType<decltype(1.0_ns)>);
static_assert(QuantityType<decltype(1.0_min)>);
static_assert(QuantityType<decltype(1.0_h)>);

// Mass literals
static_assert(QuantityType<decltype(1.0_kg)>);
static_assert(QuantityType<decltype(1.0_g)>);
static_assert(QuantityType<decltype(1.0_mg)>);
static_assert(QuantityType<decltype(1.0_t)>);

// Force/Energy/Power literals
static_assert(QuantityType<decltype(1.0_N)>);
static_assert(QuantityType<decltype(1.0_J)>);
static_assert(QuantityType<decltype(1.0_W)>);

// Frequency/Pressure literals
static_assert(QuantityType<decltype(1.0_Hz)>);
static_assert(QuantityType<decltype(1.0_Pa)>);
static_assert(QuantityType<decltype(1.0_bar)>);

// ============================================================================
// Value checks
// ============================================================================

static_assert((5.0_m).count() == 5.0);
static_assert((10_m).count() == 10);
static_assert((2.5_km).count() == 2.5);
static_assert((100_s).count() == 100);

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "literal construction"_test = [] {
    auto length = 5.0_m;
    expectEq(length.count(), 5.0);

    auto time = 10.0_s;
    expectEq(time.count(), 10.0);

    auto mass = 2.5_kg;
    expectEq(mass.count(), 2.5);
  };

  "integer literals"_test = [] {
    auto length = 100_m;
    expectEq(length.count(), 100LL);

    auto time = 60_s;
    expectEq(time.count(), 60LL);
  };

  "speed from literals"_test = [] {
    auto distance = 100.0_km;
    auto time = 2.0_h;
    auto speed = distance / time;

    expectEq(speed.count(), 50.0);
  };

  "force from literals"_test = [] {
    auto mass = 10.0_kg;
    auto accel = 9.8_m / (1.0_s * 1.0_s);
    auto force = mass * accel;

    expectNear(force.count(), 98.0, 0.001);
  };

  "conversion chain"_test = [] {
    // 1 km = 1000 m
    auto km = 1.0_km;
    auto m = km.in<DefaultRef<Metre>>();
    expectEq(m.count(), 1000.0);

    // 1 hour = 3600 seconds
    auto h = 1.0_h;
    auto s = h.in<DefaultRef<Second>>();
    expectEq(s.count(), 3600.0);
  };

  "physics calculation: kinetic energy"_test = [] {
    // E = 0.5 * m * v^2
    auto mass = 1000.0_kg;  // 1 tonne car
    auto speed_kmh = 100.0_km / 1.0_h;

    // Convert to m/s for SI coherent calculation
    auto speed_ms = speed_kmh.in<DefaultRef<MetrePerSecond>>();
    auto v_squared = speed_ms * speed_ms;
    auto energy_raw = mass * v_squared;

    // 0.5 * 1000 * (27.78)^2 ≈ 385802 J
    auto half_mv2 = energy_raw / 2.0;
    expectNear(half_mv2.count(), 385802.47, 1.0);
  };

  "mixed units arithmetic"_test = [] {
    // 1 km + 500 m = 1.5 km (using conversion)
    auto km = 1.0_km;
    auto m = 500.0_m;

    // Convert m to km first
    auto m_as_km = m.in<DefaultRef<Kilometre>>();
    auto total = km + m_as_km;

    expectEq(total.count(), 1.5);
  };

  "umbrella header includes all"_test = [] {
    // Test that units.h provides everything
    expectTrue(DimensionType<Length>);
    expectTrue(QuantitySpecType<QtyLength>);
    expectTrue(UnitType<Metre>);
    expectTrue(ReferenceType<DefaultRef<Metre>>);
    expectTrue(QuantityType<decltype(1.0_m)>);
  };

  return TestRegistry::result();
}
