# Factoring Rules Fix - Complete Analysis

## Problem Statement

The symbolic simplification system was failing 4 tests related to factoring expressions:

- `x*2 + x` should simplify to `3*x`
- `x*2 + x*3` should simplify to `5*x`
- `x + x*2` should simplify to `3*x`
- `x*2 + x*3 + x*4` should simplify to `9*x`

All tests were failing with `static_assert` errors in `simplification_pipeline_test.cpp`.

## Root Cause Analysis

### Investigative Process

1. **Initial hypothesis**: Pattern matching or traversal strategy issues
2. **Discovery 1**: Constant folding WAS working correctly, but `constexpr auto` added `const` qualifiers to types, causing `std::is_same_v` comparisons to fail
3. **Discovery 2**: The actual problem was deeper - expressions were being transformed but NOT to the expected form

### The Actual Problem

The issue was an **interaction between canonical ordering and factoring rules**:

1. Input: `x*2 + x`
2. After one `bottomup` pass with `algebraic_simplify`:

   - Canonical ordering swapped terms: `x*2 + x` → `x + x*2`
   - Canonical ordering within multiplication: `x + x*2` → `x + 2*x`
   - Result: `x + (2*x)` where multiplication is `Constant<2> * Symbol`

3. **The factoring rules expected symbols FIRST in multiplication**:

   ```cpp
   factor_simple_rev = Rewrite{x_ + x_ * a_, x_ * (1_c + a_)};  // Expects x*a
   ```

4. But canonical ordering puts **constants FIRST**:
   - Pattern expects: `x + (x * 2)`
   - Actual expression: `x + (2 * x)`
   - **MISMATCH** - rule doesn't fire!

### Why This Wasn't Obvious

- The old `TwoStageSimplify` approach applied rules at the top level before traversal, so canonical ordering hadn't run yet
- The new `bottomup` approach processes children first, causing canonical ordering to run BEFORE factoring rules could match
- This is a **fundamental tension** between:
  - **Canonical forms** (constants first for consistent structure)
  - **Pattern matching** (must match actual structure, not intended semantics)

## The Solution

Added **mirror factoring rules** that match the canonical (constant-first) multiplication order:

```cpp
// Original rules (symbol-first multiplication)
constexpr auto factor_simple = Rewrite{x_ * a_ + x_, x_*(a_ + 1_c)};
constexpr auto factor_simple_rev = Rewrite{x_ + x_ * a_, x_ * (1_c + a_)};
constexpr auto factor_both = Rewrite{x_ * a_ + x_ * b_, x_*(a_ + b_)};

// New rules (constant-first multiplication)
constexpr auto factor_simple_cf = Rewrite{a_ * x_ + x_, x_*(a_ + 1_c)};
constexpr auto factor_simple_rev_cf = Rewrite{x_ + a_ * x_, x_ * (1_c + a_)};
constexpr auto factor_both_cf = Rewrite{a_ * x_ + b_ * x_, x_*(a_ + b_)};

constexpr auto Factoring = factor_simple | factor_simple_rev | factor_both |
                            factor_simple_cf | factor_simple_rev_cf | factor_both_cf;
```

## Why This Works

The new rules match expressions AFTER canonical ordering has run:

1. `x*2 + x` → (canonical) → `x + 2*x`
2. `factor_simple_rev_cf` matches `x_ + a_ * x_` ✅
3. Rewrites to `x * (1 + 2)` → (constant fold) → `x * 3` → (canonical) → `3*x` ✅

## Key Insights

### 1. Type-Based Pattern Matching

- Tempura's pattern matching is **structural type matching**, not value matching
- `Symbol` uses lambda-based unique types, so each `Symbol x` declaration creates a NEW type
- Must use `constexpr Symbol x` to ensure compile-time type consistency

### 2. Const Qualifiers Matter

- `constexpr auto expr = ...` produces `const T`, not `T`
- `std::is_same_v<decltype(constexpr_var), T>` will fail
- Solution: Use `std::remove_const_t<decltype(...)>` for type comparisons

### 3. Canonical Ordering Affects Pattern Matching

- Canonical forms provide **unique normal forms** for expressions
- But rewrite rules must match the **actual canonical structure**, not the logical intent
- When canonical ordering changes (e.g., putting constants first), ALL affected rules must be updated

### 4. Strategy Interaction

- Different traversal strategies apply rules at different points in the tree
- `bottomup` processes children first → canonical ordering runs before parent-level rules
- `TwoStageSimplify` applied rules before traversal → avoided this issue
- **Neither is wrong** - they're different strategies with different tradeoffs

## Test Results

After the fix, all 18 symbolic3 tests pass:

```
100% tests passed, 0 tests failed out of 18
```

Specifically, the previously failing tests now work:

- ✅ "Factor simple: x\*2 + x"
- ✅ "Factor both sides: x*2 + x*3"
- ✅ "Factor reversed: x + x\*2"
- ✅ "Complex factoring: x*2 + x*3 + x\*4"

## Files Modified

1. **src/symbolic3/simplify.h** (lines 220-241)

   - Added 3 new factoring rules for constant-first canonical form
   - Updated `Factoring` combinator to include new rules

2. **src/symbolic3/test/CMakeLists.txt** (lines 4-7)
   - Added `simplification_pipeline_test` to build system
   - Enables running the test suite via ctest

## Lessons for Future Development

1. **Always consider canonical forms when writing rewrite rules**

   - If expressions can appear in multiple forms, rules must handle ALL forms
   - Or: ensure rules run BEFORE canonical ordering

2. **Pattern matching is literal**

   - `x*2` and `2*x` are different types, even though they mean the same thing
   - Commutativity must be explicitly encoded in rules

3. **Test with actual traversal strategies**

   - Testing individual rules is insufficient
   - Must test with full `bottomup`/`topdown`/`fixpoint` to catch ordering issues

4. **Diagnostic methodology**
   - Use forced compiler errors (`static_assert(sizeof(T)==0)`) to inspect types
   - Create minimal test cases that isolate specific transformations
   - Trace expressions through each transformation step

## Related Issues

This fix resolves the issues documented in:

- `SIMPLIFICATION_FIX_SUMMARY.md` - Previous analysis of traversal strategies
- `FIXPOINT_OSCILLATION_BUG.md` - Related to canonical ordering stability

The underlying tension between canonical forms and pattern matching remains a design consideration for future rule additions.
