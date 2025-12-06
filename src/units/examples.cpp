// Units Library Examples
//
// Compile: ninja -C build src/units/units_examples
// Run: ./build/src/units/units_examples

#include <iostream>

#include "units/derived_unit.h"
#include "units/literals.h"
#include "units/units.h"
#include "units/us.h"

using namespace tempura;
using namespace tempura::literals;
using namespace tempura::us;

// ============================================================================
// Example 1: Basic Quantity Operations
// ============================================================================

void basicOperations() {
  std::cout << "=== Basic Operations ===\n";

  // Create quantities using literals
  auto length = 5.0_m;
  auto width = 3.0_m;
  auto time = 2.0_s;

  // Same-unit arithmetic
  auto perimeter = 2.0 * (length + width);
  std::cout << "Perimeter: " << perimeter << "\n";

  // Multiplication creates new dimensions
  auto area = length * width;
  std::cout << "Area: " << area << "\n";

  // Division creates new dimensions
  auto speed = length / time;
  std::cout << "Speed: " << speed << "\n\n";
}

// ============================================================================
// Example 2: Unit Conversions
// ============================================================================

void unitConversions() {
  std::cout << "=== Unit Conversions ===\n";

  // Create a distance in kilometers
  auto distance_km = 42.195_km;  // Marathon distance
  std::cout << "Marathon: " << distance_km << "\n";

  // Convert to metres
  auto distance_m = distance_km.in<DefaultRef<Metre>>();
  std::cout << "In metres: " << distance_m << "\n";

  // Convert to miles (US units)
  auto distance_mi = distance_km.in<DefaultRef<Mile>>();
  std::cout << "In miles: " << distance_mi << "\n";

  // Time conversions
  auto duration = 2.5_h;
  auto duration_min = duration.in<DefaultRef<Minute>>();
  auto duration_s = duration.in<DefaultRef<Second>>();
  std::cout << "\n" << duration << " = " << duration_min << " = " << duration_s << "\n\n";
}

// ============================================================================
// Example 3: Speed and Velocity
// ============================================================================

void speedCalculations() {
  std::cout << "=== Speed Calculations ===\n";

  // Calculate speed
  auto distance = 100.0_km;
  auto time = 1.5_h;
  auto speed = distance / time;

  std::cout << "Distance: " << distance << "\n";
  std::cout << "Time: " << time << "\n";
  std::cout << "Speed: " << speed << "\n";

  // Convert km/h to m/s
  auto speed_ms = speed.in<DefaultRef<MetrePerSecond>>();
  std::cout << "Speed in m/s: " << speed_ms << "\n";

  // Speed of light
  constexpr auto c = 299792458.0_m / 1.0_s;
  std::cout << "\nSpeed of light: " << c << "\n\n";
}

// ============================================================================
// Example 4: Physics: Kinetic Energy
// ============================================================================

void kineticEnergy() {
  std::cout << "=== Kinetic Energy ===\n";

  // E = 0.5 * m * v²
  auto mass = 1000.0_kg;  // 1 tonne car
  auto velocity = 100.0_km / 1.0_h;

  // Convert velocity to m/s for SI coherent calculation
  auto v_ms = velocity.in<DefaultRef<MetrePerSecond>>();

  // Calculate kinetic energy: 0.5 * m * v * v
  auto energy = 0.5 * mass * v_ms * v_ms;

  std::cout << "Mass: " << mass << "\n";
  std::cout << "Velocity: " << velocity << " = " << v_ms << "\n";
  std::cout << "Kinetic energy: " << energy << "\n";

  // Convert to kilojoules
  auto energy_kj = energy.in<DefaultRef<Kilojoule>>();
  std::cout << "In kilojoules: " << energy_kj << "\n\n";
}

// ============================================================================
// Example 5: Force and Acceleration
// ============================================================================

void forceAndAcceleration() {
  std::cout << "=== Force and Acceleration ===\n";

  // F = m * a
  auto mass = 75.0_kg;
  auto acceleration = 9.81_m / (1.0_s * 1.0_s);

  auto force = mass * acceleration;
  std::cout << "Weight of " << mass << " person: " << force << "\n";

  // Convert to kilonewtons
  auto force_kn = force.in<DefaultRef<Kilonewton>>();
  std::cout << "In kilonewtons: " << force_kn << "\n\n";
}

// ============================================================================
// Example 6: Cross-Unit Arithmetic
// ============================================================================

void crossUnitArithmetic() {
  std::cout << "=== Cross-Unit Arithmetic ===\n";

  // Adding different units of same dimension
  auto km = 2.0_km;
  auto m = 500.0_m;

  auto total = km + m;  // Automatically uses common unit (metres)
  std::cout << km << " + " << m << " = " << total << "\n";

  // Works with time too
  auto hours = 1.0_h;
  auto minutes = 30.0_min;
  auto total_time = hours + minutes;
  std::cout << hours << " + " << minutes << " = " << total_time << "\n\n";
}

// ============================================================================
// Example 7: US Customary Units
// ============================================================================

void usUnits() {
  std::cout << "=== US Customary Units ===\n";

  // Length conversions
  auto height_ft = 6.0 * Foot{};
  auto height_m = height_ft.in<DefaultRef<Metre>>();
  std::cout << "Height: " << height_ft << " = " << height_m << "\n";

  // A mile in various units
  auto mile = 1.0 * Mile{};
  std::cout << "1 mile = " << mile.in<DefaultRef<Foot>>() << "\n";
  std::cout << "1 mile = " << mile.in<DefaultRef<Metre>>() << "\n";
  std::cout << "1 mile = " << mile.in<DefaultRef<Kilometre>>() << "\n";

  // Volume: cooking example
  auto cups = 2.0 * Cup{};
  auto ml = cups.in<DefaultRef<Millilitre>>();
  std::cout << "\n" << cups << " = " << ml << "\n";

  // Speed: mph to km/h
  auto speed_mph = 65.0 * Mile{} / (1.0 * Hour{});
  auto speed_kmh = speed_mph.in<DefaultRef<KilometrePerHour>>();
  std::cout << "\n65 mph = " << speed_kmh << "\n\n";
}

// ============================================================================
// Example 8: Derived Units with Symbols
// ============================================================================

void derivedUnits() {
  std::cout << "=== Derived Units ===\n";

  // DerivedUnit preserves structure and generates symbols
  using MperS = DerivedUnit<Metre, UnitPer<Second>>;
  using KMperH = DerivedUnit<Kilometre, UnitPer<Hour>>;
  using MperS2 = DerivedUnit<Metre, UnitPer<UnitPower<Second, 2>>>;

  std::cout << "m/s symbol: \"" << MperS::kSymbol.c_str() << "\"\n";
  std::cout << "km/h symbol: \"" << KMperH::kSymbol.c_str() << "\"\n";
  std::cout << "m/s² symbol: \"" << MperS2::kSymbol.c_str() << "\"\n\n";
}

// ============================================================================
// Example 9: Frequency and Inverse Units
// ============================================================================

void frequencyExample() {
  std::cout << "=== Frequency (Inverse Units) ===\n";

  // Period to frequency
  auto period = 0.001_s;  // 1 millisecond
  auto frequency = 1.0 / period;

  std::cout << "Period: " << period << "\n";
  std::cout << "Frequency: " << frequency << "\n";

  // Convert to named unit
  auto freq_hz = frequency.in<DefaultRef<Hertz>>();
  auto freq_khz = frequency.in<DefaultRef<Kilohertz>>();
  std::cout << "In Hz: " << freq_hz << "\n";
  std::cout << "In kHz: " << freq_khz << "\n\n";
}

// ============================================================================
// Example 10: Approximate Equality
// ============================================================================

void approximateEquality() {
  std::cout << "=== Approximate Equality ===\n";

  auto a = 1.0_km;
  auto b = 1000.0_m;
  auto c = 1000.001_m;

  std::cout << "1 km == 1000 m (exact): " << (a == b ? "true" : "false") << "\n";
  std::cout << "1 km == 1000.001 m (exact): " << (a == c ? "true" : "false") << "\n";
  std::cout << "approximateEqual(1 km, 1000.001 m, 1e-5): "
            << (approximateEqual(a, c, 1e-5) ? "true" : "false") << "\n\n";
}

// ============================================================================
// Example 11: Angles
// ============================================================================

void angleExample() {
  std::cout << "=== Angles ===\n";

  // Degrees to radians
  auto angle_deg = 180.0 * Degree{};
  auto angle_rad = angle_deg.in<DefaultRef<Radian>>();

  std::cout << angle_deg << " = " << angle_rad << "\n";

  // Full turn
  auto full_turn = 1.0 * Turn{};
  std::cout << "1 turn = " << full_turn.in<DefaultRef<Degree>>() << "\n";
  std::cout << "1 turn = " << full_turn.in<DefaultRef<Radian>>() << "\n\n";
}

// ============================================================================
// Example 12: Pressure
// ============================================================================

void pressureExample() {
  std::cout << "=== Pressure ===\n";

  // Atmospheric pressure
  auto atm_pa = 101325.0 * Pascal{};
  auto atm_kpa = atm_pa.in<DefaultRef<Kilopascal>>();
  auto atm_bar = atm_pa.in<DefaultRef<Bar>>();

  std::cout << "Atmospheric pressure:\n";
  std::cout << "  " << atm_pa << "\n";
  std::cout << "  " << atm_kpa << "\n";
  std::cout << "  " << atm_bar << "\n\n";
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  basicOperations();
  unitConversions();
  speedCalculations();
  kineticEnergy();
  forceAndAcceleration();
  crossUnitArithmetic();
  usUnits();
  derivedUnits();
  frequencyExample();
  approximateEquality();
  angleExample();
  pressureExample();

  std::cout << "All examples completed!\n";
  return 0;
}
