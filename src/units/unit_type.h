#pragma once

#include <ostream>

#include "meta/fixed_string.h"
#include "units/magnitude.h"
#include "units/quantity_spec.h"

// Unit - Named measurement units with symbolic magnitude and symbol
//
// A Unit associates a QuantitySpec with a scale factor (magnitude) relative to
// the coherent SI unit, plus a human-readable symbol.
//
// Examples:
//   using Metre = Unit<QtyLength, MagOne, "m">;
//   using Kilometre = Unit<QtyLength, Mag1000, "km">;
//   using Hour = Unit<QtyTime, Mag3600, "h">;

namespace tempura {

// ============================================================================
// Unit template
// ============================================================================

template <QuantitySpecType Spec, MagnitudeType Mag, FixedString Symbol>
struct Unit {
  using QuantitySpec = Spec;
  using Magnitude = Mag;
  static constexpr auto kMagnitude = Mag::value();  // Runtime double value
  static constexpr auto kSymbol = Symbol;
};

// ============================================================================
// UnitType concept
// ============================================================================

template <typename T>
concept UnitType = requires {
  typename T::QuantitySpec;
  typename T::Magnitude;
  { T::kMagnitude } -> SameAs<const double&>;
  { T::kSymbol };
  requires QuantitySpecType<typename T::QuantitySpec>;
  requires MagnitudeType<typename T::Magnitude>;
};

// ============================================================================
// Unit compatibility - same dimension
// ============================================================================

template <UnitType U1, UnitType U2>
constexpr bool kCompatibleUnits =
    isSame<typename U1::QuantitySpec::Dimension,
           typename U2::QuantitySpec::Dimension>;

// ============================================================================
// Conversion factor between compatible units
// ============================================================================

// ConversionFactor<From, To> = From::Magnitude / To::Magnitude
// Represents how many "To" units equal one "From" unit
template <UnitType From, UnitType To>
  requires kCompatibleUnits<From, To>
using ConversionMagnitude = MagDivide<typename From::Magnitude, typename To::Magnitude>;

template <UnitType From, UnitType To>
  requires kCompatibleUnits<From, To>
constexpr double kConversionFactor = ConversionMagnitude<From, To>::value();

// ============================================================================
// SI Prefixes - template aliases that scale any unit
// ============================================================================

// Large prefixes
template <UnitType U>
using Quetta = Unit<typename U::QuantitySpec,
                    MagMultiply<Magnitude<PrimePow<2, 30>, PrimePow<5, 30>>, typename U::Magnitude>,
                    FixedString{"Q"} + U::kSymbol>;

template <UnitType U>
using Ronna = Unit<typename U::QuantitySpec,
                   MagMultiply<Magnitude<PrimePow<2, 27>, PrimePow<5, 27>>, typename U::Magnitude>,
                   FixedString{"R"} + U::kSymbol>;

template <UnitType U>
using Yotta = Unit<typename U::QuantitySpec,
                   MagMultiply<Magnitude<PrimePow<2, 24>, PrimePow<5, 24>>, typename U::Magnitude>,
                   FixedString{"Y"} + U::kSymbol>;

template <UnitType U>
using Zetta = Unit<typename U::QuantitySpec,
                   MagMultiply<Magnitude<PrimePow<2, 21>, PrimePow<5, 21>>, typename U::Magnitude>,
                   FixedString{"Z"} + U::kSymbol>;

template <UnitType U>
using Exa = Unit<typename U::QuantitySpec,
                 MagMultiply<Magnitude<PrimePow<2, 18>, PrimePow<5, 18>>, typename U::Magnitude>,
                 FixedString{"E"} + U::kSymbol>;

template <UnitType U>
using Peta = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, 15>, PrimePow<5, 15>>, typename U::Magnitude>,
                  FixedString{"P"} + U::kSymbol>;

template <UnitType U>
using Tera = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, 12>, PrimePow<5, 12>>, typename U::Magnitude>,
                  FixedString{"T"} + U::kSymbol>;

template <UnitType U>
using Giga = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, 9>, PrimePow<5, 9>>, typename U::Magnitude>,
                  FixedString{"G"} + U::kSymbol>;

template <UnitType U>
using Mega = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, 6>, PrimePow<5, 6>>, typename U::Magnitude>,
                  FixedString{"M"} + U::kSymbol>;

template <UnitType U>
using Kilo = Unit<typename U::QuantitySpec,
                  MagMultiply<Mag1000, typename U::Magnitude>,
                  FixedString{"k"} + U::kSymbol>;

template <UnitType U>
using Hecto = Unit<typename U::QuantitySpec,
                   MagMultiply<Mag100, typename U::Magnitude>,
                   FixedString{"h"} + U::kSymbol>;

template <UnitType U>
using Deca = Unit<typename U::QuantitySpec,
                  MagMultiply<Mag10, typename U::Magnitude>,
                  FixedString{"da"} + U::kSymbol>;

// Small prefixes
template <UnitType U>
using Deci = Unit<typename U::QuantitySpec,
                  MagMultiply<MagTenth, typename U::Magnitude>,
                  FixedString{"d"} + U::kSymbol>;

template <UnitType U>
using Centi = Unit<typename U::QuantitySpec,
                   MagMultiply<MagHundredth, typename U::Magnitude>,
                   FixedString{"c"} + U::kSymbol>;

template <UnitType U>
using Milli = Unit<typename U::QuantitySpec,
                   MagMultiply<MagThousandth, typename U::Magnitude>,
                   FixedString{"m"} + U::kSymbol>;

template <UnitType U>
using Micro = Unit<typename U::QuantitySpec,
                   MagMultiply<Magnitude<PrimePow<2, -6>, PrimePow<5, -6>>, typename U::Magnitude>,
                   FixedString{"μ"} + U::kSymbol>;

template <UnitType U>
using Nano = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, -9>, PrimePow<5, -9>>, typename U::Magnitude>,
                  FixedString{"n"} + U::kSymbol>;

template <UnitType U>
using Pico = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, -12>, PrimePow<5, -12>>, typename U::Magnitude>,
                  FixedString{"p"} + U::kSymbol>;

template <UnitType U>
using Femto = Unit<typename U::QuantitySpec,
                   MagMultiply<Magnitude<PrimePow<2, -15>, PrimePow<5, -15>>, typename U::Magnitude>,
                   FixedString{"f"} + U::kSymbol>;

template <UnitType U>
using Atto = Unit<typename U::QuantitySpec,
                  MagMultiply<Magnitude<PrimePow<2, -18>, PrimePow<5, -18>>, typename U::Magnitude>,
                  FixedString{"a"} + U::kSymbol>;

// ============================================================================
// SI Base Units
// ============================================================================

// Length
using Metre = Unit<QtyLength, MagOne, "m">;
using Kilometre = Unit<QtyLength, Mag1000, "km">;
using Centimetre = Unit<QtyLength, MagHundredth, "cm">;
using Millimetre = Unit<QtyLength, MagThousandth, "mm">;
using Micrometre = Unit<QtyLength, Magnitude<PrimePow<2, -6>, PrimePow<5, -6>>, "μm">;
using Nanometre = Unit<QtyLength, Magnitude<PrimePow<2, -9>, PrimePow<5, -9>>, "nm">;

// Time
using Second = Unit<QtyTime, MagOne, "s">;
using Millisecond = Unit<QtyTime, MagThousandth, "ms">;
using Microsecond = Unit<QtyTime, Magnitude<PrimePow<2, -6>, PrimePow<5, -6>>, "μs">;
using Nanosecond = Unit<QtyTime, Magnitude<PrimePow<2, -9>, PrimePow<5, -9>>, "ns">;
using Minute = Unit<QtyTime, Mag60, "min">;
using Hour = Unit<QtyTime, Mag3600, "h">;
using Day = Unit<QtyTime, Magnitude<PrimePow<2, 7>, PrimePow<3, 3>, PrimePow<5, 2>>, "d">;  // 86400 = 2^7 * 3^3 * 5^2

// Mass
using Kilogram = Unit<QtyMass, MagOne, "kg">;
using Gram = Unit<QtyMass, MagThousandth, "g">;
using Milligram = Unit<QtyMass, Magnitude<PrimePow<2, -6>, PrimePow<5, -6>>, "mg">;
using Tonne = Unit<QtyMass, Mag1000, "t">;

// Electric current
using Ampere = Unit<QtyElectricCurrent, MagOne, "A">;
using Milliampere = Unit<QtyElectricCurrent, MagThousandth, "mA">;

// Temperature
using Kelvin = Unit<QtyTemperature, MagOne, "K">;

// Amount of substance
using Mole = Unit<QtyAmountOfSubstance, MagOne, "mol">;

// Luminous intensity
using Candela = Unit<QtyLuminousIntensity, MagOne, "cd">;

// ============================================================================
// Common Derived Units
// ============================================================================

// Area
using SquareMetre = Unit<QtyArea, MagOne, "m2">;
using SquareKilometre = Unit<QtyArea, MagMillion, "km2">;
using SquareCentimetre = Unit<QtyArea, Magnitude<PrimePow<2, -4>, PrimePow<5, -4>>, "cm2">;  // 10^-4

// Volume
using CubicMetre = Unit<QtyVolume, MagOne, "m3">;
using Litre = Unit<QtyVolume, MagThousandth, "L">;
using Millilitre = Unit<QtyVolume, Magnitude<PrimePow<2, -6>, PrimePow<5, -6>>, "mL">;

// Speed
using MetrePerSecond = Unit<QtySpeed, MagOne, "m/s">;
// km/h = 1000/3600 = 5/18 = 5 / (2 * 3^2) = 5 * 2^-1 * 3^-2
using KilometrePerHour = Unit<QtySpeed, Magnitude<PrimePow<2, -1>, PrimePow<3, -2>, PrimePow<5, 1>>, "km/h">;

// Acceleration
using MetrePerSecondSquared = Unit<QtyAcceleration, MagOne, "m/s2">;

// Force
using Newton = Unit<QtyForce, MagOne, "N">;
using Kilonewton = Unit<QtyForce, Mag1000, "kN">;

// Energy
using Joule = Unit<QtyEnergy, MagOne, "J">;
using Kilojoule = Unit<QtyEnergy, Mag1000, "kJ">;
using Megajoule = Unit<QtyEnergy, MagMillion, "MJ">;

// Power
using Watt = Unit<QtyPower, MagOne, "W">;
using Kilowatt = Unit<QtyPower, Mag1000, "kW">;
using Megawatt = Unit<QtyPower, MagMillion, "MW">;

// Pressure
using Pascal = Unit<QtyPressure, MagOne, "Pa">;
using Kilopascal = Unit<QtyPressure, Mag1000, "kPa">;
using Megapascal = Unit<QtyPressure, MagMillion, "MPa">;
// Bar = 100000 = 10^5 = 2^5 * 5^5
using Bar = Unit<QtyPressure, Magnitude<PrimePow<2, 5>, PrimePow<5, 5>>, "bar">;

// Frequency
using Hertz = Unit<QtyFrequency, MagOne, "Hz">;
using Kilohertz = Unit<QtyFrequency, Mag1000, "kHz">;
using Megahertz = Unit<QtyFrequency, MagMillion, "MHz">;
using Gigahertz = Unit<QtyFrequency, MagBillion, "GHz">;

// ============================================================================
// Angle units (using symbolic Pi!)
// ============================================================================

using Radian = Unit<QtyAngle, MagOne, "rad">;
// Degree = π/180 rad
using Degree = Unit<QtyAngle, MagPiOver180, "deg">;
// Turn = 2π rad
using Turn = Unit<QtyAngle, MagTwoPi, "turn">;

// ============================================================================
// Dimensionless unit
// ============================================================================

using One = Unit<QtyDimensionless, MagOne, "1">;
using Percent = Unit<QtyDimensionless, MagHundredth, "%">;

// ============================================================================
// Pretty printing
// ============================================================================

template <UnitType U>
auto operator<<(std::ostream& os, U /*unit*/) -> std::ostream& {
  os << U::kSymbol.c_str();
  return os;
}

}  // namespace tempura
