# Units Library

A compile-time dimensional analysis and physical units library for C++26.

## Quick Start

```cpp
#include "units/units.h"
using namespace tempura;
using namespace tempura::literals;

auto distance = 100.0_km;
auto time = 2.0_h;
auto speed = distance / time;                          // 50 km/h
auto speed_ms = speed.in<DefaultRef<MetrePerSecond>>(); // ~13.89 m/s
```

## Project Structure

```
units/
├── dimension.h        # Compile-time dimensional analysis (L, T, M, etc.)
├── magnitude.h        # Symbolic scale factors (prime factorization, π)
├── quantity_spec.h    # Semantic quantity types with hierarchy
├── unit_type.h        # Named units with magnitude and symbol
├── reference.h        # Binds QuantitySpec to Unit
├── quantity.h         # Runtime value with compile-time unit info
├── derived_unit.h     # Compound units preserving structure (km/h)
├── literals.h         # User-defined literals (_m, _s, _kg, etc.)
├── us.h               # US customary units (feet, pounds, gallons)
└── units.h            # Umbrella header
```

## Reading Order

The library builds in layers. Read in this order to understand the design:

### Layer 1: Foundations
1. **`dimension.h`** - Start here. Defines the 7 SI base dimensions and how they combine.
   - `DimLength`, `DimTime`, `DimMass`, etc.
   - `Dimension<...>` template for compound dimensions
   - `Per<...>` for denominators, `Power<D, N>` for exponents

2. **`magnitude.h`** - Scale factors as prime factorizations.
   - `PrimePow<P, N>` for P^N (e.g., `PrimePow<2, 3>` = 8)
   - `PiPow<N>` for π^N
   - `Magnitude<...>` combines factors (e.g., 1000 = 2³ × 5³)
   - Prevents overflow for extreme scales (10²⁴, 10⁻²⁴)

### Layer 2: Quantity Types
3. **`quantity_spec.h`** - Semantic meaning of quantities.
   - `QtyLength`, `QtyTime`, `QtySpeed`, etc.
   - Hierarchy support: `QtyHeight` is-a `QtyLength`
   - Algebraic operations: `QtyMultiply`, `QtyDivide`, `QtyPow`

4. **`unit_type.h`** - Named measurement units.
   - `Unit<QuantitySpec, Magnitude, Symbol>` template
   - SI base units: `Metre`, `Second`, `Kilogram`, etc.
   - SI prefixes: `Kilo<U>`, `Milli<U>`, etc.
   - Derived units: `Newton`, `Joule`, `Hertz`, etc.

### Layer 3: Binding & Values
5. **`reference.h`** - Connects QuantitySpec to Unit.
   - `Reference<QuantitySpec, Unit>` template
   - `DefaultRef<U>` creates reference from unit
   - Enables type-safe conversions

6. **`quantity.h`** - Runtime values with units. **The main user-facing type.**
   - `Quantity<Reference, Rep>` stores value + unit info
   - Arithmetic: `+`, `-`, `*`, `/` with dimensional correctness
   - Conversion: `.in<TargetRef>()`, `.valueIn<TargetRef>()`
   - Comparison: exact `==` and `approximateEqual()`

### Layer 4: Extensions
7. **`derived_unit.h`** - Compound units preserving structure.
   - `DerivedUnit<Metre, UnitPer<Second>>` for m/s
   - Symbol generation: "km/h", "m/s²"
   - `UnitMultiply`, `UnitDivide` type aliases

8. **`literals.h`** - Convenient syntax.
   - `5.0_m`, `10_s`, `2.5_kg`, etc.
   - Makes code readable: `auto force = 10.0_kg * 9.8_m / (1.0_s * 1.0_s);`

9. **`us.h`** - US customary units.
   - Length: `Foot`, `Inch`, `Mile`, `Yard`
   - Mass: `Pound`, `Ounce`
   - Volume: `Gallon`, `Quart`, `Pint`, `Cup`, `FluidOunce`

## Key Concepts

### Type Safety
Incompatible dimensions cause compile errors:
```cpp
auto length = 5.0_m;
auto time = 2.0_s;
// auto bad = length + time;  // ERROR: different dimensions
auto speed = length / time;   // OK: creates velocity
```

### Automatic Conversion
Compatible units convert automatically:
```cpp
auto km = 1.0_km;
auto m = 500.0_m;
auto sum = km + m;  // 1500.0 m (common unit selected)
```

### Symbolic Magnitudes
Scale factors stay exact until final evaluation:
```cpp
// Internally: Mag1000 = Magnitude<PrimePow<2, 3>, PrimePow<5, 3>>
// 1 km = 1000 m exactly, no floating-point drift
```

## Tests

Each header has a corresponding `*_test.cpp` file demonstrating usage:
```bash
ninja -C build src/units/units_quantity_test
./build/src/units/units_quantity_test
```
