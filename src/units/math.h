#pragma once

#include <cmath>

#include "units/quantity.h"

// Mathematical functions for Quantity types
//
// Provides unit-aware versions of standard math functions:
// - Basic: abs, floor, ceil, round, trunc, fmod
// - Powers: sqrt, cbrt, pow<N>, pow<N,D>
// - Trigonometric: sin, cos, tan (for angle quantities)
// - Inverse trig: arcsin, arccos, arctan, atan2 (return angles)
// - Hyperbolic: sinh, cosh, tanh
// - Other: hypot, fma
//
// Example:
//   auto area = 100.0_m * 100.0_m;  // 10000 m²
//   auto side = sqrt(area);          // 100 m
//
//   auto angle = 45.0_deg;
//   auto x = cos(angle);             // ~0.707 (dimensionless)

namespace tempura {

// ============================================================================
// Basic functions (preserve units)
// ============================================================================

// Absolute value
template <ReferenceType Ref, typename Rep>
constexpr auto abs(Quantity<Ref, Rep> q) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{q.value < Rep{0} ? -q.value : q.value};
}

// Floor - round toward negative infinity
template <ReferenceType Ref, typename Rep>
constexpr auto floor(Quantity<Ref, Rep> q) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::floor(q.value))};
}

// Ceil - round toward positive infinity
template <ReferenceType Ref, typename Rep>
constexpr auto ceil(Quantity<Ref, Rep> q) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::ceil(q.value))};
}

// Round - round to nearest integer
template <ReferenceType Ref, typename Rep>
constexpr auto round(Quantity<Ref, Rep> q) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::round(q.value))};
}

// Trunc - round toward zero
template <ReferenceType Ref, typename Rep>
constexpr auto trunc(Quantity<Ref, Rep> q) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::trunc(q.value))};
}

// Floating-point modulo (same units required)
template <ReferenceType Ref, typename Rep>
constexpr auto fmod(Quantity<Ref, Rep> x, Quantity<Ref, Rep> y) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::fmod(x.value, y.value))};
}

// ============================================================================
// Power functions (change dimensions)
// ============================================================================

namespace detail {

// Build reference for power result: Ref^(N/D)
template <ReferenceType Ref, int N, int D = 1>
struct PowerRef {
  using OrigSpec = typename Ref::QuantitySpec;
  using OrigMag = typename Ref::Magnitude;

  // New QuantitySpec with powered dimension
  using NewSpec = QtyPow<OrigSpec, Exp{N, D}>;

  // Compute Mag^(N/D) properly
  using NewMag = MagPow<OrigMag, N, D>;

  // Build symbol based on the power
  static constexpr auto buildSymbol() {
    if constexpr (N == 1 && D == 2) {
      return FixedString{"√"} + Ref::Unit::kSymbol;
    } else if constexpr (N == 1 && D == 3) {
      return FixedString{"∛"} + Ref::Unit::kSymbol;
    } else if constexpr (N == 2 && D == 1) {
      return Ref::Unit::kSymbol + FixedString{"²"};
    } else if constexpr (N == 3 && D == 1) {
      return Ref::Unit::kSymbol + FixedString{"³"};
    } else if constexpr (N == -1 && D == 1) {
      return Ref::Unit::kSymbol + FixedString{"⁻¹"};
    } else if constexpr (N == -2 && D == 1) {
      return Ref::Unit::kSymbol + FixedString{"⁻²"};
    } else {
      return Ref::Unit::kSymbol;
    }
  }

  static constexpr auto kSymbol = buildSymbol();
  using NewUnit = Unit<NewSpec, NewMag, kSymbol>;
  using Type = Reference<NewSpec, NewUnit>;
};

}  // namespace detail

// Square root: sqrt(L²) -> L, sqrt(L) -> L^(1/2)
// Result is in the powered unit: sqrt(km²) -> km, sqrt(m²) -> m
template <ReferenceType Ref, typename Rep>
constexpr auto sqrt(Quantity<Ref, Rep> q) {
  using ResultRef = typename detail::PowerRef<Ref, 1, 2>::Type;
  // Compute sqrt of the raw value; magnitude is handled by the type
  auto result_value = std::sqrt(q.value);
  return Quantity<ResultRef, Rep>{static_cast<Rep>(result_value)};
}

// Cube root: cbrt(L³) -> L
template <ReferenceType Ref, typename Rep>
constexpr auto cbrt(Quantity<Ref, Rep> q) {
  using ResultRef = typename detail::PowerRef<Ref, 1, 3>::Type;
  auto result_value = std::cbrt(q.value);
  return Quantity<ResultRef, Rep>{static_cast<Rep>(result_value)};
}

// Integer power: pow<2>(length) -> area
template <int N, ReferenceType Ref, typename Rep>
constexpr auto pow(Quantity<Ref, Rep> q) {
  using ResultRef = typename detail::PowerRef<Ref, N, 1>::Type;
  auto result_value = std::pow(q.value, static_cast<double>(N));
  return Quantity<ResultRef, Rep>{static_cast<Rep>(result_value)};
}

// Rational power: pow<1,2>(area) -> length (same as sqrt)
template <int N, int D, ReferenceType Ref, typename Rep>
constexpr auto pow(Quantity<Ref, Rep> q) {
  using ResultRef = typename detail::PowerRef<Ref, N, D>::Type;
  auto result_value = std::pow(q.value, static_cast<double>(N) / static_cast<double>(D));
  return Quantity<ResultRef, Rep>{static_cast<Rep>(result_value)};
}

// ============================================================================
// Trigonometric functions (angles -> dimensionless)
// ============================================================================

// Helper to convert angle to radians
template <ReferenceType Ref, typename Rep>
constexpr auto toRadians(Quantity<Ref, Rep> angle) -> double {
  // Convert to SI coherent unit (radians)
  return angle.value * Ref::kMagnitude;
}

// Sine of angle
template <ReferenceType Ref, typename Rep>
  requires isSame<typename Ref::QuantitySpec::Dimension, Dimensionless>  // angle dimension
constexpr auto sin(Quantity<Ref, Rep> angle) -> double {
  return std::sin(toRadians(angle));
}

// Cosine of angle
template <ReferenceType Ref, typename Rep>
  requires isSame<typename Ref::QuantitySpec::Dimension, Dimensionless>
constexpr auto cos(Quantity<Ref, Rep> angle) -> double {
  return std::cos(toRadians(angle));
}

// Tangent of angle
template <ReferenceType Ref, typename Rep>
  requires isSame<typename Ref::QuantitySpec::Dimension, Dimensionless>
constexpr auto tan(Quantity<Ref, Rep> angle) -> double {
  return std::tan(toRadians(angle));
}

// ============================================================================
// Inverse trigonometric functions (dimensionless -> angles)
// ============================================================================

// Arc sine - returns angle in radians
// Named arcsin to avoid collision with std::asin
constexpr auto arcsin(double x) -> Quantity<DefaultRef<Radian>, double> {
  return Quantity<DefaultRef<Radian>, double>{std::asin(x)};
}

// Arc cosine - returns angle in radians
// Named arccos to avoid collision with std::acos
constexpr auto arccos(double x) -> Quantity<DefaultRef<Radian>, double> {
  return Quantity<DefaultRef<Radian>, double>{std::acos(x)};
}

// Arc tangent - returns angle in radians
// Named arctan to avoid collision with std::atan
constexpr auto arctan(double x) -> Quantity<DefaultRef<Radian>, double> {
  return Quantity<DefaultRef<Radian>, double>{std::atan(x)};
}

// Two-argument arc tangent (for vectors)
template <ReferenceType Ref, typename Rep>
constexpr auto atan2(Quantity<Ref, Rep> y, Quantity<Ref, Rep> x)
    -> Quantity<DefaultRef<Radian>, double> {
  return Quantity<DefaultRef<Radian>, double>{std::atan2(y.value, x.value)};
}

// ============================================================================
// Hyperbolic functions (for dimensionless quantities)
// ============================================================================

template <ReferenceType Ref, typename Rep>
  requires isSame<typename Ref::QuantitySpec::Dimension, Dimensionless>
constexpr auto sinh(Quantity<Ref, Rep> x) -> double {
  return std::sinh(x.value * Ref::kMagnitude);
}

template <ReferenceType Ref, typename Rep>
  requires isSame<typename Ref::QuantitySpec::Dimension, Dimensionless>
constexpr auto cosh(Quantity<Ref, Rep> x) -> double {
  return std::cosh(x.value * Ref::kMagnitude);
}

template <ReferenceType Ref, typename Rep>
  requires isSame<typename Ref::QuantitySpec::Dimension, Dimensionless>
constexpr auto tanh(Quantity<Ref, Rep> x) -> double {
  return std::tanh(x.value * Ref::kMagnitude);
}

// ============================================================================
// Other useful functions
// ============================================================================

// Hypotenuse: sqrt(x² + y²) - same units
template <ReferenceType Ref, typename Rep>
constexpr auto hypot(Quantity<Ref, Rep> x, Quantity<Ref, Rep> y) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::hypot(x.value, y.value))};
}

// Three-argument hypot
template <ReferenceType Ref, typename Rep>
constexpr auto hypot(Quantity<Ref, Rep> x, Quantity<Ref, Rep> y, Quantity<Ref, Rep> z)
    -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::hypot(x.value, y.value, z.value))};
}

// Fused multiply-add: x * y + z (all same units, returns same units)
// Note: This is for scalar factors; for mixed dimensions, use regular operators
template <ReferenceType Ref, typename Rep>
constexpr auto fma(Quantity<Ref, Rep> x, Rep y, Quantity<Ref, Rep> z) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::fma(x.value, y, z.value))};
}

// Min/max
template <ReferenceType Ref, typename Rep>
constexpr auto min(Quantity<Ref, Rep> a, Quantity<Ref, Rep> b) -> Quantity<Ref, Rep> {
  return a.value < b.value ? a : b;
}

template <ReferenceType Ref, typename Rep>
constexpr auto max(Quantity<Ref, Rep> a, Quantity<Ref, Rep> b) -> Quantity<Ref, Rep> {
  return a.value > b.value ? a : b;
}

// Clamp value to range
template <ReferenceType Ref, typename Rep>
constexpr auto clamp(Quantity<Ref, Rep> v, Quantity<Ref, Rep> lo, Quantity<Ref, Rep> hi)
    -> Quantity<Ref, Rep> {
  return min(max(v, lo), hi);
}

// Sign function: returns -1, 0, or 1 (dimensionless)
template <ReferenceType Ref, typename Rep>
constexpr auto sign(Quantity<Ref, Rep> q) -> int {
  return (q.value > Rep{0}) - (q.value < Rep{0});
}

// Linear interpolation: lerp(a, b, t) = a + t*(b-a)
template <ReferenceType Ref, typename Rep>
constexpr auto lerp(Quantity<Ref, Rep> a, Quantity<Ref, Rep> b, double t) -> Quantity<Ref, Rep> {
  return Quantity<Ref, Rep>{static_cast<Rep>(std::lerp(a.value, b.value, t))};
}

}  // namespace tempura
