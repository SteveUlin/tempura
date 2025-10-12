# Research Summary: Simplification Test Fixes

## What I Did

1. **Fixed all simplification tests** to use structural assertions (`static_assert` with `match`) instead of evaluation-based testing
2. **Investigated failing tests** to determine root causes
3. **Documented bugs** in the simplification library with detailed analysis

## Key Findings

### ✅ What Works

- **Basic simplification rules**: All identity, power, and like-terms rules work correctly
- **Individual rule application**: Factoring rules fire and produce correct patterns
- **Constant folding**: Works when applied directly to constant expressions
- **Nested simplification**: `x * (2 + 1)` correctly simplifies to `x * 3`

### ❌ What's Broken

- **Factoring + constant folding pipeline**: `x * 2 + x` produces `x * (2 + 1)` but doesn't fold the nested `(2 + 1)` to `3`
- **Multiple nested simplifications**: Complex expressions with multiple levels of nesting don't fully simplify

## Root Cause Analysis

The bug is in the **full_simplify pipeline**, not in the individual rules.

### The Pipeline Structure

```cpp
constexpr auto full_simplify = [](auto expr, auto ctx) {
  return FixPoint{innermost(simplify_fixpoint)}.apply(expr, ctx);
};
```

Where `simplify_fixpoint = FixPoint{algebraic_simplify}`.

### The Problem

1. Factoring transforms `x * 2 + x` → `x * (2 + 1)`
2. The outer fixpoint sees no change at the top level (structure changed once)
3. The `innermost` traversal should go into `(2 + 1)` and simplify it
4. **But it doesn't happen** - the nested constant expression stays unsimplified

### Why This Happens

Possible explanations:

- The double fixpoint (`FixPoint{innermost(FixPoint{...})}`) terminates too early
- The `innermost` traversal doesn't properly handle multiplication's nested arguments
- The fixpoint check compares expressions before and after, missing nested changes

## Test Results Summary

### Basic Tests (`simplification_basic_test.cpp`)

**Status**: ✅ **12/12 tests pass**

All basic rules verified with structural assertions:

- Power rules: `x^0 → 1`, `x^1 → x`
- Addition identity: `x + 0 → x`, `0 + x → x`
- Multiplication identity: `x * 1 → x`, `1 * x → x`, `x * 0 → 0`

### Advanced Tests (`simplification_advanced_test.cpp`)

**Status**: ✅ **25/25 tests pass**

All transcendental function tests verified:

- Logarithm rules
- Exponential rules
- Trigonometric rules
- Square root rules
- Mathematical constants (π, e)

### Pipeline Tests (`simplification_pipeline_test.cpp`)

**Status**: ⚠️ **8 bugs identified**

Tests correctly fail, identifying these bugs:

1. `x*2 + x` doesn't simplify to `x*3`
2. `x*2 + x*3` doesn't simplify to `x*5`
3. `x + x*2` doesn't simplify to `x*3`
4. `x*2 + x*3 + x*4` doesn't simplify to `x*9`
5. `(x + 0) * 1 + 0` doesn't simplify to `x`
6. `(x * 1) + (y * 0)` doesn't simplify to `x`
7. `sin(0) + cos(0) * x` doesn't simplify to `x`
8. `x * (y + (z * 0))` doesn't simplify to `x * y`

## Experimental Tests Created

To diagnose the issues, I created several test programs:

1. **`/tmp/test_like_terms2.cpp`** - Verified LikeTerms rule works (✓)
2. **`/tmp/test_full_simplify.cpp`** - Verified `x + x → 2*x` works (✓)
3. **`/tmp/test_factoring.cpp`** - Found factoring works but result isn't folded (✗)
4. **`/tmp/test_nested_constant_fold.cpp`** - Verified `x * (2 + 1) → x * 3` works (✓)
5. **`/tmp/test_step_by_step.cpp`** - Confirmed factoring works but full_simplify doesn't fold nested constants (✗)
6. **`/tmp/test_pipeline_stages.cpp`** - Pinpointed that `algebraic_simplify` produces `x * (2 + 1)` without folding (✗)

## Recommended Next Steps

### Option 1: Fix the Full Simplify Pipeline

Investigate and fix why `FixPoint{innermost(simplify_fixpoint)}` doesn't fold nested constants:

- Debug the `innermost` traversal strategy
- Check if fixpoint terminates prematurely
- Ensure constant_fold runs on all nested expressions

### Option 2: Add Explicit Constant Folding Pass

Add an explicit constant folding pass after factoring:

```cpp
constexpr auto full_simplify_fixed = [](auto expr, auto ctx) {
  auto step1 = FixPoint{innermost(simplify_fixpoint)}.apply(expr, ctx);
  // Explicit nested constant folding pass
  auto step2 = innermost(constant_fold).apply(step1, ctx);
  // Repeat until stable
  return FixPoint{step2 == expr ? Identity{} : full_simplify_fixed}.apply(step2, ctx);
};
```

### Option 3: Change Factoring Rules to Fold Immediately

Modify factoring rules to fold constants immediately:

```cpp
// Instead of: x·a + x → x·(a+1)
// Use: x·a + x → x·<evaluate(a+1)> when a is constant
```

## Files Modified

- ✅ `src/symbolic3/test/simplification_basic_test.cpp` - All tests use structural assertions
- ✅ `src/symbolic3/test/simplification_advanced_test.cpp` - All tests use structural assertions
- ✅ `src/symbolic3/test/simplification_pipeline_test.cpp` - All tests use structural assertions (reveals bugs)
- ✅ `SIMPLIFICATION_TESTS_FIXED.md` - Documentation of changes
- ✅ `FACTORING_BUG_ANALYSIS.md` - Detailed bug analysis
- ✅ `RESEARCH_SUMMARY.md` - This file

## Conclusion

The tests are now **correct and properly identify real bugs** in the simplification library. The bugs are not in the individual rules, but in how the `full_simplify` pipeline composes them. The factoring rules work, constant folding works, but they don't compose correctly in the fixpoint/innermost pipeline.

This is exactly what good tests should do - identify bugs that need to be fixed in the implementation.
