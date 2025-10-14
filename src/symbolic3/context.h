#pragma once

#include <numbers>  // For mathematical constants (pi, e) - compile-time only

#include "meta/utility.h"

// Data-driven context for transformation strategies
// Context carries WHAT data/mode to use, not HOW to behave
//
// NOTE: This file minimally uses <numbers> for mathematical constants (π, e)
// which are compile-time values. This is acceptable as they don't impact
// compile times significantly.

namespace tempura::symbolic3 {

// ============================================================================
// Domain Configuration
// ============================================================================

// Numeric domain for operations
enum class Domain {
  Real,              // Real numbers
  Complex,           // Complex numbers
  Integer,           // Integer arithmetic
  Boolean,           // Boolean algebra
  ModularArithmetic  // Modulo arithmetic
};

// Modular domain configuration
template <auto Modulus>
struct ModularDomain {
  static constexpr auto modulus = Modulus;
};

// Angle domain (for trig functions)
template <auto Period = std::numbers::pi * 2>
struct AngleDomain {
  static constexpr auto period = Period;  // e.g., 2π, 360°
};

// ============================================================================
// Simplification Modes (what operations to perform)
// ============================================================================

struct SimplificationMode {
  // Constant folding options
  bool fold_numeric_constants = true;    // 2 + 3 → 5
  bool fold_symbolic_constants = false;  // π/6 → 0.523... or keep symbolic?
  bool fold_algebraic = true;            // x + 0 → x, x * 1 → x

  // Special value handling
  bool preserve_special_values = true;   // Keep π, e, √2 symbolic
  bool preserve_exact_rationals = true;  // Keep 1/3 vs 0.333...

  // Numeric precision
  bool prefer_exact = true;  // Exact over approximate
  int float_precision = 15;  // Digits for float folding
};

// ============================================================================
// Transformation Context
// ============================================================================

template <SizeT Depth = 0, Domain DomainType = Domain::Real,
          typename AngleConfig = AngleDomain<>, typename ModConfig = void>
struct TransformContext {
  static constexpr SizeT depth = Depth;
  static constexpr Domain domain = DomainType;

  // Data: what mode to operate in
  SimplificationMode mode{};

  // Depth tracking (for recursion limits)
  template <SizeT N>
  constexpr auto increment_depth() const {
    TransformContext<Depth + N, DomainType, AngleConfig, ModConfig> result;
    result.mode = mode;
    return result;
  }

  constexpr auto reset_depth() const {
    TransformContext<0, DomainType, AngleConfig, ModConfig> result;
    result.mode = mode;
    return result;
  }

  // Mode modification (returns new context with updated mode)
  constexpr auto with_mode(SimplificationMode new_mode) const {
    TransformContext<Depth, DomainType, AngleConfig, ModConfig> result;
    result.mode = new_mode;
    return result;
  }

  // Convenience: disable specific operations
  constexpr auto without_constant_folding() const {
    auto new_mode = mode;
    new_mode.fold_numeric_constants = false;
    new_mode.fold_symbolic_constants = false;
    return with_mode(new_mode);
  }

  constexpr auto with_symbolic_constants() const {
    auto new_mode = mode;
    new_mode.fold_symbolic_constants = false;
    new_mode.preserve_special_values = true;
    return with_mode(new_mode);
  }

  // Domain queries (compile-time)
  static constexpr bool is_real() { return DomainType == Domain::Real; }
  static constexpr bool is_complex() { return DomainType == Domain::Complex; }
  static constexpr bool is_integer() { return DomainType == Domain::Integer; }
  static constexpr bool is_modular() {
    return DomainType == Domain::ModularArithmetic;
  }

  // Angle domain access (compile-time)
  static constexpr auto angle_period() { return AngleConfig::period; }

  // Check if we're in modular arithmetic
  template <typename T = ModConfig>
    requires(!isSame<T, void>)
  static constexpr auto modulus() {
    return T::modulus;
  }
};

// ============================================================================
// Context Factories
// ============================================================================

// Default: real domain, full simplification
constexpr auto default_context() {
  TransformContext<0, Domain::Real> ctx;
  ctx.mode.fold_numeric_constants = true;
  ctx.mode.fold_algebraic = true;
  ctx.mode.preserve_special_values = false;  // Aggressive folding
  return ctx;
}

// Symbolic: preserve special values and exact forms
constexpr auto symbolic_context() {
  TransformContext<0, Domain::Real> ctx;
  ctx.mode.fold_numeric_constants = false;
  ctx.mode.fold_symbolic_constants = false;
  ctx.mode.preserve_special_values = true;
  ctx.mode.preserve_exact_rationals = true;
  ctx.mode.prefer_exact = true;
  return ctx;
}

// Numeric: aggressive floating-point evaluation
constexpr auto numeric_context() {
  TransformContext<0, Domain::Real> ctx;
  ctx.mode.fold_numeric_constants = true;
  ctx.mode.fold_symbolic_constants = true;  // Fold π, e, etc.
  ctx.mode.preserve_special_values = false;
  ctx.mode.preserve_exact_rationals = false;
  ctx.mode.prefer_exact = false;
  return ctx;
}

// Integer domain
constexpr auto integer_context() {
  TransformContext<0, Domain::Integer> ctx;
  ctx.mode.fold_numeric_constants = true;
  ctx.mode.fold_algebraic = true;
  return ctx;
}

// Modular arithmetic (e.g., mod 2π for angles)
template <auto Modulus>
constexpr auto modular_context() {
  TransformContext<0, Domain::ModularArithmetic, AngleDomain<>,
                   ModularDomain<Modulus>>
      ctx;
  ctx.mode.fold_numeric_constants = true;
  ctx.mode.fold_algebraic = true;
  return ctx;
}

// Angle domain (for trig functions with specific period)
template <auto Period = std::numbers::pi * 2>
constexpr auto angle_context() {
  TransformContext<0, Domain::Real, AngleDomain<Period>> ctx;
  ctx.mode.fold_numeric_constants = true;
  ctx.mode.preserve_special_values = true;  // Keep π symbolic
  return ctx;
}

}  // namespace tempura::symbolic3

// ============================================================================
// Design Notes
// ============================================================================

/*

KEY IMPROVEMENTS:

1. **Data-Driven, Not Behavioral**
   - Context carries WHAT mode to use, not HOW to behave
   - Strategies query mode.fold_numeric_constants, not ctx.has<InsideTrigTag>()
   - Separation of concerns: context = data, strategy = behavior

2. **Domain as Type Parameter**
   - Domain::Real, Domain::Integer, Domain::ModularArithmetic
   - Compile-time dispatch based on domain
   - No runtime overhead

3. **Modular Arithmetic Support**
   - mod_context<2π>() for angle wrapping
   - Compile-time modulus value
   - Type-safe

4. **Simplification Mode Structure**
   - Clear flags for what operations to perform
   - fold_numeric_constants: 2+3 → 5
   - fold_symbolic_constants: π/6 → 0.523...
   - preserve_special_values: keep π, e symbolic
   - prefer_exact: exact rationals vs floats

USAGE EXAMPLE:

```cpp
// Strategy checks context.mode, not "inside trig" tag
struct FoldConstants {
  template <Symbolic S, typename Context>
  constexpr auto apply(this auto const& self, S expr, Context ctx) {
    if (!ctx.mode.fold_numeric_constants) {
      return expr;  // Folding disabled
    }

    // Check if we should preserve special values
    if (is_special_constant(expr) && ctx.mode.preserve_special_values) {
      return expr;  // Keep π, e, etc. symbolic
    }

    // Otherwise fold
    return fold_impl(expr);
  }
};

// Trig context: preserve special values inside arguments
auto simplify_trig(auto expr) {
  auto ctx = symbolic_context();  // Preserve π, etc.
  return simplify.apply(expr, ctx);
}

// Modular arithmetic: angles mod 2π
auto simplify_angle(auto expr) {
  auto ctx = modular_context<std::numbers::pi * 2>();
  return simplify.apply(expr, ctx);
}
```

BENEFITS:

1. Strategy code doesn't need to know "where" it is (inside trig, etc.)
2. Strategy queries data: "should I fold constants?"
3. Context is pure data, no behavioral implications
4. Easy to add new modes without changing strategies
5. Compile-time domain dispatch for efficiency

*/
