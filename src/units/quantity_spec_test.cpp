#include "units/quantity_spec.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// QuantitySpecType concept
// ============================================================================

static_assert(QuantitySpecType<QtyLength>);
static_assert(QuantitySpecType<QtyTime>);
static_assert(QuantitySpecType<QtyMass>);
static_assert(QuantitySpecType<QtySpeed>);
static_assert(QuantitySpecType<QtyHeight>);
static_assert(!QuantitySpecType<int>);
static_assert(!QuantitySpecType<Length>);  // Dimension, not QuantitySpec

// ============================================================================
// Dimension association
// ============================================================================

static_assert(isSame<QtyLength::Dimension, Length>);
static_assert(isSame<QtyTime::Dimension, Time>);
static_assert(isSame<QtyMass::Dimension, Mass>);
static_assert(isSame<QtySpeed::Dimension, Velocity>);
static_assert(isSame<QtyAcceleration::Dimension, Acceleration>);
static_assert(isSame<QtyForce::Dimension, Force>);
static_assert(isSame<QtyEnergy::Dimension, Energy>);

// ============================================================================
// Hierarchy: kIsA checks
// ============================================================================

// Identity: every spec is-a itself
static_assert(kIsA<QtyLength, QtyLength>);
static_assert(kIsA<QtyHeight, QtyHeight>);

// Sub-quantities are-a their parent
static_assert(kIsA<QtyHeight, QtyLength>);
static_assert(kIsA<QtyWidth, QtyLength>);
static_assert(kIsA<QtyRadius, QtyLength>);
static_assert(kIsA<QtyDepth, QtyLength>);

// Parent is NOT-a child (hierarchy is directional)
static_assert(!kIsA<QtyLength, QtyHeight>);
static_assert(!kIsA<QtyLength, QtyWidth>);

// Unrelated specs are not is-a
static_assert(!kIsA<QtyLength, QtyTime>);
static_assert(!kIsA<QtyHeight, QtyTime>);
static_assert(!kIsA<QtySpeed, QtyLength>);

// ============================================================================
// Dimension compatibility
// ============================================================================

static_assert(kSameDimension<QtyLength, QtyLength>);
static_assert(kSameDimension<QtyHeight, QtyLength>);  // same dimension, different specs
static_assert(kSameDimension<QtyWidth, QtyHeight>);   // both have Length dimension
static_assert(!kSameDimension<QtyLength, QtyTime>);
static_assert(!kSameDimension<QtySpeed, QtyLength>);

// ============================================================================
// Algebraic operations
// ============================================================================

// Multiply
static_assert(isSame<QtyMultiply<QtyLength, QtyLength>::Dimension, Area>);
static_assert(isSame<QtyMultiply<QtyMass, QtyAcceleration>::Dimension, Force>);

// Divide
static_assert(isSame<QtyDivide<QtyLength, QtyTime>::Dimension, Velocity>);
static_assert(isSame<QtyDivide<QtyLength, QtyLength>::Dimension, Dimensionless>);

// Power
static_assert(isSame<QtyPow<QtyLength, Exp{2}>::Dimension, Area>);
static_assert(isSame<QtyPow<QtyLength, Exp{3}>::Dimension, Volume>);

// Inverse
static_assert(isSame<QtyInverse<QtyTime>::Dimension, Frequency>);

// Square root
static_assert(isSame<QtySqrt<QtyArea>::Dimension, Length>);

// ============================================================================
// Derived quantity specs have correct dimensions
// ============================================================================

static_assert(isSame<QtyArea::Dimension, Area>);
static_assert(isSame<QtyVolume::Dimension, Volume>);
static_assert(isSame<QtyFrequency::Dimension, Frequency>);
static_assert(isSame<QtyPressure::Dimension, Pressure>);
static_assert(isSame<QtyMomentum::Dimension, Momentum>);

// ============================================================================
// Dimensionless
// ============================================================================

static_assert(QuantitySpecType<QtyDimensionless>);
static_assert(isSame<QtyDimensionless::Dimension, Dimensionless>);
static_assert(QtyDimensionless::Dimension::isDimensionless());

// ============================================================================
// Runtime tests
// ============================================================================

auto main() -> int {
  "QuantitySpecType concept"_test = [] {
    expectTrue(QuantitySpecType<QtyLength>);
    expectTrue(QuantitySpecType<QtySpeed>);
    expectTrue(QuantitySpecType<QtyHeight>);
    expectFalse(QuantitySpecType<int>);
  };

  "hierarchy is-a relationships"_test = [] {
    // Sub-quantities
    expectTrue((kIsA<QtyHeight, QtyLength>));
    expectTrue((kIsA<QtyWidth, QtyLength>));
    expectTrue((kIsA<QtyRadius, QtyLength>));

    // Not reverse
    expectFalse((kIsA<QtyLength, QtyHeight>));

    // Unrelated
    expectFalse((kIsA<QtyLength, QtyTime>));
  };

  "dimension compatibility"_test = [] {
    expectTrue((kSameDimension<QtyHeight, QtyWidth>));
    expectFalse((kSameDimension<QtyLength, QtyTime>));
  };

  "algebraic operations preserve dimensions"_test = [] {
    // Speed = Length / Time
    expectTrue((isSame<QtySpeed::Dimension, Velocity>));

    // Force = Mass * Acceleration
    expectTrue((isSame<QtyForce::Dimension, Force>));

    // Energy = Force * Length
    expectTrue((isSame<QtyEnergy::Dimension, Energy>));
  };

  return TestRegistry::result();
}
