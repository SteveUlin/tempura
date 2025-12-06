#include "units/dimension.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// Base dimension tag types
// ============================================================================

static_assert(BaseDimension<DimLength>);
static_assert(BaseDimension<DimTime>);
static_assert(BaseDimension<DimMass>);
static_assert(!BaseDimension<int>);

// ============================================================================
// Dimension concept
// ============================================================================

static_assert(DimensionType<Dimensionless>);
static_assert(DimensionType<Length>);
static_assert(DimensionType<Time>);
static_assert(!DimensionType<int>);
static_assert(!DimensionType<DimLength>);

// ============================================================================
// Dimensionless
// ============================================================================

static_assert(Dimensionless::isDimensionless());
static_assert(!Length::isDimensionless());

// ============================================================================
// Representation checks - verify canonical forms
// ============================================================================

// Length = Dimension<DimLength> (not DimExp<DimLength, 1>)
static_assert(isSame<Length, Dimension<DimLength>>);

// Area = Dimension<Power<DimLength, 2>>
static_assert(isSame<Area, Dimension<Power<DimLength, 2>>>);

// Volume = Dimension<Power<DimLength, 3>>
static_assert(isSame<Volume, Dimension<Power<DimLength, 3>>>);

// Velocity = Dimension<DimLength, Per<DimTime>>
static_assert(isSame<Velocity, Dimension<DimLength, Per<DimTime>>>);

// Acceleration = Dimension<DimLength, Per<Power<DimTime, 2>>>
static_assert(isSame<Acceleration, Dimension<DimLength, Per<Power<DimTime, 2>>>>);

// Frequency = Dimension<Per<DimTime>>
static_assert(isSame<Frequency, Dimension<Per<DimTime>>>);

// ============================================================================
// Dimension multiplication - commutativity
// ============================================================================

static_assert(isSame<
    DimMultiply<Length, Time>,
    DimMultiply<Time, Length>
>);

static_assert(isSame<Area, Length::multiply<Length>>);
static_assert(isSame<Area, Length::pow<Exp{2}>>);
static_assert(isSame<Volume, Length::pow<Exp{3}>>);

// ============================================================================
// Dimension division
// ============================================================================

static_assert(isSame<Length::divide<Length>, Dimensionless>);
static_assert(isSame<Velocity, Length::divide<Time>>);
static_assert(isSame<Acceleration, Velocity::divide<Time>>);

// ============================================================================
// Dimension inversion
// ============================================================================

static_assert(isSame<Frequency, Time::inverse>);
static_assert(isSame<Frequency::inverse, Time>);

// ============================================================================
// Dimension power
// ============================================================================

static_assert(isSame<Length::pow<Exp{2}>, Area>);
static_assert(isSame<Length::pow<Exp{3}>, Volume>);

using InverseLength = Length::pow<Exp{-1}>;
static_assert(isSame<InverseLength, Length::inverse>);
static_assert(isSame<InverseLength, Dimension<Per<DimLength>>>);

// ============================================================================
// Dimension roots
// ============================================================================

static_assert(isSame<Area::sqrt, Length>);
static_assert(isSame<Volume::cbrt, Length>);

// ============================================================================
// Derived dimensions
// ============================================================================

static_assert(isSame<Force, Mass::multiply<Acceleration>>);
static_assert(isSame<Energy, Force::multiply<Length>>);
static_assert(isSame<PowerDim, Energy::divide<Time>>);
static_assert(isSame<Pressure, Force::divide<Area>>);

// ============================================================================
// Complex expressions
// ============================================================================

using KineticEnergyDim = Mass::multiply<Velocity::pow<Exp{2}>>;
static_assert(isSame<KineticEnergyDim, Energy>);

using PotentialEnergyDim = Mass::multiply<Acceleration>::multiply<Length>;
static_assert(isSame<PotentialEnergyDim, Energy>);

// ============================================================================
// Commutativity
// ============================================================================

static_assert(isSame<Mass::multiply<Length>, Length::multiply<Mass>>);
static_assert(isSame<Force::multiply<Time>, Time::multiply<Force>>);

// ============================================================================
// Sorting verification - dimensions should be sorted by symbol
// ============================================================================

// L < M < T in symbol order, so Length * Mass * Time should be sorted
// DimLength("L") < DimMass("M") < DimTime("T")
using LMT = Length::multiply<Mass>::multiply<Time>;
using TML = Time::multiply<Mass>::multiply<Length>;
static_assert(isSame<LMT, TML>);  // Same canonical form regardless of order

auto main() -> int {
  "dimension base types satisfy BaseDimension concept"_test = [] {
    expectTrue(BaseDimension<DimLength>);
    expectTrue(BaseDimension<DimTime>);
    expectTrue(BaseDimension<DimMass>);
  };

  "dimensionless check"_test = [] {
    expectTrue(Dimensionless::isDimensionless());
    expectFalse(Length::isDimensionless());
    expectFalse(Energy::isDimensionless());
  };

  "velocity dimension uses Per<>"_test = [] {
    expectTrue((isSame<Velocity, Dimension<DimLength, Per<DimTime>>>));
  };

  "energy dimension"_test = [] {
    // Energy = M L² T⁻² = Dimension<DimLength, DimMass, Per<Power<DimTime, 2>>>
    // Note: sorted by symbol, so L comes before M
    using Expected = Dimension<Power<DimLength, 2>, DimMass, Per<Power<DimTime, 2>>>;
    expectTrue((isSame<Energy, Expected>));
  };

  "dimension algebra laws"_test = [] {
    // Associativity: (A × B) × C = A × (B × C)
    using LHS = Mass::multiply<Length>::multiply<Time>;
    using RHS = Mass::multiply<Length::multiply<Time>>;
    expectTrue((isSame<LHS, RHS>));

    // Identity: A × 1 = A
    expectTrue((isSame<Length::multiply<Dimensionless>, Length>));

    // Inverse: A × A⁻¹ = 1
    expectTrue((isSame<Length::multiply<Length::inverse>, Dimensionless>));
  };

  "no DimExp in output"_test = [] {
    // Verify Length is Dimension<DimLength>, not Dimension<DimExp<DimLength, 1>>
    expectTrue((isSame<Length, Dimension<DimLength>>));

    // Verify Area uses Power, not DimExp
    expectTrue((isSame<Area, Dimension<Power<DimLength, 2>>>));
  };

  return TestRegistry::result();
}

// ============================================================================
// Dimensionless behavior
// ============================================================================

// Length / Length = Dimensionless
static_assert(isSame<Length::divide<Length>, Dimensionless>);
static_assert(Length::divide<Length>::isDimensionless());

// Dimensionless * Length = Length
static_assert(isSame<Dimensionless::multiply<Length>, Length>);

// Length * Dimensionless = Length  
static_assert(isSame<Length::multiply<Dimensionless>, Length>);

// Dimensionless * Dimensionless = Dimensionless
static_assert(isSame<Dimensionless::multiply<Dimensionless>, Dimensionless>);

// Complex cancellation: (L * T) / (L * T) = Dimensionless
using LT = Length::multiply<Time>;
static_assert(isSame<LT::divide<LT>, Dimensionless>);

// Partial cancellation: (L * T) / T = L
static_assert(isSame<LT::divide<Time>, Length>);

// Dimensionless^2 = Dimensionless
static_assert(isSame<Dimensionless::pow<Exp{2}>, Dimensionless>);

// ============================================================================
// Normalize - standalone normalization (separate from algebraic operations)
// ============================================================================

// Normalize sorts terms by symbol
static_assert(isSame<
    Normalize<DimTime, DimMass, DimLength>,  // T, M, L (unsorted)
    Dimension<DimLength, DimMass, DimTime>   // L, M, T (sorted)
>);

// Normalize combines same-base terms
static_assert(isSame<
    Normalize<DimMass, DimMass>,
    Dimension<Power<DimMass, 2>>
>);

// Normalize handles Per<> and combines exponents
static_assert(isSame<
    Normalize<DimMass, Per<DimTime>, DimMass>,  // M * T⁻¹ * M
    Dimension<Power<DimMass, 2>, Per<DimTime>>  // M² / T
>);

// Normalize eliminates zero-exponent terms
static_assert(isSame<
    Normalize<DimLength, Per<DimLength>>,  // L * L⁻¹
    Dimensionless
>);

// Normalize handles Power<> terms
static_assert(isSame<
    Normalize<Power<DimLength, 2>, DimLength>,  // L² * L
    Dimension<Power<DimLength, 3>>              // L³
>);

// Normalize handles mixed terms
static_assert(isSame<
    Normalize<DimMass, Power<DimLength, 2>, Per<Power<DimTime, 2>>>,
    Energy  // M L² T⁻²
>);

// Already-canonical input passes through unchanged
static_assert(isSame<
    Normalize<DimLength, Per<DimTime>>,
    Velocity
>);
