# Bug Analysis: Factoring + Constant Folding

## Summary

The factoring rules work correctly, but the constant folding of nested expressions created by factoring doesn't happen properly in `full_simplify`.

## Detailed Analysis

### Test Case: `x * 2 + x`

**Expected**: `x * 3` or `3 * x`
**Actual**: `x * (2 + 1)` (factored but not folded)

### Root Cause

1. **Factoring rule fires correctly**:

   - `x * 2 + x` matches `factor_simple: x·a + x → x·(a+1)`
   - Produces `x * (2 + 1)` ✓

2. **Constant folding works in isolation**:

   - `2 + 1` can be folded to `3` ✓
   - `x * (2 + 1)` simplifies to `x * 3` with `full_simplify` ✓

3. **But the pipeline doesn't connect them**:
   - `full_simplify(x * 2 + x)` produces `x * (2 + 1)` and stops ✗
   - The nested `(2 + 1)` expression is not simplified

### Why This Happens

The `full_simplify` implementation is:

```cpp
FixPoint{innermost(simplify_fixpoint)}.apply(expr, ctx);
```

Where `simplify_fixpoint = FixPoint{algebraic_simplify}`.

The issue is likely:

- After factoring creates `x * (2 + 1)`, the fixpoint sees no further changes at the _top level_
- The `innermost` should traverse into `(2 + 1)` and simplify it, but it's not happening
- Or the fixpoint terminates before the nested simplification propagates back up

### Hypothesis

The double fixpoint structure (`FixPoint{innermost(FixPoint{...})}`) may not be working as intended. The inner fixpoint might be terminating before the nested constant gets folded, and the outer fixpoint might not be re-triggering traversal.

## Reproduction

See `/tmp/test_pipeline_stages.cpp`:

- Exit code 2 indicates "Factored but not folded by algebraic_simplify"
- This confirms factoring works but constant folding doesn't apply to nested results

## Similar Bugs

All factoring tests fail with the same pattern:

- `x*2 + x` → should be `x*3`, actually `x*(2+1)`
- `x*2 + x*3` → should be `x*5`, actually `x*(2+3)`
- `x + x*2` → should be `x*3`, actually `x*(1+2)`
- `x*2 + x*3 + x*4` → should be `x*9`, actually `x*(2+3+4)` (or partially simplified)

## Recommended Fix

Need to investigate whether:

1. `innermost` is correctly traversing into multiplication arguments
2. The fixpoint iteration is applying enough passes
3. The constant_fold strategy is being triggered on nested expressions

Possible solutions:

- Ensure constant_fold runs after every transformation
- Add explicit nested traversal for constant folding
- Restructure the fixpoint to ensure nested expressions are fully simplified before moving up
