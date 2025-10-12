# FixPoint Oscillation Bug in Simplification Pipeline

## Summary

The simplification pipeline has a critical bug where rules oscillate between different forms, preventing proper simplification. The immediate issue was `Never` propagation in traversal strategies, which has been fixed. However, this revealed a deeper problem: **rule oscillation**.

## Root Cause Analysis

### Issue 1: Never Propagation (FIXED)

**Problem**: When `innermost(simplify_fixpoint)` applied rules to leaf nodes (like `Symbol`), the rules would fail and return `Never`. The `apply_to_children` function would then try to rebuild expressions with `Never` values, creating invalid expression types.

**Fix**: Wrapped `simplify_fixpoint` in `try_strategy()` before using with `innermost`:

```cpp
// Before:
return FixPoint{innermost(simplify_fixpoint)}.apply(expr, ctx);

// After:
return FixPoint{innermost(try_strategy(simplify_fixpoint))}.apply(expr, ctx);
```

**Impact**: This prevents traversal from breaking, but doesn't fix the underlying simplification issues.

### Issue 2: Rule Oscillation (ACTIVE BUG)

**Problem**: Rules in `algebraic_simplify` are fighting each other, causing FixPoint to oscillate between forms:

#### Test Case: `x*2 + x`

1. `algebraic_simplify(x*2 + x)` → `x*(2+1)` (factoring works!)
2. `algebraic_simplify(x*(2+1))` → transforms to something else (depth 2, but not the expected forms)
3. The transform doesn't stabilize - rules keep rewriting in cycles

#### Diagnostic Evidence

```cpp
Step 1 - algebraic_simplify(x*2+x):
  Depth: 2
  Matches x*(2+1): 1     ← Factoring produces correct form

Step 2 - algebraic_simplify(x*(2+1)):
  Depth: 2
  Same type as step1: 0   ← Type changed!
  Matches x*(2+1): 0      ← Not the factored form
  Matches x*(1+2): 0      ← Not reordered version
  Matches x + 2*x: 0      ← Not distributed form

Step 3 - simplify_fixpoint(x*2+x):
  Depth: 2
  Matches x*(1+2): 1      ← Different term order from step 1
```

### Root Cause: Term Reordering

The issue is that **associativity and ordering rules are reversing the factoring**:

1. Factoring rule: `x·a + x → x·(a+1)` creates `x*(2+1)`
2. Some other rule (likely associativity or distribution) transforms `x*(2+1)` back
3. FixPoint detects type change and continues iterating
4. Rules cycle indefinitely (or until max iterations)

## Why Tests Are Failing

The tests expect `full_simplify(x*2+x)` to produce `x*3` or `3*x`, but:

1. Factoring produces `x*(2+1)` correctly
2. Constant folding should then convert `(2+1)` to `3`, giving `x*3`
3. But the oscillation prevents nested constant folding from happening
4. Final result is some intermediate form, not fully simplified

## Next Steps

### Option 1: Fix Rule Priority

Ensure factoring has higher priority than distribution/associativity, or add guards to prevent factored forms from being redistributed.

### Option 2: Add Canonical Form Enforcement

After each application, canonicalize the expression (sort terms, fold nested constants) before checking for fixpoint.

### Option 3: Stronger FixPoint Termination

Instead of type-based equality checking, use structural equality that ignores term order.

### Option 4: Multi-Stage Pipeline

Separate factoring/collection from other simplifications:

1. Stage 1: Collect like terms (factoring)
2. Stage 2: Fold constants (innermost)
3. Stage 3: Apply other rules (distribution, etc.)

## Status

- ✅ Never propagation fixed (wrapping in `try_strategy`)
- ⚠️ Rule oscillation active - preventing proper simplification
- ⚠️ 8 tests still failing due to incomplete simplification

## Related Files

- `src/symbolic3/simplify.h` - Rule definitions and pipeline
- `src/symbolic3/traversal.h` - Innermost strategy implementation
- `src/symbolic3/strategy.h` - FixPoint and Try combinators
- `src/symbolic3/test/simplification_pipeline_test.cpp` - Failing tests
