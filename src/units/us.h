#pragma once

#include "units/unit_type.h"

// US Customary Units
//
// All definitions use exact conversion factors based on international agreements:
// - 1 inch = 25.4 mm exactly (International Yard and Pound Agreement, 1959)
// - 1 pound = 0.45359237 kg exactly
// - 1 US gallon = 231 cubic inches exactly
//
// Using prime factorization for lossless arithmetic:
// - 127/5000 = 127 × 2⁻³ × 5⁻⁴ (inch in metres)
// - No overflow even for extreme conversions

namespace tempura::us {

// ============================================================================
// Length Units
// ============================================================================
//
// 1 inch = 25.4 mm = 127/5000 m = 127 × 2⁻³ × 5⁻⁴ m
// 127 is prime, 5000 = 2³ × 5⁴

using Inch = Unit<QtyLength,
                  Magnitude<PrimePow<2, -3>, PrimePow<5, -4>, PrimePow<127, 1>>,
                  "in">;

// 1 foot = 12 inches = 12 × 127/5000 m
// 12 = 2² × 3, so: 2⁻¹ × 3 × 5⁻⁴ × 127
using Foot = Unit<QtyLength,
                  Magnitude<PrimePow<2, -1>, PrimePow<3, 1>, PrimePow<5, -4>, PrimePow<127, 1>>,
                  "ft">;

// 1 yard = 3 feet = 36 inches
// 36 = 2² × 3², so: 2⁻¹ × 3² × 5⁻⁴ × 127
using Yard = Unit<QtyLength,
                  Magnitude<PrimePow<2, -1>, PrimePow<3, 2>, PrimePow<5, -4>, PrimePow<127, 1>>,
                  "yd">;

// 1 mile = 5280 feet = 63360 inches
// 63360 = 2⁷ × 3² × 5 × 11, so: 2⁴ × 3² × 5⁻³ × 11 × 127
using Mile = Unit<QtyLength,
                  Magnitude<PrimePow<2, 4>, PrimePow<3, 2>, PrimePow<5, -3>, PrimePow<11, 1>, PrimePow<127, 1>>,
                  "mi">;

// ============================================================================
// Mass Units
// ============================================================================
//
// 1 pound (avoirdupois) = 0.45359237 kg exactly
// 45359237/100000000 = 45359237 × 2⁻⁸ × 5⁻⁸
//
// Note: 45359237 = 45359237 (kept as single factor for simplicity;
// its prime factorization is complex and doesn't simplify conversions)

using Pound = Unit<QtyMass,
                   Magnitude<PrimePow<2, -8>, PrimePow<5, -8>, PrimePow<45359237, 1>>,
                   "lb">;

// 1 ounce = 1/16 pound
// 16 = 2⁴, so: 2⁻¹² × 5⁻⁸ × 45359237
using Ounce = Unit<QtyMass,
                   Magnitude<PrimePow<2, -12>, PrimePow<5, -8>, PrimePow<45359237, 1>>,
                   "oz">;

// 1 short ton = 2000 pounds
// 2000 = 2⁴ × 5³, so: 2⁻⁴ × 5⁻⁵ × 45359237
using ShortTon = Unit<QtyMass,
                      Magnitude<PrimePow<2, -4>, PrimePow<5, -5>, PrimePow<45359237, 1>>,
                      "ton">;

// ============================================================================
// Volume Units (US Liquid)
// ============================================================================
//
// 1 US gallon = 231 cubic inches (exactly)
// 1 in³ = (127/5000)³ m³ = 127³ × 2⁻⁹ × 5⁻¹² m³
// 231 = 3 × 7 × 11
// So: gallon = 3 × 7 × 11 × 127³ × 2⁻⁹ × 5⁻¹² m³

using Gallon = Unit<QtyVolume,
                    Magnitude<PrimePow<2, -9>, PrimePow<3, 1>, PrimePow<5, -12>,
                              PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                    "gal">;

// 1 quart = 1/4 gallon
// 4 = 2², so: 2⁻¹¹ × 3 × 5⁻¹² × 7 × 11 × 127³
using Quart = Unit<QtyVolume,
                   Magnitude<PrimePow<2, -11>, PrimePow<3, 1>, PrimePow<5, -12>,
                             PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                   "qt">;

// 1 pint = 1/2 quart = 1/8 gallon
// 8 = 2³, so: 2⁻¹² × 3 × 5⁻¹² × 7 × 11 × 127³
using Pint = Unit<QtyVolume,
                  Magnitude<PrimePow<2, -12>, PrimePow<3, 1>, PrimePow<5, -12>,
                            PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                  "pt">;

// 1 cup = 1/2 pint = 1/16 gallon
// 16 = 2⁴, so: 2⁻¹³ × 3 × 5⁻¹² × 7 × 11 × 127³
using Cup = Unit<QtyVolume,
                 Magnitude<PrimePow<2, -13>, PrimePow<3, 1>, PrimePow<5, -12>,
                           PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                 "cup">;

// 1 fluid ounce = 1/8 cup = 1/128 gallon
// 128 = 2⁷, so: 2⁻¹⁶ × 3 × 5⁻¹² × 7 × 11 × 127³
using FluidOunce = Unit<QtyVolume,
                        Magnitude<PrimePow<2, -16>, PrimePow<3, 1>, PrimePow<5, -12>,
                                  PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                        "fl oz">;

// 1 tablespoon = 1/2 fluid ounce = 1/256 gallon
// 256 = 2⁸, so: 2⁻¹⁷ × 3 × 5⁻¹² × 7 × 11 × 127³
using Tablespoon = Unit<QtyVolume,
                        Magnitude<PrimePow<2, -17>, PrimePow<3, 1>, PrimePow<5, -12>,
                                  PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                        "tbsp">;

// 1 teaspoon = 1/3 tablespoon = 1/768 gallon
// 768 = 2⁸ × 3, so: 2⁻¹⁷ × 5⁻¹² × 7 × 11 × 127³ (3 cancels!)
using Teaspoon = Unit<QtyVolume,
                      Magnitude<PrimePow<2, -17>, PrimePow<5, -12>,
                                PrimePow<7, 1>, PrimePow<11, 1>, PrimePow<127, 3>>,
                      "tsp">;

// ============================================================================
// Speed Units
// ============================================================================

// Miles per hour
using MilePerHour = Unit<QtySpeed,
                         MagDivide<typename Mile::Magnitude, Mag3600>,
                         "mph">;

// Feet per second
using FootPerSecond = Unit<QtySpeed,
                           typename Foot::Magnitude,
                           "ft/s">;

}  // namespace tempura::us
