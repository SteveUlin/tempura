#pragma once

#include "units/unit_type.h"

// Reference - Binds a QuantitySpec to a Unit
//
// A Reference combines the semantic meaning (QuantitySpec) with a concrete
// measurement unit. This enables type-safe quantity construction where the
// quantity type carries both its semantic identity and its unit.
//
// Examples:
//   using MetreRef = Reference<QtyLength, Metre>;
//   using HeightInMetres = Reference<QtyHeight, Metre>;
//
// The operator[] syntax allows:
//   auto ref = QtyHeight{}[Metre{}];  // Reference<QtyHeight, Metre>

namespace tempura {

// ============================================================================
// Reference template
// ============================================================================

template <QuantitySpecType Spec, UnitType U>
  requires isSame<typename Spec::Dimension, typename U::QuantitySpec::Dimension>
struct Reference {
  using QuantitySpec = Spec;
  using Unit = U;
  using Dimension = typename Spec::Dimension;
  using Magnitude = typename U::Magnitude;

  static constexpr double kMagnitude = U::kMagnitude;
  static constexpr auto kSymbol = U::kSymbol;
};

// ============================================================================
// ReferenceType concept
// ============================================================================

template <typename T>
concept ReferenceType = requires {
  typename T::QuantitySpec;
  typename T::Unit;
  typename T::Dimension;
  typename T::Magnitude;
  requires QuantitySpecType<typename T::QuantitySpec>;
  requires UnitType<typename T::Unit>;
  requires MagnitudeType<typename T::Magnitude>;
};

// ============================================================================
// DefaultRef - Unit implies its associated spec
// ============================================================================

template <UnitType U>
using DefaultRef = Reference<typename U::QuantitySpec, U>;

// ============================================================================
// Reference compatibility
// ============================================================================

template <ReferenceType R1, ReferenceType R2>
constexpr bool kCompatibleRefs =
    isSame<typename R1::Dimension, typename R2::Dimension>;

// ============================================================================
// Conversion factor between references (using symbolic magnitudes)
// ============================================================================

template <ReferenceType From, ReferenceType To>
  requires kCompatibleRefs<From, To>
using RefConversionMagnitude = MagDivide<typename From::Magnitude, typename To::Magnitude>;

template <ReferenceType From, ReferenceType To>
  requires kCompatibleRefs<From, To>
constexpr double kRefConversionFactor = RefConversionMagnitude<From, To>::value();

// ============================================================================
// RefBuilder - Enables QtySpec{}[Unit{}] syntax
// ============================================================================

template <QuantitySpecType Spec>
struct RefBuilder {
  template <UnitType U>
    requires isSame<typename Spec::Dimension, typename U::QuantitySpec::Dimension>
  constexpr auto operator[](U /*unused*/) const -> Reference<Spec, U> {
    return {};
  }
};

// ============================================================================
// Predefined reference builders for common specs
// ============================================================================

inline constexpr RefBuilder<QtyLength> qtyLength{};
inline constexpr RefBuilder<QtyTime> qtyTime{};
inline constexpr RefBuilder<QtyMass> qtyMass{};
inline constexpr RefBuilder<QtySpeed> qtySpeed{};
inline constexpr RefBuilder<QtyAcceleration> qtyAcceleration{};
inline constexpr RefBuilder<QtyForce> qtyForce{};
inline constexpr RefBuilder<QtyEnergy> qtyEnergy{};
inline constexpr RefBuilder<QtyPower> qtyPower{};
inline constexpr RefBuilder<QtyPressure> qtyPressure{};
inline constexpr RefBuilder<QtyFrequency> qtyFrequency{};
inline constexpr RefBuilder<QtyAngle> qtyAngle{};

// Hierarchical specs
inline constexpr RefBuilder<QtyHeight> qtyHeight{};
inline constexpr RefBuilder<QtyWidth> qtyWidth{};
inline constexpr RefBuilder<QtyDepth> qtyDepth{};
inline constexpr RefBuilder<QtyRadius> qtyRadius{};
inline constexpr RefBuilder<QtyDistance> qtyDistance{};

// ============================================================================
// Reference algebra - multiply/divide references
// ============================================================================

namespace detail {

// Multiply two references
template <ReferenceType R1, ReferenceType R2>
struct RefMultiplyImpl {
  using Spec = QtyMultiply<typename R1::QuantitySpec, typename R2::QuantitySpec>;
  using CombinedMag = MagMultiply<typename R1::Magnitude, typename R2::Magnitude>;
  // Generate symbol: "s1·s2"
  static constexpr auto kSymbol = R1::Unit::kSymbol + FixedString{"·"} + R2::Unit::kSymbol;
  using Unit = ::tempura::Unit<Spec, CombinedMag, kSymbol>;
  using Type = Reference<Spec, Unit>;
};

// Divide two references
template <ReferenceType R1, ReferenceType R2>
struct RefDivideImpl {
  using Spec = QtyDivide<typename R1::QuantitySpec, typename R2::QuantitySpec>;
  using CombinedMag = MagDivide<typename R1::Magnitude, typename R2::Magnitude>;
  // Generate symbol: "s1/s2"
  static constexpr auto kSymbol = R1::Unit::kSymbol + FixedString{"/"} + R2::Unit::kSymbol;
  using Unit = ::tempura::Unit<Spec, CombinedMag, kSymbol>;
  using Type = Reference<Spec, Unit>;
};

}  // namespace detail

template <ReferenceType R1, ReferenceType R2>
using RefMultiply = typename detail::RefMultiplyImpl<R1, R2>::Type;

template <ReferenceType R1, ReferenceType R2>
using RefDivide = typename detail::RefDivideImpl<R1, R2>::Type;

}  // namespace tempura
