# Derivative Refactoring Summary

## Overview

Updated `src/symbolic2/derivative.h` to use the new pattern matching syntax with explicit pattern annotations, improved organization, and minimal header dependencies.

## Changes Made

### 1. Header Dependencies

**Before:**

```cpp
#include "symbolic2/symbolic.h"  // Umbrella header with everything
```

**After:**

```cpp
#include "symbolic2/accessors.h"  // left(), right(), operand()
#include "symbolic2/constants.h"  // Constant literals (_c)
#include "symbolic2/matching.h"   // AnyArg, AnySymbol, AnyConstant, match()
#include "symbolic2/operators.h"  // Operator overloads (sin, cos, pow, etc.)
```

**Benefits:**

- Minimal, explicit dependencies
- Faster compilation (no umbrella header)
- Clear indication of what features are used

### 2. Pattern Annotations

Added explicit pattern comments to all differentiation rules:

**Example - Sum Rule:**

```cpp
// Sum rule: d/dx(f + g) = df/dx + dg/dx
// Pattern: AnyArg{} + AnyArg{}
template <Symbolic Expr, Symbolic Var>
  requires(match(Expr{}, AnyArg{} + AnyArg{}))
constexpr auto diff(Expr expr, Var var) { ... }
```

All rules now document:

- The mathematical rule being implemented
- The pattern being matched
- Whether chain rule is applied (for composite functions)

### 3. Code Organization

Reorganized rules into logical sections with clear headers:

```cpp
// BASE CASES - Constants and Variables
//   - d/dx(x) = 1
//   - d/dx(y) = 0 (different variable)
//   - d/dx(c) = 0 (constants)

// ARITHMETIC RULES - Addition, Subtraction, Multiplication, Division
//   - Sum rule, Difference rule, Negation rule
//   - Product rule, Quotient rule

// POWER AND EXPONENTIAL RULES
//   - Power rule, Square root
//   - Exponential, Logarithm

// TRIGONOMETRIC RULES
//   - Sine, Cosine, Tangent

// INVERSE TRIGONOMETRIC RULES
//   - Arc sine, Arc cosine, Arc tangent
```

### 4. Documentation Improvements

Enhanced the header documentation:

```cpp
// Note: This uses recursive template instantiation rather than RewriteSystem
//       because differentiation inherently requires recursive calls (chain rule).
```

This clarifies why `diff` cannot be converted to the declarative `RewriteSystem` approach used in `simplify.h`.

### 5. Test File Update

Updated `src/symbolic2/test/derivative_test.cpp` to explicitly include needed headers:

**Added:**

```cpp
#include "symbolic2/binding.h"   // BinderPack
#include "symbolic2/evaluate.h"  // evaluate()
#include "symbolic2/simplify.h"  // simplify()
```

Previously these were transitively included through the `symbolic.h` umbrella header.

## Why Not RewriteSystem?

Unlike simplification rules which can be purely declarative transformations, differentiation requires:

1. **Recursive function calls**: The chain rule requires `diff(f, var)` to appear in the replacement pattern
2. **Context preservation**: The `var` parameter must be threaded through all recursive calls
3. **Dynamic structure**: Different expression structures require different recursive patterns

Example - Product Rule:

```cpp
// d/dx(f * g) = df/dx * g + f * dg/dx
return diff(f, var) * g + f * diff(g, var);
//     ^^^^^^^^^^^^^^^^     ^^^^^^^^^^^^^^^^
//     Recursive calls to diff() - not expressible in RewriteSystem
```

The current template overloading approach with `requires` clauses is the appropriate design for this use case.

## Pattern Matching Usage

All rules use wildcard patterns from `matching.h`:

- `AnyArg{}` - Matches any symbolic expression
- `AnySymbol{}` - Matches any symbol
- `AnyConstant{}` - Matches any constant

These are combined with expression constructors to form patterns:

- `AnyArg{} + AnyArg{}` - Matches addition
- `pow(AnyArg{}, AnyArg{})` - Matches power expressions
- `sin(AnyArg{})` - Matches sine function

## Testing

All 22 derivative tests pass:

- Basic rules (constant, variable, sum, product, quotient)
- Power rule and chain rule
- Trigonometric and inverse trigonometric functions
- Complex expressions
- Evaluation of computed derivatives

## Lines of Code

- Before: 183 lines
- After: 230 lines (+47 lines)
- Increase due to: Better organization, explicit pattern comments, section headers

## Benefits

1. **Explicit patterns**: Every rule documents what pattern it matches
2. **Better organization**: Logical grouping by mathematical category
3. **Minimal dependencies**: Only includes what's needed
4. **Self-documenting**: Clear structure makes code easier to understand
5. **Zero behavioral changes**: All tests pass, API unchanged
6. **Consistent style**: Matches pattern matching approach used in `simplify.h`
