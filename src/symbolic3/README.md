# Symbolic3: Combinator-Based Symbolic Computation

**Status:** ✅ Phase 1 Complete - All Tests Passing (10/10)  
**Next Steps:** See `NEXT_STEPS.md` for development roadmap

---

## Quick Start

```cpp
#include "symbolic3/symbolic3.h"
using namespace tempura::symbolic3;

// Create symbols
Symbol x, y;

// Build expressions
auto expr = sin(x) + Constant<2>{} * y;

// Apply transformations
auto simplified = algebraic_simplify.apply(expr, default_context());

// Debug output
debug_print(simplified, "result");  // result: (sin(x123) + (2 * x124))
```

---

## Table of Contents

1. [Overview](#overview)
2. [Core Concepts](#core-concepts)
3. [API Reference](#api-reference)
4. [Examples](#examples)
5. [Architecture](#architecture)
6. [Debugging Guide](#debugging-guide)
7. [Testing](#testing)
8. [Design Rationale](#design-rationale)
9. [Next Steps](#next-steps)

---

## Overview

Symbolic3 is a complete redesign of tempura's symbolic computation system using a **combinator-based architecture**. It provides compile-time symbolic manipulation with zero runtime overhead.

### Key Features

- **Generic recursion combinators** (fixpoint, fold, unfold, innermost, outermost)
- **Composable transformation pipelines** (sequential `>>`, choice `|`, conditional `when`)
- **Context-aware transformations** (domain-specific rules, depth tracking)
- **User-extensible strategies** (add custom transformations without modifying core)
- **Full compile-time evaluation** (all operations are `constexpr`)

### What's Implemented (Phase 1)

✅ Core symbolic types (Symbol, Constant, Expression)  
✅ Pattern matching with wildcards  
✅ Strategy system with concepts (no CRTP)  
✅ Context system with type-safe tags  
✅ Traversal combinators (8 strategies)  
✅ Basic simplification (algebraic, trigonometric)  
✅ Symbolic differentiation with chain rule  
✅ String conversion and debugging utilities  
✅ 10 test suites with 100% pass rate  

### What's Missing (See NEXT_STEPS.md)

🔧 Evaluation system (cannot substitute values yet)  
🔧 Term collection (2*x + 3*x doesn't simplify to 5*x)  
🔧 Extended simplification rules (log laws, exp laws, advanced trig)  
🔧 Integration with other tempura libraries  

---

## Core Concepts

### 1. Symbolic Types

All symbolic values are **types**, not runtime values:

```cpp
Symbol<> x;              // Type: Symbol<lambda123>
Constant<42> c;          // Type: Constant<42>
auto expr = x + c;       // Type: Expression<AddOp, Symbol<...>, Constant<42>>
```

**Benefits:**
- Zero runtime overhead
- Compile-time computation
- Type-level uniqueness (each symbol is distinct type)

### 2. Strategy Pattern

Transformations are **strategies** implementing a simple concept:

```cpp
template <typename S>
concept Strategy = requires(S const& strat, Symbol<> sym, TransformContext<> ctx) {
  { strat.apply(sym, ctx) } -> Symbolic;
};
```

Any type with an `apply(expr, ctx)` method is a strategy.

**Example:**
```cpp
struct MyStrategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    // Your transformation logic here
    return expr;  // or transformed version
  }
};
```

### 3. Context System

Context carries state through transformations:

```cpp
auto ctx = default_context()
  .increment_depth<1>()                    // Track recursion depth
  .with(SimplificationMode{
    .constant_folding_enabled = true,
    .algebraic_simplification_enabled = true
  });

// Check context
if constexpr (ctx.mode.constant_folding_enabled) { /* ... */ }
```

**Context factories:**
- `default_context()` - Balanced settings
- `numeric_context()` - Aggressive numeric evaluation
- `symbolic_context()` - Preserve symbolic forms

### 4. Composition Operators

Build complex transformations from simple pieces:

```cpp
// Sequential: apply in order
auto pipeline = strategy1 >> strategy2 >> strategy3;

// Choice: try first, fallback to second if no change
auto alternatives = try_this | or_this | or_that;

// Conditional: apply only if predicate holds
auto conditional = when([](auto expr) { return is_symbol(expr); }, strategy);
```

### 5. Traversal Combinators

Control how transformations recurse:

```cpp
fold(strategy)          // Bottom-up (children first)
unfold(strategy)        // Top-down (parent first)  
innermost(strategy)     // Apply at leaves first, recursively
outermost(strategy)     // Apply at root first, recursively
fixpoint(strategy)      // Repeat until no change
bottomup(strategy)      // Post-order traversal
topdown(strategy)       // Pre-order traversal
```

---

## API Reference

### Creating Expressions

```cpp
// Symbols
Symbol x, y, z;

// Constants
Constant<42> c1;
auto c2 = 3_c;  // Constant<3> via user-defined literal

// Arithmetic
auto sum = x + y;
auto product = x * y;
auto power = pow(x, 2_c);
auto negation = -x;

// Trigonometric
auto s = sin(x);
auto c = cos(x);
auto t = tan(x);

// Exponential/Logarithm
auto e = exp(x);
auto l = log(x);

// Hyperbolic
auto sh = sinh(x);
auto ch = cosh(x);
auto th = tanh(x);
```

### Pattern Matching

```cpp
// Wildcards
AnyArg x_;        // Matches any symbolic value
AnyExpr expr_;    // Matches compound expressions only
AnyConstant c_;   // Matches constants only
AnySymbol s_;     // Matches symbols only

// Matching
if (auto [matched, bindings] = match(expr, pattern); matched) {
  // Extract bound values from bindings
}

// Example: match sin(x)
auto [matched, x_val] = match(expr, sin(x_));
if (matched) {
  // x_val contains the argument to sin
}
```

### Predefined Strategies

```cpp
// Basic transformations
Identity{}              // No-op
Fail{}                  // Always fails

// Algebraic simplification
FoldConstants{}         // Evaluate constant arithmetic
ApplyAlgebraicRules{}   // Apply identities (x+0→x, x*1→x, etc.)
algebraic_simplify      // Pre-built pipeline

// Recursive simplification
algebraic_simplify_recursive   // With bottom-up traversal
bottomup_simplify              // Bottom-up traversal
topdown_simplify               // Top-down traversal
full_simplify                  // Aggressive multi-pass

// Trigonometric
trig_aware_simplify     // Context-aware trig rules
```

### Differentiation

```cpp
Symbol x;
auto expr = x * x + 2_c * x + 1_c;

// Basic differentiation
auto deriv = diff(expr, x);  
// Returns: 2*x + 2 (after simplification)

// Compile-time verification
constexpr auto deriv_constexpr = diff(pow(x, 3_c), x);
static_assert(match(deriv_constexpr, 3_c * pow(x, 2_c)));

// Supported operations
diff(sin(x), x);        // → cos(x)
diff(exp(x), x);        // → exp(x)
diff(log(x), x);        // → 1/x
diff(pow(x, n), x);     // → n * pow(x, n-1)
```

### String Conversion

```cpp
Symbol x;
auto expr = x + Constant<2>{};

// Runtime string conversion
std::string str = toStringRuntime(expr);  
// Result: "(x123 + 2)"

// Debug printing
debug_print(expr, "my expr");
// Output: my expr: (x123 + 2)

// Compact with type info
debug_print_compact(expr, "expr");
// Output: expr: (x123 + 2) :: Expression<Add, Symbol<123>, Constant<2>>

// Tree visualization
debug_print_tree(expr, 0, "expr");
// Output:
// expr: Expression: (x123 + 2)
//   Op: Add (+)
//   Args:
//     [0]: Symbol: x123
//     [1]: Constant: 2
```

---

## Examples

### Example 1: Basic Simplification

```cpp
#include "symbolic3/symbolic3.h"
using namespace tempura::symbolic3;

"Basic algebraic simplification"_test = [] {
  Symbol x;
  
  // x + 0 → x
  auto expr1 = x + Constant<0>{};
  auto result1 = algebraic_simplify.apply(expr1, default_context());
  static_assert(match(result1, x));
  
  // x * 1 → x
  auto expr2 = x * Constant<1>{};
  auto result2 = algebraic_simplify.apply(expr2, default_context());
  static_assert(match(result2, x));
  
  // x * 0 → 0
  auto expr3 = x * Constant<0>{};
  auto result3 = algebraic_simplify.apply(expr3, default_context());
  static_assert(match(result3, Constant<0>{}));
};
```

### Example 2: Custom Strategy

```cpp
// Define custom transformation
struct DoubleNegationRemoval {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    // Pattern: -(-x) → x
    if constexpr (is_expression<S>) {
      using Op = get_op_t<S>;
      if constexpr (std::same_as<Op, NegOp>) {
        using Args = get_args_t<S>;
        using Inner = std::tuple_element_t<0, Args>;
        
        if constexpr (is_expression<Inner>) {
          using InnerOp = get_op_t<Inner>;
          if constexpr (std::same_as<InnerOp, NegOp>) {
            using InnerArgs = get_args_t<Inner>;
            using InnerInner = std::tuple_element_t<0, InnerArgs>;
            return InnerInner{};
          }
        }
      }
    }
    return expr;
  }
};

// Use it
"Double negation removal"_test = [] {
  Symbol x;
  auto expr = -(-x);
  auto result = DoubleNegationRemoval{}.apply(expr, default_context());
  static_assert(match(result, x));
};
```

### Example 3: Differentiation

```cpp
"Chain rule in action"_test = [] {
  Symbol x;
  
  // d/dx[sin(x²)] = cos(x²) · 2x
  auto expr = sin(x * x);
  auto deriv = diff(expr, x);
  
  // After simplification, should be cos(x²) * 2x
  auto simplified = full_simplify.apply(deriv, default_context());
  
  debug_print(simplified, "d/dx[sin(x²)]");
};
```

### Example 4: Context-Aware Transformation

```cpp
"Numeric vs symbolic context"_test = [] {
  Symbol x;
  auto expr = Constant<2>{} * x + Constant<3>{} * x;
  
  // With numeric context: aggressive folding
  auto result1 = algebraic_simplify.apply(expr, numeric_context());
  // Note: term collection not yet implemented!
  
  // With symbolic context: preserve structure
  auto result2 = algebraic_simplify.apply(expr, symbolic_context());
};
```

---

## Architecture

### File Organization

```
symbolic3/
├── symbolic3.h           # Main header (include this)
├── core.h                # Symbol, Constant, Expression types
├── operators.h           # Expression builders (+, *, sin, etc.)
├── constants.h           # Common constants (pi, e, etc.)
├── context.h             # Context system with tags
├── matching.h            # Pattern matching utilities
├── pattern_matching.h    # Advanced pattern matching
├── strategy.h            # Strategy concept and basic combinators
├── traversal.h           # Traversal strategies (Fold, Unfold, etc.)
├── transforms.h          # Example transformation strategies
├── simplify.h            # Algebraic and trigonometric simplification
├── derivative.h          # Symbolic differentiation
├── evaluate.h            # Value substitution (stub - TODO)
├── to_string.h           # String conversion and debugging
├── ordering.h            # Term ordering utilities
├── README.md             # This file
├── NEXT_STEPS.md         # Development roadmap
└── test/                 # Test suite (10 test files)
```

### Dependency Graph

```
symbolic3.h
  ├─ core.h
  ├─ operators.h ──┬─ core.h
  │                └─ constants.h
  ├─ context.h ──── core.h
  ├─ strategy.h ──┬─ core.h
  │               └─ context.h
  ├─ matching.h ─── core.h
  ├─ pattern_matching.h ──┬─ core.h
  │                       └─ matching.h
  ├─ traversal.h ──┬─ core.h
  │                ├─ strategy.h
  │                └─ context.h
  ├─ simplify.h ───┬─ core.h
  │                ├─ operators.h
  │                ├─ pattern_matching.h
  │                ├─ strategy.h
  │                └─ traversal.h
  ├─ derivative.h ─┬─ core.h
  │                ├─ operators.h
  │                ├─ pattern_matching.h
  │                ├─ simplify.h
  │                └─ strategy.h
  ├─ to_string.h ──┬─ core.h
  │                └─ operators.h
  ├─ transforms.h ─┬─ strategy.h
  │                └─ matching.h
  └─ evaluate.h ─── core.h (stub)
```

### Type System

```cpp
// Core hierarchy
SymbolicTag              // Base tag for all symbolic types
  ├─ Symbol<unique>      // Symbolic variable
  ├─ Constant<val>       // Numeric constant
  ├─ Expression<Op, Args...>  // Compound expression
  └─ Wildcards           // Pattern matching
      ├─ AnyArg
      ├─ AnyExpr
      ├─ AnyConstant
      └─ AnySymbol

// Operators (tag types)
AddOp, MulOp, PowOp, NegOp, DivOp
SinOp, CosOp, TanOp
SinhOp, CoshOp, TanhOp
ExpOp, LogOp, SqrtOp
ASinOp, ACosOp, ATanOp
```

### Compilation Model

**Symbolic expressions are pure types:**
- No runtime representation (zero overhead)
- All computation at compile-time
- Type encoding: `x + 2` becomes `Expression<AddOp, Symbol<λ₁>, Constant<2>>`
- Compiler optimizes away all abstractions

**Performance characteristics:**
- Runtime: Zero overhead (types erased after compilation)
- Compile-time: Grows with expression complexity
- Template instantiation depth: Grows with recursion depth

---

## Debugging Guide

### Quick Reference

| Function | Purpose | Example |
|----------|---------|---------|
| `toStringRuntime(expr)` | Convert to string | `std::string s = toStringRuntime(x + 2_c);` |
| `debug_print(expr, label)` | Simple print | `debug_print(expr, "result");` |
| `debug_print_compact(expr)` | Print with type | Shows full type information |
| `debug_print_tree(expr)` | Tree view | Hierarchical structure visualization |
| `debug_type_name(expr)` | Compiler type name | Raw compiler type string |

### Common Debugging Patterns

**1. Print expression structure:**
```cpp
Symbol x;
auto expr = (x + Constant<1>{}) * (x - Constant<1>{});
debug_print_tree(expr);
// Output:
// Expression: ((x123 + 1) * (x123 - 1))
//   Op: Mul (*)
//   Args:
//     [0]: Expression: (x123 + 1)
//       Op: Add (+)
//       Args:
//         [0]: Symbol: x123
//         [1]: Constant: 1
//     [1]: Expression: (x123 - 1)
//       ...
```

**2. Check transformation steps:**
```cpp
auto step1 = strategy1.apply(expr, ctx);
debug_print(step1, "after strategy1");

auto step2 = strategy2.apply(step1, ctx);
debug_print(step2, "after strategy2");
```

**3. Verify pattern matching:**
```cpp
auto [matched, x_val] = match(expr, sin(x_));
if (matched) {
  debug_print(x_val, "matched argument");
} else {
  std::println("Pattern did not match");
}
```

**4. Inspect types at compile-time:**
```cpp
// Force compile error to see type
template <typename T> struct ShowType;
ShowType<decltype(expr)> show;  // Error message shows type
```

---

## Testing

### Running Tests

```bash
# Build
cmake -B build -G Ninja
ninja -C build

# Run all symbolic3 tests
ctest --test-dir build -R symbolic3 --output-on-failure

# Run specific test
./build/src/symbolic3/test/basic_test

# Run with verbose output
ctest --test-dir build -R symbolic3 -V
```

### Test Organization

```
src/symbolic3/test/
├── basic_test.cpp              # Core types and operators
├── matching_test.cpp           # Pattern matching
├── pattern_binding_test.cpp    # Pattern binding
├── strategy_v2_test.cpp        # Strategy system
├── simplify_test.cpp           # Simplification rules
├── traversal_test.cpp          # Traversal combinators
├── traversal_simplify_test.cpp # Traversal with simplification
├── full_simplify_test.cpp      # Full simplification pipeline
├── derivative_test.cpp         # Symbolic differentiation
├── transcendental_test.cpp     # Trig and exp/log functions
└── to_string_test.cpp          # String conversion
```

### Writing Tests

Tests use tempura's `unit.h` framework:

```cpp
#include "unit.h"
#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

"Test description"_test = [] {
  Symbol x;
  auto expr = x + Constant<0>{};
  auto result = algebraic_simplify.apply(expr, default_context());
  
  // Compile-time check
  static_assert(match(result, x));
  
  // Runtime check
  assert(match(result, x));
};

// Test groups
suite("simplification") = [] {
  "addition identity"_test = [] { /* ... */ };
  "multiplication identity"_test = [] { /* ... */ };
};
```

### Current Test Status

✅ **10/10 tests passing** (as of Phase 1 completion)

**Coverage:**
- Core types and concepts
- Pattern matching
- Strategy composition
- Traversal combinators
- Algebraic simplification
- Trigonometric simplification
- Differentiation
- String conversion

**Gaps (to be filled in Phase 2):**
- Evaluation with value substitution
- Term collection
- Advanced simplification rules
- Integration with other libraries

---

## Design Rationale

### Why Combinators?

**Problems with symbolic2:**
- Hardcoded fixpoint recursion only
- Flat if-else dispatch (hard to extend)
- No context awareness
- Monolithic design

**Benefits of symbolic3:**
1. **Modularity**: Compose strategies like LEGO bricks
2. **Extensibility**: Users add strategies without modifying core
3. **Flexibility**: Multiple traversal strategies, conditional application
4. **Context**: Pass state (depth, mode, domain) through transformations
5. **Performance**: Better compiler optimization opportunities

### Why Concepts Instead of CRTP?

**V1 used CRTP:**
```cpp
struct MyStrategy : Strategy<MyStrategy> {
  template <Symbolic S>
  constexpr auto apply_impl(S expr) const { /* ... */ }
};
```

**V2 uses concepts:**
```cpp
struct MyStrategy {
  template <Symbolic S>
  constexpr auto apply(S expr) const { /* ... */ }
};
```

**Benefits:**
- Simpler code (no CRTP boilerplate)
- Better error messages
- More flexible (any type matching concept works)
- Easier composition

### Why Types Instead of Values?

Expressions are encoded as types, not runtime values:

```cpp
// Type encoding
using ExprType = Expression<AddOp, Symbol<lambda123>, Constant<2>>;

// NOT runtime encoding
struct RuntimeExpr {
  OpType op;
  std::vector<RuntimeExpr> children;
};
```

**Benefits:**
- Zero runtime overhead
- Compile-time computation and verification
- Type-level uniqueness (impossible to confuse different expressions)

**Costs:**
- Large type names
- Slow compilation for complex expressions
- Cannot handle dynamic/user-input expressions

**When to use which:**
- Use symbolic3 for compile-time known expressions
- Use symbolic2 or runtime system for dynamic expressions

---

## Next Steps

See **`NEXT_STEPS.md`** for comprehensive development roadmap.

**Immediate priorities (Phase 2):**

1. **Evaluation System** - Substitute values into expressions
2. **Term Collection** - Simplify `2*x + 3*x` to `5*x`
3. **Extended Simplification** - Log laws, exp laws, advanced trig

**Longer-term (Phase 3+):**
- Integration with autodiff
- Matrix/vector symbolic operations
- Rational function simplification
- Performance optimization

---

## Comparison to Symbolic2

| Feature | Symbolic2 | Symbolic3 |
|---------|-----------|-----------|
| **Recursion** | Fixpoint only | Fixpoint, Fold, Unfold, Innermost, Outermost, etc. |
| **Dispatch** | Flat if-else chain | Composable strategies |
| **Context** | None | Full context system with tags |
| **Extensibility** | Modify core files | User-defined strategies |
| **Composition** | None | `>>`, `\|`, `when` operators |
| **Performance** | Baseline | Better optimization opportunities |
| **API Style** | CRTP-based | Concept-based |
| **Complexity** | Moderate | Higher (but more powerful) |

**When to use symbolic2:**
- Simple algebraic manipulations
- Quick prototyping
- When you need runtime flexibility

**When to use symbolic3:**
- Complex transformation pipelines
- Context-aware transformations
- Custom traversal strategies
- Performance-critical compile-time computation

---

## Contributing

### Adding a New Simplification Rule

1. **Define pattern and replacement:**
   ```cpp
   // Example: sin(-x) → -sin(x)
   auto pattern = sin(neg(x_));
   auto replacement = neg(sin(x_));
   ```

2. **Implement strategy:**
   ```cpp
   struct SinNegRule {
     template <Symbolic S, typename Context>
     constexpr auto apply(S expr, Context ctx) const {
       if (auto [matched, x_val] = match(expr, pattern); matched) {
         return evaluate_replacement(replacement, x_val);
       }
       return expr;
     }
   };
   ```

3. **Add to pipeline:**
   ```cpp
   constexpr auto trig_rules = SinNegRule{} | existing_trig_rules;
   ```

4. **Test:**
   ```cpp
   "sin(-x) simplifies"_test = [] {
     Symbol x;
     auto expr = sin(neg(x));
     auto result = trig_rules.apply(expr, default_context());
     static_assert(match(result, neg(sin(x))));
   };
   ```

### Adding a New Operator

1. Define operator tag in `operators.h`
2. Add expression builder function
3. Implement differentiation rule in `derivative.h`
4. Add simplification rules in `simplify.h`
5. Add to_string support in `to_string.h`
6. Write tests

---

## References

### Design Documents
- `SYMBOLIC2_COMBINATORS_DESIGN.md` (35KB) - Original architecture
- `SYMBOLIC2_GENERALIZATION_COMPLETE.md` (20KB) - Implementation guide
- `SYMBOLIC2_COMBINATORS_QUICKREF.md` (11KB) - Quick reference

### Examples
- `examples/symbolic3_demo.cpp` - Basic usage demo
- `examples/symbolic3_v2_demo.cpp` - V2 strategy system demo
- `examples/symbolic3_simplify_demo.cpp` - Simplification examples

### Prototypes
- `prototypes/combinator_strategies.cpp` - Original prototype
- `prototypes/value_based_symbolic.cpp` - Alternative design exploration

---

**Last Updated:** October 5, 2025  
**Version:** 1.0 (Phase 1 Complete)  
**Authors:** Tempura Development Team  
**License:** See repository LICENSE file
