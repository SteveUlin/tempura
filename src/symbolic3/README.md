# Symbolic3 - Compile-Time Symbolic Mathematics

Combinator-based symbolic computation with compile-time evaluation using C++26 template metaprogramming.

## Quick Start

```cpp
#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

// Define symbols and expressions
constexpr Symbol x, y;
constexpr auto expr = x*x + 2_c*x + 1_c;

// Simplify
constexpr auto simplified = simplify(expr, default_context());

// Differentiate
constexpr auto derivative = simplify(diff(expr, x), default_context());
// Result: 2·x + 2

// Evaluate
constexpr auto result = evaluate(expr, BinderPack{x = 3.0});
// Result: 16.0
```

## Core Components

### Headers

- **`symbolic3.h`** - Main entry point (includes all core functionality)
- **`core.h`** - `Symbol<unique>`, `Expression<Op, Args...>`, `Constant<N>`, `Fraction<N,D>`
- **`operators.h`** - Arithmetic and transcendental operators (Add, Mul, Sin, Log, etc.)
- **`simplify.h`** - Simplification strategies and rule sets
- **`evaluate.h`** - Expression evaluation with type-safe variable bindings
- **`derivative.h`** - Symbolic differentiation
- **`pattern_matching.h`** - Pattern matching and rewrite rules
- **`strategy.h`** - Strategy composition and combinators
- **`traversal.h`** - Tree traversal strategies (innermost, outermost, etc.)
- **`to_string.h`** - String conversion for debugging
- **`debug.h`** - Compile-time debugging utilities

### Design Principles

1. **Type-based computation** - Expressions are types, computation happens at compile-time
2. **Zero runtime overhead** - Everything computable at compile-time is
3. **Composable strategies** - Build complex transformations from simple rules
4. **Exact arithmetic** - Uses `Fraction<N,D>` for rational numbers, no implicit float conversions
5. **Pattern-based rewriting** - Transformations expressed as pattern → replacement rules

## Key Concepts

### Expressions

Expressions are template types representing mathematical operations:

```cpp
Symbol<0>                           // x
Constant<42>                        // Literal 42
Fraction<1,3>                       // 1/3 (exact rational)
Expression<AddOp, Symbol<0>, Constant<1>>  // x + 1
```

### Simplification

Multi-stage pipeline using fixpoint iteration:

```cpp
// Full simplification (recommended default)
auto result = simplify(expr, default_context());

// Individual rule sets
auto result1 = algebraic_simplify(expr, ctx);      // Basic algebra
auto result2 = transcendental_simplify(expr, ctx); // Trig/log/exp rules
auto result3 = collect_terms(expr, ctx);           // Like term collection
```

### Pattern Matching

Wildcards for writing rewrite rules:

```cpp
// Wildcards
x_  // Matches any single argument
a_, b_, c_  // Additional wildcards
AnyExpr     // Matches any expression
AnyConstant // Matches any constant
AnySymbol   // Matches any symbol

// Example rule: x + 0 → x
constexpr auto rule = [](auto expr, auto ctx) {
  if constexpr (match(expr, x_ + Constant<0>{})) {
    return get_matched_arg<0>(expr);  // Return x
  }
  return expr;
};
```

### Strategy Composition

Compose strategies using operators:

```cpp
// Sequential: apply A, then B
auto composed = strategyA >> strategyB;

// Choice: try A, if unchanged try B
auto fallback = strategyA | strategyB;

// Conditional
auto conditional = when(predicate, strategy);

// Fixpoint: repeat until no change
auto fixed = fixpoint(strategy);

// Traversal
auto bottom_up = innermost(strategy);
auto top_down = outermost(strategy);
```

## Examples

See `examples/` directory:

- **`symbolic3_simplify_demo.cpp`** - Simplification pipelines
- **`symbolic3_debug_demo.cpp`** - Debugging and visualization
- **`advanced_simplify_demo.cpp`** - Advanced transformations

## Tests

Tests are in `test/` directory:

```bash
# Run all symbolic3 tests
ctest --test-dir build -R symbolic3

# Organized into:
# - simplification_basic_test - Power rules, identities, fundamental algebra
# - simplification_advanced_test - Transcendental functions, constants
# - simplification_pipeline_test - Full pipelines, term collection, traversals
# - debug_test - Compile-time debugging utilities
# - evaluate_test - Expression evaluation
# - derivative_test - Symbolic differentiation
# - pattern_matching_test - Pattern matching and rewriting
```

## Debugging

### Compile-Time Inspection

```cpp
#include "symbolic3/debug.h"

// Check properties
static_assert(expression_depth(expr) == 2);
static_assert(operation_count(expr) == 5);
static_assert(is_likely_simplified(expr));

// Force type display in compiler error
force_type_display(expr);  // Shows type in error message
```

### Runtime String Conversion

```cpp
#include "symbolic3/to_string.h"

auto str = toStringRuntime(expr);
std::print("Expression: {}\n", str);
```

## Implementation Notes

- **Stateless lambdas for type identity** - Each symbol uses a unique lambda type
- **Friend injection for type IDs** - Enables efficient compile-time lookup
- **Canonical ordering** - Expressions normalized to canonical form for matching
- **Minimal stdlib usage** - Reduces compile times, enables pure constexpr
- **Strategy concepts** - Type-safe strategy composition using C++20 concepts

## Current Limitations

- No runtime symbolic computation (everything must be known at compile-time)
- Large expressions can increase compile times significantly
- Error messages from template instantiation can be complex
- Some simplifications incomplete (marked with TODOs in tests)

## References

- Code is canonical documentation - check header comments
- Examples demonstrate practical usage patterns
- Tests serve as executable specifications
