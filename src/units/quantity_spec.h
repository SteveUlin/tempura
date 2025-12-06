#pragma once

#include "units/dimension.h"

// QuantitySpec - Semantic quantity identifiers with hierarchy support
//
// A QuantitySpec ties a semantic meaning (like "height" or "speed") to a dimension.
// It supports hierarchical relationships: QtyHeight is-a QtyLength.
//
// Examples:
//   struct QtyLength : QuantitySpec<Length> {};           // base quantity
//   struct QtyHeight : QuantitySpec<Length, QtyLength> {}; // sub-quantity
//   using QtySpeed = QtyDivide<QtyLength, QtyTime>;       // derived quantity

namespace tempura {

// ============================================================================
// QuantitySpec - Base template
// ============================================================================

// Primary template: root quantity spec (no parent)
template <DimensionType Dim, typename Parent = void>
struct QuantitySpec {
  using Dimension = Dim;
  using ParentSpec = Parent;
};

// ============================================================================
// QuantitySpecType concept
// ============================================================================

template <typename T>
constexpr bool kIsQuantitySpec = false;

template <DimensionType Dim, typename Parent>
constexpr bool kIsQuantitySpec<QuantitySpec<Dim, Parent>> = true;

// Also match types that inherit from QuantitySpec
template <typename T>
concept QuantitySpecType = requires {
  typename T::Dimension;
  typename T::ParentSpec;
  requires DimensionType<typename T::Dimension>;
};

// ============================================================================
// Hierarchy checking: kIsA<Q1, Q2> - is Q1 the same as or derived from Q2?
// ============================================================================

namespace detail {

template <typename Q1, typename Q2>
struct IsAImpl {
  static constexpr bool value = false;
};

// Same type
template <typename Q>
struct IsAImpl<Q, Q> {
  static constexpr bool value = true;
};

// Check parent chain
template <QuantitySpecType Q1, QuantitySpecType Q2>
  requires(!isSame<Q1, Q2> && !isSame<typename Q1::ParentSpec, void>)
struct IsAImpl<Q1, Q2> {
  static constexpr bool value = IsAImpl<typename Q1::ParentSpec, Q2>::value;
};

}  // namespace detail

template <QuantitySpecType Q1, QuantitySpecType Q2>
constexpr bool kIsA = detail::IsAImpl<Q1, Q2>::value;

// ============================================================================
// Dimension compatibility
// ============================================================================

template <QuantitySpecType Q1, QuantitySpecType Q2>
constexpr bool kSameDimension =
    isSame<typename Q1::Dimension, typename Q2::Dimension>;

// ============================================================================
// Algebraic operations on QuantitySpecs
// ============================================================================

// Multiply two quantity specs - produces anonymous spec with combined dimension
template <QuantitySpecType Q1, QuantitySpecType Q2>
using QtyMultiply =
    QuantitySpec<typename Q1::Dimension::template multiply<typename Q2::Dimension>>;

// Divide two quantity specs
template <QuantitySpecType Q1, QuantitySpecType Q2>
using QtyDivide =
    QuantitySpec<typename Q1::Dimension::template divide<typename Q2::Dimension>>;

// Power of a quantity spec
template <QuantitySpecType Q, Exp E>
using QtyPow = QuantitySpec<typename Q::Dimension::template pow<E>>;

// Inverse of a quantity spec
template <QuantitySpecType Q>
using QtyInverse = QuantitySpec<typename Q::Dimension::inverse>;

// Square root of a quantity spec
template <QuantitySpecType Q>
using QtySqrt = QuantitySpec<typename Q::Dimension::sqrt>;

// ============================================================================
// SI Base Quantity Specs
// ============================================================================

struct QtyLength : QuantitySpec<Length> {};
struct QtyTime : QuantitySpec<Time> {};
struct QtyMass : QuantitySpec<Mass> {};
struct QtyElectricCurrent : QuantitySpec<Current> {};
struct QtyTemperature : QuantitySpec<Temperature> {};
struct QtyAmountOfSubstance : QuantitySpec<Amount> {};
struct QtyLuminousIntensity : QuantitySpec<Luminosity> {};

// ============================================================================
// Common Derived Quantity Specs
// ============================================================================

using QtyArea = QtyPow<QtyLength, Exp{2}>;
using QtyVolume = QtyPow<QtyLength, Exp{3}>;
using QtyFrequency = QtyInverse<QtyTime>;
using QtySpeed = QtyDivide<QtyLength, QtyTime>;
using QtyVelocity = QtySpeed;  // alias (same dimension, different semantics if needed)
using QtyAcceleration = QtyDivide<QtySpeed, QtyTime>;
using QtyForce = QtyMultiply<QtyMass, QtyAcceleration>;
using QtyEnergy = QtyMultiply<QtyForce, QtyLength>;
using QtyPower = QtyDivide<QtyEnergy, QtyTime>;
using QtyPressure = QtyDivide<QtyForce, QtyArea>;
using QtyMomentum = QtyMultiply<QtyMass, QtySpeed>;

// ============================================================================
// Dimensionless
// ============================================================================

using QtyDimensionless = QuantitySpec<Dimensionless>;

// ============================================================================
// Angle (dimensionless but semantically distinct)
// ============================================================================

// Angle is dimensionless in SI but treated as distinct for type safety
struct QtyAngle : QuantitySpec<Dimensionless, QtyDimensionless> {};

// ============================================================================
// Example hierarchical quantity specs (specialized lengths)
// ============================================================================

struct QtyHeight : QuantitySpec<Length, QtyLength> {};
struct QtyWidth : QuantitySpec<Length, QtyLength> {};
struct QtyDepth : QuantitySpec<Length, QtyLength> {};
struct QtyRadius : QuantitySpec<Length, QtyLength> {};
struct QtyDiameter : QuantitySpec<Length, QtyLength> {};
struct QtyDistance : QuantitySpec<Length, QtyLength> {};
struct QtyPathLength : QuantitySpec<Length, QtyLength> {};

}  // namespace tempura
