# symbolic3 to_string Simplification Complete

## Summary

Simplified `to_string.h` to use operator metadata directly, following the same pattern established for `evaluate.h`. Removed ~60 lines of redundant helper code.

## Changes Made

### 1. Added Operator Metadata (`operators.h`)

Added `kSymbol` and `kDisplayMode` static members to all operators:

```cpp
struct AddOp {
  static constexpr StaticString kSymbol = "+";
  static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;
  constexpr auto operator()(auto a, auto b) const { return a + b; }
};
```

**Operators updated:**

- Arithmetic: `AddOp`, `SubOp`, `MulOp`, `DivOp`, `PowOp`, `NegOp`
- Transcendental: `SinOp`, `CosOp`, `TanOp`, `ExpOp`, `LogOp`, `SqrtOp`

### 2. Removed Helper Functions (`to_string.h`)

**Deleted (~40 lines):**

- `HasOperatorMetadata` concept (unnecessary - all operators now have metadata)
- `getOpSymbol<Op>()` helper (replaced with `Op::kSymbol`)
- `getOpDisplayMode<Op>()` helper (replaced with `Op::kDisplayMode`)

**Updated functions:**

- `toString()`: Uses `Op::kSymbol` and `Op::kDisplayMode` directly
- `toStringRuntime()`: Uses `Op::kSymbol.c_str()` directly
- `toStringInfixImpl()`: Uses `Op::kSymbol` instead of `getOpSymbol<Op>()`

### 3. Fixed StaticString (`meta/function_objects.h`)

Fixed `c_str()` method to return raw C-array pointer:

```cpp
// Before: constexpr const char* c_str() const { return chars_.data(); }
// After:  constexpr const char* c_str() const { return chars_; }
```

## Pattern Consistency

Both `evaluate.h` and `to_string.h` now follow the same design:

1. Operators are callable structs with `operator()`
2. Operators have metadata (`kSymbol`, `kDisplayMode`)
3. Generic functions directly access operator members
4. No helper functions or type traits needed

## Test Results

All 11 symbolic3 tests pass:

```
100% tests passed, 0 tests failed out of 11
✓ symbolic3_basic_test
✓ symbolic3_matching
✓ symbolic3_pattern_binding
✓ symbolic3_simplify
✓ symbolic3_strategy_v2
✓ symbolic3_transcendental
✓ symbolic3_traversal_simplify
✓ symbolic3_full_simplify
✓ symbolic3_derivative
✓ symbolic3_evaluate
✓ symbolic3_to_string
```

## Benefits

1. **Less code**: Removed ~60 lines of helper functions
2. **Simpler**: Direct property access instead of template metaprogramming
3. **Consistent**: Same pattern as evaluate.h
4. **Type-safe**: Compile-time checks via constexpr
5. **No fallbacks**: All operators must have metadata (compile error if missing)

## Next Steps

- Comparison operators (`EqOp`, `NeqOp`, `LtOp`, etc.) are currently stubs
- Logical operators (`AndOp`, `OrOp`, `NotOp`) are currently stubs
- When implementing these, add `kSymbol` and `kDisplayMode` from the start
