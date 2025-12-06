#pragma once

// Units Library - Complete physical units system
//
// This umbrella header includes all units library components:
// - Dimension: Compile-time dimensional analysis
// - QuantitySpec: Semantic quantity identifiers with hierarchy
// - Unit: Named units with magnitude and symbol
// - Reference: QuantitySpec + Unit binding
// - Quantity: Runtime value with compile-time unit information
// - Literals: Convenient literal operators (_m, _s, _kg, etc.)
//
// Basic usage:
//   #include "units/units.h"
//   using namespace tempura;
//   using namespace tempura::literals;
//
//   auto distance = 100.0_km;
//   auto time = 2.0_h;
//   auto speed = distance / time;  // 50 km/h
//   auto speed_ms = speed.in<DefaultRef<MetrePerSecond>>();

#include "units/dimension.h"
#include "units/literals.h"
#include "units/math.h"
#include "units/quantity.h"
#include "units/quantity_spec.h"
#include "units/reference.h"
#include "units/unit_type.h"
