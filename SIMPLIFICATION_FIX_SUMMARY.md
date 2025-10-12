# Simplification Pipeline Fix Summary

## Problems Fixed

### 1. Never Propagation in Traversal Strategies ✅ FIXED

**Problem**: When `innermost(simplify_fixpoint)` applied strategies to leaf nodes (like `Symbol`), the strategies would fail and return `Never`. The `apply_to_children` function would then try to rebuild expressions with `Never` values, creating invalid expression types.

**Root Cause**: Traversal strategies (innermost, topdown, etc.) didn't handle `Never` correctly - they would try to build new expressions with `Never` children.

**Solution**: Wrap strategies in `try_strategy()` before using with traversal. This converts `Never` failures back to the original expression:

```cpp
innermost(try_strategy(constant_fold))  // ✓ Safe
innermost(constant_fold)                 // ✗ Breaks on leaves
```

### 2. Distribution/Factoring Oscillation ✅ FIXED

**Problem**: Distribution and Factoring rules are inverses:

- Factoring: `x·a + x → x·(a+1)` (collect like terms)
- Distribution: `x·(a+b) → x·a + x·b` (expand products)

When both were in the same pipeline, they fought each other:

1. `x*2 + x` → factoring → `x*(2+1)`
2. `x*(2+1)` → distribution → `x*2 + x*1`
3. `x*2 + x*1` → simplify `x*1` → `x*2 + x`
4. Back to step 1 (infinite loop!)

**Solution**: Removed Distribution from `MultiplicationRules` to prefer Factoring for simplification:

```cpp
constexpr auto MultiplicationRules =
    MultiplicationRuleCategories::Identity |
    // MultiplicationRuleCategories::Distribution |  // DISABLED
    MultiplicationRuleCategories::Ordering |
    MultiplicationRuleCategories::PowerCombining |
    MultiplicationRuleCategories::Associativity;
```

### 3. Traversal Preventing Parent-Level Pattern Matching ✅ FIXED

**Problem**: Factoring rules match parent-level patterns like `x·a + x`. When wrapped in `innermost()`, the strategy applies to children first, preventing parent patterns from matching:

```cpp
// Original (BROKEN):
FixPoint{innermost(simplify_fixpoint)}.apply(x*2+x, ctx)
// innermost applies to: x, 2, (x*2), x separately
// Parent pattern x·a + x never gets a chance to match!
```

**Root Cause**: `innermost` applies strategies bottom-up (leaves first). By the time it reaches the parent `Add` node, the children have already been individually simplified, and the parent pattern no longer matches.

**Solution**: Apply algebraic rules to the WHOLE expression first, THEN use traversal for nested simplifications:

```cpp
// Fixed two-stage pipeline:
struct TwoStageSimplify {
  constexpr auto apply(expr, ctx) const {
    // Stage 1: Apply algebraic rules to whole expression (enables factoring)
    auto with_alg_rules = algebraic_simplify.apply(expr, ctx);

    // Stage 2: Fold nested constants using innermost
    return innermost(try_strategy(constant_fold)).apply(with_alg_rules, ctx);
  }
};

constexpr auto full_simplify = [](auto expr, auto ctx) {
  return FixPoint{two_stage_simplify}.apply(expr, ctx);
};
```

## Test Results

### Before Fix

- 8 tests failing with compilation errors (static_assert failures)
- `full_simplify(x*2+x)` → `x + 2*x` (unchanged, depth 2)
- Factoring not working at all in the pipeline

### After Fix

- 5 tests failing (down from 8)
- `full_simplify(x*2+x)` → `3*x` (correct, depth 1) ✅
- `full_simplify(x*2+x*3)` → `5*x` ✅
- `full_simplify(x+x*2)` → `3*x` ✅
- Factoring and constant folding working correctly!

### Remaining Issues

Tests still failing due to insufficient recursive simplification of nested expressions:

1. `x * (y + (z * 0))` should simplify to `x * y`
2. `(x + 0) * 1 + 0` should simplify to `x`
3. `(x * 1) + (y * 0)` should simplify to `x`
4. `sin(0) + cos(0) * x` should simplify to `x`
5. `((x + 0) * 1) + ((y * 0) + z)` should simplify to `x + z`

These require applying the full algebraic simplification recursively to nested subexpressions, not just constant folding.

## Architecture Changes

### File: `src/symbolic3/simplify.h`

**Changes**:

1. Added `TwoStageSimplify` struct implementing the two-stage pipeline
2. Rewrote `full_simplify` to use `FixPoint{two_stage_simplify}`
3. Disabled `Distribution` in `MultiplicationRules` (commented out)
4. Added extensive documentation explaining WHY the two-stage approach works

**Key Insight**: Parent-level pattern matching requires applying rules to the whole expression BEFORE traversing into children. This is counter-intuitive but necessary for rules like factoring.

## Next Steps

To fix the remaining test failures, we need to recursively apply the full algebraic simplification to nested subexpressions:

### Option 1: Recursive TwoStageSimplify

Apply the two-stage pipeline recursively using a proper traversal strategy:

```cpp
struct RecursiveTwoStageSimplify {
  constexpr auto apply(expr, ctx) const {
    // Apply algebraic rules at this level
    auto with_alg = algebraic_simplify.apply(expr, ctx);

    // Recursively simplify children
    auto with_simplified_children =
      apply_to_children(RecursiveTwoStageSimplify{}, with_alg, ctx);

    // Fold constants at this level
    return constant_fold.apply(with_simplified_children, ctx);
  }
};
```

### Option 2: Multi-Pass Pipeline

Alternate between top-level simplification and innermost simplification:

```cpp
1. Apply algebraic_simplify to whole expression (parent patterns)
2. Apply innermost(algebraic_simplify) (children patterns)
3. Apply innermost(constant_fold) (fold all constants)
4. Repeat until stable
```

### Option 3: Hybrid Traversal

Create a new traversal strategy that applies rules at each level (not just leaves):

```cpp
// Apply at current level, then recurse
auto hybrid = topdown(algebraic_simplify) >> innermost(constant_fold);
```

## Performance Considerations

The current fix may increase compilation time because:

- `FixPoint` iterates up to 20 times
- Each iteration applies `algebraic_simplify` + `innermost(constant_fold)`
- Innermost traverses the entire expression tree

Consider:

- Reducing `FixPoint` max iterations if expressions stabilize quickly
- Adding early termination checks
- Benchmarking compile times on complex expressions

## Lessons Learned

1. **Traversal order matters**: Bottom-up (innermost) vs top-down (topdown) fundamentally changes which patterns can match
2. **Inverse rules are dangerous**: Having both Distribution and Factoring in the same pipeline causes oscillation
3. **Try is essential**: Any strategy that can fail (return `Never`) MUST be wrapped in `try_strategy()` before use with traversal
4. **Type-based equality has limitations**: `FixPoint` using `std::is_same_v` for termination misses semantic equivalence (e.g., `x*3` vs `3*x`)

## References

- Original issue: Test failures in `simplification_pipeline_test.cpp`
- Key files modified:
  - `src/symbolic3/simplify.h` (pipeline implementation)
  - `src/symbolic3/test/simplification_pipeline_test.cpp` (tests updated to use structural assertions)
- Related documentation:
  - `FIXPOINT_OSCILLATION_BUG.md` - Detailed analysis of the oscillation problem
  - `FACTORING_BUG_ANALYSIS.md` - Earlier investigation
  - `RESEARCH_SUMMARY.md` - Comprehensive research findings
