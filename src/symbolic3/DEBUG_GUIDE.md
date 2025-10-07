# Symbolic3 Compile-Time Debugging Guide

This guide demonstrates how to use the compile-time debugging utilities in `symbolic3/debug.h` to inspect and verify symbolic expressions during compilation.

## Overview

The symbolic3 debugging utilities provide tools for:

- **Type inspection** - View expression types in compiler errors
- **Property analysis** - Check expression depth, operation count, and simplification status
- **Structural comparison** - Verify expressions are equivalent
- **Simplification verification** - Ensure transformations produce expected results
- **Compile-time testing** - Build regression tests using `static_assert`

All these tools operate entirely at compile time with **zero runtime overhead**.

## Quick Start

```cpp
#include "symbolic3/debug.h"
#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

// Create an expression
constexpr Symbol x;
constexpr auto expr = x + Constant<1>{};

// Check properties at compile time
static_assert(expression_depth(expr) == 1);
static_assert(operation_count(expr) == 1);
static_assert(is_likely_simplified(expr));

// Verify simplification
constexpr auto unsimplified = x + Constant<0>{};
constexpr auto simplified = simplify.apply(unsimplified, default_context());
static_assert(!std::is_same_v<decltype(simplified), decltype(unsimplified)>);
```

## Expression Properties

### `expression_depth(expr)` - Tree Depth

Returns the maximum depth of the expression tree.

```cpp
constexpr Symbol x, y;

static_assert(expression_depth(x) == 0);                    // Leaf node
static_assert(expression_depth(x + y) == 1);                // One operation
static_assert(expression_depth((x + y) * (x - y)) == 2);    // Nested operations
```

### `operation_count(expr)` - Operation Count

Returns the total number of operations in the expression.

```cpp
constexpr Symbol x, y;

static_assert(operation_count(x) == 0);                 // No operations
static_assert(operation_count(x + y) == 1);             // One addition
static_assert(operation_count((x + y) * (x - y)) == 3); // +, -, *
```

### `is_likely_simplified(expr)` - Simplification Check

Heuristic check for obvious unsimplified patterns (x + 0, x \* 1, etc.).

```cpp
constexpr Symbol x;

static_assert(is_likely_simplified(x + Constant<2>{}));   // Simplified
static_assert(!is_likely_simplified(x + Constant<0>{}));  // Not simplified
static_assert(!is_likely_simplified(x * Constant<1>{}));  // Not simplified
```

**Note**: This is a heuristic check, not exhaustive. It only detects common patterns.

## Structural Analysis

### `structurally_equal(expr1, expr2)` - Type Equality

Checks if two expressions have identical structure (same types).

```cpp
constexpr Symbol x;
constexpr auto expr1 = x + Constant<1>{};
constexpr auto expr2 = x + Constant<1>{};
constexpr auto expr3 = x + Constant<2>{};

static_assert(structurally_equal(expr1, expr2));   // Same structure
static_assert(!structurally_equal(expr1, expr3));  // Different constants
```

### `contains_subexpression(parent, sub)` - Subexpression Search

Checks if an expression contains a subexpression.

```cpp
constexpr Symbol x, y, z;
constexpr auto sub = x + y;
constexpr auto parent = (x + y) * z;

static_assert(contains_subexpression(parent, sub));  // Contains x+y
static_assert(contains_subexpression(parent, x));    // Contains x
static_assert(!contains_subexpression(x, parent));   // x doesn't contain parent
```

## Type Inspection (Debugging)

### `CONSTEXPR_PRINT_TYPE(type)` - Show Type in Errors

Forces compiler to show type information in error message. **This will fail compilation!**

```cpp
constexpr Symbol x;
constexpr auto expr = simplify.apply(x + Constant<0>{}, default_context());

// Uncomment to see the type of simplified expression:
// CONSTEXPR_PRINT_TYPE(decltype(expr));
```

The compiler error will contain the full type name, showing you exactly what the simplification produced.

### `constexpr_type_name(expr)` - Runtime Type Info

Returns `__PRETTY_FUNCTION__` with type information (for runtime inspection).

```cpp
constexpr auto type_info = constexpr_type_name(expr);
// Use in debugger or print at runtime
```

## Simplification Verification

### `VERIFY_SIMPLIFICATION(actual, expected)` - Assert Simplification Result

Verifies that simplification produced the expected result. **Fails compilation if different.**

```cpp
constexpr Symbol x;
constexpr auto expr = x + Constant<0>{};
constexpr auto result = simplify.apply(expr, default_context());

// Uncomment to verify result matches expectation:
// VERIFY_SIMPLIFICATION(result, x);  // Fails if result != x
```

### `CONSTEXPR_ASSERT_EQUAL(actual, expected)` - Assert Expression Equality

Similar to `VERIFY_SIMPLIFICATION` but more general purpose.

```cpp
constexpr Symbol x;
constexpr auto expr1 = x + Constant<1>{};
constexpr auto expr2 = x + Constant<1>{};

// Uncomment to verify they're equal:
// CONSTEXPR_ASSERT_EQUAL(expr1, expr2);  // Passes
```

### `SYMBOLIC_STATIC_ASSERT(condition, actual, expected)` - Enhanced Static Assert

Provides better error messages for symbolic expression comparisons.

```cpp
constexpr Symbol x;
constexpr auto result = simplify.apply(x + Constant<0>{}, default_context());
constexpr auto expected = x;

SYMBOLIC_STATIC_ASSERT(
    std::is_same_v<decltype(result), decltype(expected)>,
    result,
    expected
);
```

## Practical Workflow

### 1. Develop New Simplification Rules

```cpp
"My new rule"_test = [] {
  constexpr Symbol x;

  // Step 1: Create test case
  constexpr auto input = Constant<2>{} * x + Constant<3>{} * x;

  // Step 2: Apply simplification
  constexpr auto result = full_simplify.apply(input, default_context());

  // Step 3: Verify properties
  static_assert(operation_count(result) < operation_count(input),
                "Should reduce operations");

  // Step 4: Verify structure (uncomment to check)
  // CONSTEXPR_PRINT_TYPE(decltype(result));
};
```

### 2. Debug Failing Simplifications

```cpp
"Debug simplification"_test = [] {
  constexpr Symbol x;
  constexpr auto expr = /* complex expression */;

  // Check input properties
  static_assert(expression_depth(expr) == /* expected depth */);
  static_assert(operation_count(expr) == /* expected count */);

  // Apply simplification
  constexpr auto result = simplify.apply(expr, default_context());

  // Uncomment to see what type was produced:
  // CONSTEXPR_PRINT_TYPE(decltype(result));

  // Check if it changed
  static_assert(!std::is_same_v<decltype(result), decltype(expr)>,
                "Should have transformed");
};
```

### 3. Build Regression Tests

```cpp
"Regression test for issue #123"_test = [] {
  constexpr Symbol x, y;
  constexpr auto input = (x + y) - (y + x);  // Should simplify to 0
  constexpr auto result = full_simplify.apply(input, default_context());

  // This is a regression test - if simplification breaks,
  // this static_assert will catch it at compile time
  VERIFY_SIMPLIFICATION(result, Constant<0>{});
};
```

## Examples

See these files for comprehensive examples:

- **`examples/constexpr_debugging_demo.cpp`** - Runtime demo showing all features
- **`src/symbolic3/test/debug_test.cpp`** - Compile-time test suite

Run the demo:

```bash
ninja -C build examples/constexpr_debugging_demo
./build/examples/constexpr_debugging_demo
```

Run the tests:

```bash
ctest --test-dir build -R symbolic3_debug
```

## Tips and Tricks

### Tip 1: Use in Test Files

The debugging utilities are most useful in test files where you can use `static_assert`:

```cpp
#include "symbolic3/debug.h"
#include "unit.h"

"Test name"_test = [] {
  constexpr auto result = /* ... */;
  static_assert(expression_depth(result) <= 3);
  static_assert(is_likely_simplified(result));
};
```

### Tip 2: Incremental Debugging

When debugging complex simplifications, apply rules incrementally:

```cpp
constexpr auto step1 = rule1.apply(expr, ctx);
static_assert(operation_count(step1) < operation_count(expr));

constexpr auto step2 = rule2.apply(step1, ctx);
static_assert(operation_count(step2) < operation_count(step1));
```

### Tip 3: Compiler Flags

Use `-ftime-trace` (Clang) or `-ftime-report` (GCC) to see compile-time profiling:

```bash
g++ -ftime-report ...
clang++ -ftime-trace ...
```

### Tip 4: Expression Complexity Limits

Very deep expressions can cause long compile times. Use `expression_depth()` to monitor:

```cpp
constexpr auto deep = /* nested expression */;
static_assert(expression_depth(deep) < 10, "Too deep!");
```

## Limitations

1. **Heuristic Checks**: `is_likely_simplified()` only catches obvious patterns
2. **Type-Based**: All comparisons are based on types, not symbolic values
3. **Compile-Time Only**: These tools work during compilation, not at runtime
4. **No Symbolic Equality**: Can't check if `x+y` and `y+x` are symbolically equal

## Integration with Testing

The debugging utilities integrate with the `unit.h` testing framework:

```cpp
#include "symbolic3/debug.h"
#include "unit.h"

auto main() -> int {
  "Property tests"_test = [] {
    constexpr Symbol x;
    static_assert(expression_depth(x) == 0);
    static_assert(operation_count(x + x) == 1);
  };

  return TestRegistry::result();
}
```

All checks run at compile time, so test failures appear as compilation errors!

## See Also

- **`symbolic3/to_string.h`** - String conversion (runtime)
- **`symbolic3/simplify.h`** - Simplification strategies
- **`symbolic3/strategy.h`** - Strategy composition
- **`SYMBOLIC3_RECOMMENDATIONS.md`** - Best practices and patterns
