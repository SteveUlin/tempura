#pragma once

#include "units/quantity.h"

// Literal operators for convenient quantity construction
//
// Usage:
//   using namespace tempura::literals;
//   auto distance = 100.0_km;
//   auto time = 2.0_h;
//   auto speed = distance / time;  // 50 km/h

namespace tempura::literals {

// ============================================================================
// Length literals
// ============================================================================

constexpr auto operator""_m(long double v) {
  return Quantity<DefaultRef<Metre>, double>{static_cast<double>(v)};
}

constexpr auto operator""_m(unsigned long long v) {
  return Quantity<DefaultRef<Metre>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_km(long double v) {
  return Quantity<DefaultRef<Kilometre>, double>{static_cast<double>(v)};
}

constexpr auto operator""_km(unsigned long long v) {
  return Quantity<DefaultRef<Kilometre>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_cm(long double v) {
  return Quantity<DefaultRef<Centimetre>, double>{static_cast<double>(v)};
}

constexpr auto operator""_cm(unsigned long long v) {
  return Quantity<DefaultRef<Centimetre>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_mm(long double v) {
  return Quantity<DefaultRef<Millimetre>, double>{static_cast<double>(v)};
}

constexpr auto operator""_mm(unsigned long long v) {
  return Quantity<DefaultRef<Millimetre>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Time literals
// ============================================================================

constexpr auto operator""_s(long double v) {
  return Quantity<DefaultRef<Second>, double>{static_cast<double>(v)};
}

constexpr auto operator""_s(unsigned long long v) {
  return Quantity<DefaultRef<Second>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_ms(long double v) {
  return Quantity<DefaultRef<Millisecond>, double>{static_cast<double>(v)};
}

constexpr auto operator""_ms(unsigned long long v) {
  return Quantity<DefaultRef<Millisecond>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_us(long double v) {
  return Quantity<DefaultRef<Microsecond>, double>{static_cast<double>(v)};
}

constexpr auto operator""_us(unsigned long long v) {
  return Quantity<DefaultRef<Microsecond>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_ns(long double v) {
  return Quantity<DefaultRef<Nanosecond>, double>{static_cast<double>(v)};
}

constexpr auto operator""_ns(unsigned long long v) {
  return Quantity<DefaultRef<Nanosecond>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_min(long double v) {
  return Quantity<DefaultRef<Minute>, double>{static_cast<double>(v)};
}

constexpr auto operator""_min(unsigned long long v) {
  return Quantity<DefaultRef<Minute>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_h(long double v) {
  return Quantity<DefaultRef<Hour>, double>{static_cast<double>(v)};
}

constexpr auto operator""_h(unsigned long long v) {
  return Quantity<DefaultRef<Hour>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Mass literals
// ============================================================================

constexpr auto operator""_kg(long double v) {
  return Quantity<DefaultRef<Kilogram>, double>{static_cast<double>(v)};
}

constexpr auto operator""_kg(unsigned long long v) {
  return Quantity<DefaultRef<Kilogram>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_g(long double v) {
  return Quantity<DefaultRef<Gram>, double>{static_cast<double>(v)};
}

constexpr auto operator""_g(unsigned long long v) {
  return Quantity<DefaultRef<Gram>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_mg(long double v) {
  return Quantity<DefaultRef<Milligram>, double>{static_cast<double>(v)};
}

constexpr auto operator""_mg(unsigned long long v) {
  return Quantity<DefaultRef<Milligram>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_t(long double v) {
  return Quantity<DefaultRef<Tonne>, double>{static_cast<double>(v)};
}

constexpr auto operator""_t(unsigned long long v) {
  return Quantity<DefaultRef<Tonne>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Force literals
// ============================================================================

constexpr auto operator""_N(long double v) {
  return Quantity<DefaultRef<Newton>, double>{static_cast<double>(v)};
}

constexpr auto operator""_N(unsigned long long v) {
  return Quantity<DefaultRef<Newton>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_kN(long double v) {
  return Quantity<DefaultRef<Kilonewton>, double>{static_cast<double>(v)};
}

constexpr auto operator""_kN(unsigned long long v) {
  return Quantity<DefaultRef<Kilonewton>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Energy literals
// ============================================================================

constexpr auto operator""_J(long double v) {
  return Quantity<DefaultRef<Joule>, double>{static_cast<double>(v)};
}

constexpr auto operator""_J(unsigned long long v) {
  return Quantity<DefaultRef<Joule>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_kJ(long double v) {
  return Quantity<DefaultRef<Kilojoule>, double>{static_cast<double>(v)};
}

constexpr auto operator""_kJ(unsigned long long v) {
  return Quantity<DefaultRef<Kilojoule>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Power literals
// ============================================================================

constexpr auto operator""_W(long double v) {
  return Quantity<DefaultRef<Watt>, double>{static_cast<double>(v)};
}

constexpr auto operator""_W(unsigned long long v) {
  return Quantity<DefaultRef<Watt>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_kW(long double v) {
  return Quantity<DefaultRef<Kilowatt>, double>{static_cast<double>(v)};
}

constexpr auto operator""_kW(unsigned long long v) {
  return Quantity<DefaultRef<Kilowatt>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_MW(long double v) {
  return Quantity<DefaultRef<Megawatt>, double>{static_cast<double>(v)};
}

constexpr auto operator""_MW(unsigned long long v) {
  return Quantity<DefaultRef<Megawatt>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Pressure literals
// ============================================================================

constexpr auto operator""_Pa(long double v) {
  return Quantity<DefaultRef<Pascal>, double>{static_cast<double>(v)};
}

constexpr auto operator""_Pa(unsigned long long v) {
  return Quantity<DefaultRef<Pascal>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_kPa(long double v) {
  return Quantity<DefaultRef<Kilopascal>, double>{static_cast<double>(v)};
}

constexpr auto operator""_kPa(unsigned long long v) {
  return Quantity<DefaultRef<Kilopascal>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_bar(long double v) {
  return Quantity<DefaultRef<Bar>, double>{static_cast<double>(v)};
}

constexpr auto operator""_bar(unsigned long long v) {
  return Quantity<DefaultRef<Bar>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Frequency literals
// ============================================================================

constexpr auto operator""_Hz(long double v) {
  return Quantity<DefaultRef<Hertz>, double>{static_cast<double>(v)};
}

constexpr auto operator""_Hz(unsigned long long v) {
  return Quantity<DefaultRef<Hertz>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_kHz(long double v) {
  return Quantity<DefaultRef<Kilohertz>, double>{static_cast<double>(v)};
}

constexpr auto operator""_kHz(unsigned long long v) {
  return Quantity<DefaultRef<Kilohertz>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_MHz(long double v) {
  return Quantity<DefaultRef<Megahertz>, double>{static_cast<double>(v)};
}

constexpr auto operator""_MHz(unsigned long long v) {
  return Quantity<DefaultRef<Megahertz>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_GHz(long double v) {
  return Quantity<DefaultRef<Gigahertz>, double>{static_cast<double>(v)};
}

constexpr auto operator""_GHz(unsigned long long v) {
  return Quantity<DefaultRef<Gigahertz>, long long>{static_cast<long long>(v)};
}

// ============================================================================
// Angle literals
// ============================================================================

constexpr auto operator""_rad(long double v) {
  return Quantity<DefaultRef<Radian>, double>{static_cast<double>(v)};
}

constexpr auto operator""_rad(unsigned long long v) {
  return Quantity<DefaultRef<Radian>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_deg(long double v) {
  return Quantity<DefaultRef<Degree>, double>{static_cast<double>(v)};
}

constexpr auto operator""_deg(unsigned long long v) {
  return Quantity<DefaultRef<Degree>, long long>{static_cast<long long>(v)};
}

constexpr auto operator""_turn(long double v) {
  return Quantity<DefaultRef<Turn>, double>{static_cast<double>(v)};
}

constexpr auto operator""_turn(unsigned long long v) {
  return Quantity<DefaultRef<Turn>, long long>{static_cast<long long>(v)};
}

}  // namespace tempura::literals
