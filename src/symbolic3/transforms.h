#pragma once

#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"
#include "symbolic3/strategy.h"

// Example transformations using data-driven context and concepts

namespace tempura::symbolic3 {

// ============================================================================
// Constant Folding (queries context.mode)
// ============================================================================

struct FoldConstants {
  template <Symbolic S, typename Context>
  constexpr auto apply(this auto const& self, S expr, Context ctx) {
    // Query: should we fold constants?
    if (!ctx.mode.fold_numeric_constants) {
      return expr;
    }

    // Fold addition of constants
    if constexpr (matches_op_v<AddOp, S>) {
      using args = get_args_t<S>;
      using L = std::tuple_element_t<0, args>;
      using R = std::tuple_element_t<1, args>;

      if constexpr (is_constant<L> && is_constant<R>) {
        constexpr auto result = L::value + R::value;

        // Check if we're in modular arithmetic
        if constexpr (ctx.is_modular()) {
          constexpr auto mod = ctx.modulus();
          constexpr auto wrapped = result % mod;
          return Constant<wrapped>{};
        } else {
          return Constant<result>{};
        }
      }
    }

    // Fold multiplication of constants
    if constexpr (matches_op_v<MulOp, S>) {
      using args = get_args_t<S>;
      using L = std::tuple_element_t<0, args>;
      using R = std::tuple_element_t<1, args>;

      if constexpr (is_constant<L> && is_constant<R>) {
        constexpr auto result = L::value * R::value;

        if constexpr (ctx.is_modular()) {
          constexpr auto mod = ctx.modulus();
          constexpr auto wrapped = result % mod;
          return Constant<wrapped>{};
        } else {
          return Constant<result>{};
        }
      }
    }

    return expr;
  }
};

// ============================================================================
// Algebraic Identities
// ============================================================================

struct ApplyAlgebraicRules {
  template <Symbolic S, typename Context>
  constexpr auto apply(this auto const& self, S expr, Context ctx) {
    // Only apply if algebraic simplification is enabled
    if (!ctx.mode.fold_algebraic) {
      return expr;
    }

    // x + 0 → x
    if constexpr (matches_op_v<AddOp, S>) {
      using args = get_args_t<S>;
      using L = std::tuple_element_t<0, args>;
      using R = std::tuple_element_t<1, args>;

      if constexpr (is_constant<R> && R::value == 0) {
        return L{};
      }
      if constexpr (is_constant<L> && L::value == 0) {
        return R{};
      }
    }

    // x * 0 → 0
    if constexpr (matches_op_v<MulOp, S>) {
      using args = get_args_t<S>;
      using L = std::tuple_element_t<0, args>;
      using R = std::tuple_element_t<1, args>;

      if constexpr (is_constant<R> && R::value == 0) {
        return Constant<0>{};
      }
      if constexpr (is_constant<L> && L::value == 0) {
        return Constant<0>{};
      }

      // x * 1 → x
      if constexpr (is_constant<R> && R::value == 1) {
        return L{};
      }
      if constexpr (is_constant<L> && L::value == 1) {
        return R{};
      }
    }

    return expr;
  }
};

// ============================================================================
// Example: Trig Simplification (data-driven)
// ============================================================================

struct SimplifyTrig {
  template <Symbolic S, typename Context>
  constexpr auto apply(this auto const& self, S expr, Context ctx) {
    // Check if expression is a trig function
    if constexpr (is_trig_function<S>) {
      // For trig functions, we want to:
      // 1. Preserve special angle values (π/6, π/4, etc.) if in symbolic mode
      // 2. Normalize angles to [0, period) if in angle domain

      using args = get_args_t<S>;
      using Arg = std::tuple_element_t<0, args>;

      // If the argument is a constant and we should fold symbolic constants
      if constexpr (is_constant<Arg>) {
        if (ctx.mode.fold_symbolic_constants) {
          // Actually evaluate sin(π/6) → 0.5
          // (simplified for example)
          return expr;  // Would compute actual value
        } else {
          // Keep symbolic: sin(π/6) stays as sin(π/6)
          return expr;
        }
      }

      // If we're in an angle domain, normalize to [0, period)
      if constexpr (requires { ctx.angle_period(); }) {
        // Would normalize angle here
        // e.g., sin(5π) → sin(π) when period is 2π
      }
    }

    return expr;
  }
};

// ============================================================================
// Predefined Pipelines
// ============================================================================

// NOTE: algebraic_simplify is now defined in simplify.h with comprehensive
// rules The version here was a placeholder that used FoldConstants |
// ApplyAlgebraicRules

// With trig awareness
// inline constexpr auto simplify_with_trig =
//     algebraic_simplify | SimplifyTrig{};

}  // namespace tempura::symbolic3

// ============================================================================
// Usage Examples
// ============================================================================

/*

EXAMPLE 1: Different folding modes

```cpp
using namespace tempura::symbolic3::v2;

constexpr Symbol x;
constexpr Constant<2> two;
constexpr Constant<3> three;

// Numeric mode: fold everything
auto numeric_ctx = numeric_context();
auto result1 = FoldConstants{}.apply(FoldConstants{}, two + three, numeric_ctx);
// Result: Constant<5>

// Symbolic mode: preserve constants
auto symbolic_ctx = symbolic_context();
auto result2 = FoldConstants{}.apply(FoldConstants{}, two + three,
symbolic_ctx);
// Result: 2 + 3 (unchanged because folding disabled)
```

EXAMPLE 2: Modular arithmetic

```cpp
// Working mod 2π (for angles)
auto mod_ctx = modular_context<2 * std::numbers::pi>();

constexpr Constant<7> seven;  // 7 radians
constexpr Constant<2> two;

auto result = FoldConstants{}.apply(
    FoldConstants{},
    seven + two,  // 7 + 2 = 9 radians
    mod_ctx
);
// Result: Constant<9 % (2π)> = Constant<~2.717> (wrapped to [0, 2π))
```

EXAMPLE 3: Strategy doesn't need to know "where" it is

```cpp
// OLD WAY (behavioral context):
struct OldFoldConstants : Strategy<OldFoldConstants> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Strategy needs to know about "inside trig" concept
    if (ctx.has<InsideTrigTag>()) {
      // Different behavior
    }
  }
};

// NEW WAY (data-driven context):
struct FoldConstants {
  template <Symbolic S, typename Context>
  constexpr auto apply(this auto const& self, S expr, Context ctx) {
    // Strategy just checks: should I fold?
    if (!ctx.mode.fold_numeric_constants) {
      return expr;  // No need to know WHY folding is disabled
    }
    // ... folding logic
  }
};

// Caller decides the mode
auto inside_trig_ctx = symbolic_context();  // Preserve special values
auto result = FoldConstants{}.apply(FoldConstants{}, expr, inside_trig_ctx);
```

EXAMPLE 4: Domain-specific simplification

```cpp
// Integer domain
auto int_ctx = integer_context();
auto result1 = algebraic_simplify.apply(
    algebraic_simplify,
    x + Constant<0>{},
    int_ctx
);
// Result: x (integer arithmetic rules)

// Boolean domain (future extension)
auto bool_ctx = TransformContext<0, Domain::Boolean>{};
bool_ctx.mode.fold_algebraic = true;
auto result2 = algebraic_simplify.apply(
    algebraic_simplify,
    a && true,  // Boolean expression
    bool_ctx
);
// Result: a (boolean identity)
```

KEY BENEFITS OF THIS DESIGN:

1. **Separation of Concerns**
   - Context = WHAT data/mode
   - Strategy = HOW to transform
   - Caller = decides the mode

2. **Strategies are Simple**
   - Just check ctx.mode flags
   - No need to understand "inside trig" or other behavioral concepts
   - Pure data queries

3. **Flexible**
   - Easy to add new modes (just add flags to SimplificationMode)
   - Easy to add new domains (just extend Domain enum)
   - Strategies don't need to change

4. **Type-Safe**
   - Compile-time domain dispatch
   - Compile-time modulus values
   - No runtime overhead

5. **Clear Intent**
   - numeric_context() - "evaluate everything"
   - symbolic_context() - "preserve special values"
   - modular_context<2π>() - "wrap angles"

*/
