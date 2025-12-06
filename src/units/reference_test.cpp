#include "units/reference.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// ReferenceType concept
// ============================================================================

static_assert(ReferenceType<Reference<QtyLength, Metre>>);
static_assert(ReferenceType<Reference<QtyTime, Second>>);
static_assert(ReferenceType<DefaultRef<Metre>>);
static_assert(ReferenceType<DefaultRef<Kilometre>>);
static_assert(!ReferenceType<int>);
static_assert(!ReferenceType<QtyLength>);  // QuantitySpec, not Reference
static_assert(!ReferenceType<Metre>);      // Unit, not Reference

// ============================================================================
// DefaultRef
// ============================================================================

static_assert(isSame<DefaultRef<Metre>, Reference<QtyLength, Metre>>);
static_assert(isSame<DefaultRef<Second>, Reference<QtyTime, Second>>);
static_assert(isSame<DefaultRef<Newton>, Reference<QtyForce, Newton>>);

// ============================================================================
// Reference properties
// ============================================================================

static_assert(isSame<Reference<QtyLength, Metre>::QuantitySpec, QtyLength>);
static_assert(isSame<Reference<QtyLength, Metre>::Unit, Metre>);
static_assert(isSame<Reference<QtyLength, Metre>::Dimension, Length>);
static_assert(Reference<QtyLength, Metre>::kMagnitude == 1.0);
static_assert(Reference<QtyLength, Kilometre>::kMagnitude == 1000.0);

// ============================================================================
// Hierarchical specs with units
// ============================================================================

// Height can use Metre (same dimension as Length)
static_assert(ReferenceType<Reference<QtyHeight, Metre>>);
static_assert(isSame<Reference<QtyHeight, Metre>::Dimension, Length>);

// ============================================================================
// Reference compatibility
// ============================================================================

static_assert(kCompatibleRefs<DefaultRef<Metre>, DefaultRef<Kilometre>>);
static_assert(kCompatibleRefs<Reference<QtyHeight, Metre>, DefaultRef<Centimetre>>);
static_assert(!kCompatibleRefs<DefaultRef<Metre>, DefaultRef<Second>>);
static_assert(!kCompatibleRefs<DefaultRef<Newton>, DefaultRef<Metre>>);

// ============================================================================
// Conversion factors between references (now doubles)
// ============================================================================

static_assert(kRefConversionFactor<DefaultRef<Kilometre>, DefaultRef<Metre>> == 1000.0);
static_assert(kRefConversionFactor<DefaultRef<Metre>, DefaultRef<Kilometre>> == 0.001);
static_assert(kRefConversionFactor<DefaultRef<Hour>, DefaultRef<Second>> == 3600.0);

// Height to Length conversion (same dimension)
static_assert(kRefConversionFactor<Reference<QtyHeight, Metre>, DefaultRef<Centimetre>> == 100.0);

// ============================================================================
// RefBuilder operator[] syntax
// ============================================================================

// qtyLength[Metre{}] → Reference<QtyLength, Metre>
static_assert(isSame<decltype(qtyLength[Metre{}]), Reference<QtyLength, Metre>>);
static_assert(isSame<decltype(qtyTime[Second{}]), Reference<QtyTime, Second>>);
static_assert(isSame<decltype(qtyHeight[Metre{}]), Reference<QtyHeight, Metre>>);
static_assert(isSame<decltype(qtyHeight[Centimetre{}]), Reference<QtyHeight, Centimetre>>);

// ============================================================================
// Reference algebra
// ============================================================================

// Multiply: length * time = length*time
using LengthTimeRef = RefMultiply<DefaultRef<Metre>, DefaultRef<Second>>;
static_assert(ReferenceType<LengthTimeRef>);

// Divide: length / time = speed
using SpeedRef = RefDivide<DefaultRef<Metre>, DefaultRef<Second>>;
static_assert(ReferenceType<SpeedRef>);
static_assert(isSame<SpeedRef::Dimension, Velocity>);
static_assert(SpeedRef::kMagnitude == 1.0);  // m/s has magnitude 1

// km/h reference
using KmPerHourRef = RefDivide<DefaultRef<Kilometre>, DefaultRef<Hour>>;
static_assert(ReferenceType<KmPerHourRef>);
static_assert(isSame<KmPerHourRef::Dimension, Velocity>);
// 1000/3600 = 5/18 ≈ 0.277...
constexpr double kKmPerHourMag = 5.0 / 18.0;
static_assert(KmPerHourRef::kMagnitude == kKmPerHourMag);

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "ReferenceType concept"_test = [] {
    expectTrue(ReferenceType<DefaultRef<Metre>>);
    expectTrue(ReferenceType<Reference<QtyHeight, Metre>>);
    expectFalse(ReferenceType<int>);
    expectFalse(ReferenceType<Metre>);
  };

  "DefaultRef creates correct reference"_test = [] {
    expectTrue((isSame<DefaultRef<Metre>::Unit, Metre>));
    expectTrue((isSame<DefaultRef<Metre>::QuantitySpec, QtyLength>));
  };

  "reference compatibility"_test = [] {
    expectTrue((kCompatibleRefs<DefaultRef<Metre>, DefaultRef<Kilometre>>));
    expectFalse((kCompatibleRefs<DefaultRef<Metre>, DefaultRef<Second>>));
  };

  "conversion factors"_test = [] {
    expectEq(kRefConversionFactor<DefaultRef<Kilometre>, DefaultRef<Metre>>, 1000.0);
    expectEq(kRefConversionFactor<DefaultRef<Metre>, DefaultRef<Kilometre>>, 0.001);
  };

  "RefBuilder syntax"_test = [] {
    auto ref = qtyHeight[Metre{}];
    expectTrue((isSame<decltype(ref), Reference<QtyHeight, Metre>>));
  };

  "reference algebra"_test = [] {
    // m/s magnitude is 1
    expectEq(SpeedRef::kMagnitude, 1.0);

    // km/h magnitude is 5/18
    expectNear(KmPerHourRef::kMagnitude, 5.0 / 18.0, 1e-15);
  };

  return TestRegistry::result();
}
