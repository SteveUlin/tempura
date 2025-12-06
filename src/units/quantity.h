#pragma once

#include <ostream>

#include "units/reference.h"

// Quantity - A value with compile-time dimensional information
//
// A Quantity combines a runtime numeric value with a compile-time Reference
// that specifies both the quantity type (semantic meaning) and unit of measure.
//
// Examples:
//   Quantity<DefaultRef<Metre>, double> length{5.0};
//   auto speed = 100.0_km / 2.0_h;  // Quantity with km/h reference
//   auto mps = speed.in<DefaultRef<MetrePerSecond>>();  // Convert to m/s

namespace tempura {

// ============================================================================
// Forward declarations
// ============================================================================

template <ReferenceType Ref, typename Rep>
struct Quantity;

// ============================================================================
// Type traits for Quantity
// ============================================================================

template <typename T>
constexpr bool kIsQuantity = false;

template <ReferenceType Ref, typename Rep>
constexpr bool kIsQuantity<Quantity<Ref, Rep>> = true;

template <typename T>
concept QuantityType = kIsQuantity<T>;

// ============================================================================
// Floating-point detection for safe conversions
// ============================================================================

template <typename T>
constexpr bool kIsFloatingPoint = false;

template <>
constexpr bool kIsFloatingPoint<float> = true;

template <>
constexpr bool kIsFloatingPoint<double> = true;

template <>
constexpr bool kIsFloatingPoint<long double> = true;

// ============================================================================
// Quantity template
// ============================================================================

template <ReferenceType Ref, typename Rep = double>
struct Quantity {
  Rep value{};

  using Reference = Ref;
  using Representation = Rep;
  using Dimension = typename Ref::Dimension;

  // -------------------------------------------------------------------------
  // Construction
  // -------------------------------------------------------------------------

  constexpr Quantity() = default;
  constexpr explicit Quantity(Rep v) : value{v} {}

  // -------------------------------------------------------------------------
  // Implicit conversion from compatible quantities (floating-point only)
  // -------------------------------------------------------------------------
  //
  // Constraints for safe implicit conversion:
  // - Compatible dimensions (same physical quantity)
  // - Target is floating-point (allows fractional values)
  // - Source rep size <= target rep size (prevents precision loss)

  template <ReferenceType OtherRef, typename OtherRep>
    requires(kCompatibleRefs<Ref, OtherRef> && kIsFloatingPoint<Rep> &&
             sizeof(OtherRep) <= sizeof(Rep))
  constexpr Quantity(Quantity<OtherRef, OtherRep> other)
      : value{static_cast<Rep>(other.value * kRefConversionFactor<OtherRef, Ref>)} {}

  // -------------------------------------------------------------------------
  // Value access
  // -------------------------------------------------------------------------

  constexpr auto count() const -> Rep { return value; }

  // -------------------------------------------------------------------------
  // Explicit conversion
  // -------------------------------------------------------------------------

  template <ReferenceType TargetRef>
    requires kCompatibleRefs<Ref, TargetRef>
  constexpr auto in() const -> Quantity<TargetRef, Rep> {
    constexpr double factor = kRefConversionFactor<Ref, TargetRef>;
    return Quantity<TargetRef, Rep>{static_cast<Rep>(value * factor)};
  }

  // -------------------------------------------------------------------------
  // Forced conversion (allows truncation)
  // -------------------------------------------------------------------------

  template <ReferenceType TargetRef>
    requires kCompatibleRefs<Ref, TargetRef>
  constexpr auto forceIn() const -> Quantity<TargetRef, Rep> {
    constexpr double factor = kRefConversionFactor<Ref, TargetRef>;
    return Quantity<TargetRef, Rep>{static_cast<Rep>(value * factor)};
  }

  // -------------------------------------------------------------------------
  // Raw value in target unit
  // -------------------------------------------------------------------------

  template <ReferenceType TargetRef>
    requires kCompatibleRefs<Ref, TargetRef>
  constexpr auto valueIn() const -> Rep {
    constexpr double factor = kRefConversionFactor<Ref, TargetRef>;
    return static_cast<Rep>(value * factor);
  }

  // -------------------------------------------------------------------------
  // Arithmetic with same reference
  // -------------------------------------------------------------------------

  constexpr auto operator+(Quantity rhs) const -> Quantity {
    return Quantity{value + rhs.value};
  }

  constexpr auto operator-(Quantity rhs) const -> Quantity {
    return Quantity{value - rhs.value};
  }

  constexpr auto operator-() const -> Quantity { return Quantity{-value}; }

  constexpr auto operator+=(Quantity rhs) -> Quantity& {
    value += rhs.value;
    return *this;
  }

  constexpr auto operator-=(Quantity rhs) -> Quantity& {
    value -= rhs.value;
    return *this;
  }

  // -------------------------------------------------------------------------
  // Scalar multiplication/division
  // -------------------------------------------------------------------------

  constexpr auto operator*(Rep scalar) const -> Quantity {
    return Quantity{value * scalar};
  }

  constexpr auto operator/(Rep scalar) const -> Quantity {
    return Quantity{value / scalar};
  }

  constexpr auto operator*=(Rep scalar) -> Quantity& {
    value *= scalar;
    return *this;
  }

  constexpr auto operator/=(Rep scalar) -> Quantity& {
    value /= scalar;
    return *this;
  }

  // -------------------------------------------------------------------------
  // Comparison (same reference)
  // -------------------------------------------------------------------------

  constexpr auto operator==(Quantity rhs) const -> bool {
    return value == rhs.value;
  }

  constexpr auto operator!=(Quantity rhs) const -> bool {
    return value != rhs.value;
  }

  constexpr auto operator<(Quantity rhs) const -> bool {
    return value < rhs.value;
  }

  constexpr auto operator>(Quantity rhs) const -> bool {
    return value > rhs.value;
  }

  constexpr auto operator<=(Quantity rhs) const -> bool {
    return value <= rhs.value;
  }

  constexpr auto operator>=(Quantity rhs) const -> bool {
    return value >= rhs.value;
  }
};

// ============================================================================
// Common reference selection
// ============================================================================

// Pick the reference with smaller magnitude for better precision.
// E.g., CommonRef<km, m> = m (magnitude 1 < 1000)
template <ReferenceType Ref1, ReferenceType Ref2>
  requires kCompatibleRefs<Ref1, Ref2>
struct CommonRefImpl {
  static constexpr bool kRef1Smaller = Ref1::kMagnitude <= Ref2::kMagnitude;
  using Type = typename Conditional<kRef1Smaller, Ref1, Ref2>::Type;
};

template <ReferenceType Ref1, ReferenceType Ref2>
  requires kCompatibleRefs<Ref1, Ref2>
using CommonRef = typename CommonRefImpl<Ref1, Ref2>::Type;

// ============================================================================
// Scalar * Quantity (commutative)
// ============================================================================

template <ReferenceType Ref, typename Rep>
constexpr auto operator*(Rep scalar, Quantity<Ref, Rep> q) -> Quantity<Ref, Rep> {
  return q * scalar;
}

// ============================================================================
// Cross-reference addition/subtraction
// ============================================================================

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator+(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) {
  using Common = CommonRef<Ref1, Ref2>;
  using ResultRep = decltype(lhs.value + rhs.value);
  auto lhs_common = lhs.template in<Common>();
  auto rhs_common = rhs.template in<Common>();
  return Quantity<Common, ResultRep>{
      static_cast<ResultRep>(lhs_common.value) + static_cast<ResultRep>(rhs_common.value)};
}

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator-(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) {
  using Common = CommonRef<Ref1, Ref2>;
  using ResultRep = decltype(lhs.value - rhs.value);
  auto lhs_common = lhs.template in<Common>();
  auto rhs_common = rhs.template in<Common>();
  return Quantity<Common, ResultRep>{
      static_cast<ResultRep>(lhs_common.value) - static_cast<ResultRep>(rhs_common.value)};
}

// ============================================================================
// Quantity * Quantity (dimension multiplication)
// ============================================================================

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
constexpr auto operator*(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) {
  using ResultRef = RefMultiply<Ref1, Ref2>;
  using ResultRep = decltype(lhs.value * rhs.value);
  return Quantity<ResultRef, ResultRep>{lhs.value * rhs.value};
}

// ============================================================================
// Quantity / Quantity (dimension division)
// ============================================================================

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
constexpr auto operator/(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) {
  using ResultRef = RefDivide<Ref1, Ref2>;
  using ResultRep = decltype(lhs.value / rhs.value);
  return Quantity<ResultRef, ResultRep>{lhs.value / rhs.value};
}

// ============================================================================
// Scalar / Quantity (produces inverse dimension)
// ============================================================================

namespace detail {

// Helper to build inverse symbol: "1/s" from "s"
template <FixedString Symbol>
constexpr auto inverseSymbol() {
  return FixedString{"1/"} + Symbol;
}

}  // namespace detail

template <typename Scalar, ReferenceType Ref, typename Rep>
  requires(!QuantityType<Scalar>)
constexpr auto operator/(Scalar scalar, Quantity<Ref, Rep> q) {
  // Need inverse reference
  using InvSpec = QtyInverse<typename Ref::QuantitySpec>;
  using InvMag = MagInverse<typename Ref::Magnitude>;
  static constexpr auto kInvSymbol = detail::inverseSymbol<Ref::Unit::kSymbol>();
  using InvUnit = Unit<InvSpec, InvMag, kInvSymbol>;
  using InvRef = Reference<InvSpec, InvUnit>;
  using ResultRep = decltype(scalar / q.value);
  return Quantity<InvRef, ResultRep>{static_cast<ResultRep>(scalar) / q.value};
}

// ============================================================================
// Helper: Create quantity from value * unit
// ============================================================================

template <typename Rep, UnitType U>
constexpr auto operator*(Rep value, U /*unit*/) -> Quantity<DefaultRef<U>, Rep> {
  return Quantity<DefaultRef<U>, Rep>{value};
}

// ============================================================================
// Quantity comparison with different units (same dimension)
// ============================================================================

// Design Decision: Exact floating-point equality
//
// Like std::chrono, we use exact == comparison for quantities. This is
// intentional because:
// 1. Conversions between units with exact rational relationships (km to m,
//    hours to seconds) should produce exactly equal results
// 2. Approximate equality introduces ambiguity about tolerance
// 3. Users who need approximate comparison can use approximateEqual()
//
// For approximate comparisons with tolerance, use approximateEqual() below.

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator==(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) -> bool {
  // Convert lhs to rhs's units and compare exactly
  constexpr double factor = kRefConversionFactor<Ref1, Ref2>;
  return lhs.value * factor == rhs.value;
}

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator<(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) -> bool {
  constexpr double factor = kRefConversionFactor<Ref1, Ref2>;
  return lhs.value * factor < rhs.value;
}

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator!=(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) -> bool {
  return !(lhs == rhs);
}

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator>(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) -> bool {
  return rhs < lhs;
}

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator<=(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) -> bool {
  return !(lhs > rhs);
}

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires(kCompatibleRefs<Ref1, Ref2> && !isSame<Ref1, Ref2>)
constexpr auto operator>=(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs) -> bool {
  return !(lhs < rhs);
}

// ============================================================================
// Approximate equality for floating-point quantities
// ============================================================================

// Compare quantities with relative and/or absolute tolerance.
// Returns true if |lhs - rhs| <= max(relativeTolerance * |rhs|, absoluteTolerance)
//
// Usage:
//   approximateEqual(1.0_km, 999.9_m)                    // default tolerances
//   approximateEqual(a, b, 1e-6)                         // custom relative tolerance
//   approximateEqual(a, b, 1e-6, 1e-9)                   // relative + absolute

template <ReferenceType Ref1, typename Rep1, ReferenceType Ref2, typename Rep2>
  requires kCompatibleRefs<Ref1, Ref2>
constexpr auto approximateEqual(Quantity<Ref1, Rep1> lhs, Quantity<Ref2, Rep2> rhs,
                                 double relativeTolerance = 1e-9,
                                 double absoluteTolerance = 0.0) -> bool {
  // Convert both to the reference with smaller magnitude for precision
  using Common = CommonRef<Ref1, Ref2>;
  auto lhs_common = lhs.template in<Common>().count();
  auto rhs_common = rhs.template in<Common>().count();

  auto diff = lhs_common - rhs_common;
  if (diff < 0) diff = -diff;

  auto rhs_abs = rhs_common < 0 ? -rhs_common : rhs_common;
  auto tolerance = relativeTolerance * rhs_abs;
  if (absoluteTolerance > tolerance) tolerance = absoluteTolerance;

  return diff <= tolerance;
}

// Same-reference overload (no conversion needed)
template <ReferenceType Ref, typename Rep>
constexpr auto approximateEqual(Quantity<Ref, Rep> lhs, Quantity<Ref, Rep> rhs,
                                 double relativeTolerance = 1e-9,
                                 double absoluteTolerance = 0.0) -> bool {
  auto diff = lhs.value - rhs.value;
  if (diff < 0) diff = -diff;

  auto rhs_abs = rhs.value < 0 ? -rhs.value : rhs.value;
  auto tolerance = relativeTolerance * rhs_abs;
  if (absoluteTolerance > tolerance) tolerance = absoluteTolerance;

  return diff <= tolerance;
}

// ============================================================================
// Pretty printing
// ============================================================================

template <ReferenceType Ref, typename Rep>
auto operator<<(std::ostream& os, const Quantity<Ref, Rep>& q) -> std::ostream& {
  os << q.value << " " << Ref::Unit::kSymbol.c_str();
  return os;
}

}  // namespace tempura
