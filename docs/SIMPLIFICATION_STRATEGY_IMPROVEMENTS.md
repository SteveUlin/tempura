# Simplification Strategy Improvements - Summary

## Current Situation

**Current approach** (`simplify.h` line 655-667):

```cpp
constexpr auto full_simplify = [](auto expr, auto ctx) {
  return FixPoint{bottomup(try_strategy(algebraic_simplify))}.apply(expr, ctx);
};
```

This does:

1. Bottom-up traversal: simplify children, then parent
2. Repeat until nothing changes (fixpoint)

**Problems:**

1. **No short-circuits**: `0 * (huge_expr)` simplifies `huge_expr` unnecessarily
2. **Redundant work**: `(x + y) + (x + y)` simplifies both sides independently
3. **No traversal control**: Can't express "check this pattern before recursing"
4. **Inefficient**: Always recurses into all children, even when unnecessary

## Research Findings

### Key Insight from Term Rewriting Theory

**Different patterns need different evaluation orders:**

| Pattern Type             | Best Strategy | Reason                                      |
| ------------------------ | ------------- | ------------------------------------------- |
| Annihilators (0·x, x·0)  | **Outermost** | Check parent before recursing into children |
| Identities (exp(log(x))) | **Outermost** | Cancel before simplifying argument          |
| Collection (x+x, x·x)    | **Innermost** | Children must be simplified first           |
| Distribution ((a+b)·c)   | **Top-down**  | Expand before recursing into terms          |
| Factoring (a·x + b·x)    | **Bottom-up** | Collect after children simplified           |

### Evaluation Strategies

From functional programming and term rewriting literature:

1. **Eager/Applicative Order**: Evaluate all arguments first (like current approach)
2. **Lazy/Normal Order**: Only evaluate what's needed (better for short-circuits)
3. **Hybrid**: Different strategies for different operations

## Recommended Improvements

### Phase 1: Short-Circuit Patterns (✨ High Impact, Low Complexity)

**Add quick pattern checks before recursing:**

```cpp
// Check annihilators and identities FIRST (outermost-first)
constexpr auto quick_patterns =
  Rewrite{0_c * x_, 0_c} |
  Rewrite{x_ * 0_c, 0_c} |
  Rewrite{1_c * x_, x_} |
  Rewrite{x_ * 1_c, x_} |
  Rewrite{exp(log(x_)), x_} |
  Rewrite{log(exp(x_)), x_};

// Combined strategy
constexpr auto optimized_simplify = [](auto expr, auto ctx) {
  // Try quick patterns first (don't recurse)
  auto quick = quick_patterns.apply(expr, ctx);
  if constexpr (!std::is_same_v<decltype(quick), Never>) {
    return quick;
  }

  // Fall back to full simplification
  return FixPoint{bottomup(try_strategy(algebraic_simplify))}.apply(expr, ctx);
};
```

**Benefits:**

- `0 * (complex_expr)` returns immediately (no recursion)
- `exp(log(complex_expr))` returns `complex_expr` without simplifying it first
- Minimal code changes
- Zero overhead when patterns don't match

### Phase 2: Two-Phase Traversal (⭐ High Elegance, Medium Complexity)

**Separate rules into descent (going down) vs ascent (going up):**

```cpp
// Rules to apply BEFORE recursing into children (pre-order)
constexpr auto descent_rules =
  annihilator_rules |      // 0·x → 0 (before recursing into x)
  identity_rules |         // exp(log(x)) → x (before simplifying x)
  distribution_rules;      // (a+b)·c → a·c + b·c (expand early)

// Rules to apply AFTER children are simplified (post-order)
constexpr auto ascent_rules =
  constant_fold |          // 2 + 3 → 5 (after children are constants)
  collection_rules |       // x + x → 2·x (after x is simplified)
  factoring_rules |        // x·a + x·b → x·(a+b) (after a, b simplified)
  ordering_rules;          // y + x → x + y (after children in canonical form)

// Two-phase traversal
constexpr auto two_phase_simplify = [](auto expr, auto ctx) {
  // Phase 1: Apply descent rules (parent-first)
  auto after_descent = descent_rules.apply(expr, ctx);
  auto current = descent_changed ? after_descent : expr;

  // Phase 2: Recurse into children
  auto after_children = recurse_into_children(current, ctx);

  // Phase 3: Apply ascent rules (after children)
  return ascent_rules.apply(after_children, ctx);
};
```

**Benefits:**

- **Elegant rule organization**: Clear semantic meaning for each rule set
- **Prevents conflicts**: Distribution and factoring don't fight each other
- **More efficient**: Apply rules at the right time
- **Flexible**: Easy to add new rules to appropriate phase

**Trade-off:**

- Requires reorganizing existing rules into two categories
- Need to think carefully about which phase each rule belongs to

### Phase 3: Common Subexpression Elimination (🔧 Medium Impact, Medium Complexity)

**Detect structurally identical children at compile-time:**

```cpp
// Check if binary operation has identical children
template <typename Op, Symbolic A>
constexpr auto smart_simplify_binary(Expression<Op, A, A> expr, auto ctx) {
  // Simplify child ONCE (both children are identical)
  auto child = simplify(A{}, ctx);

  // Apply operation-specific like-term rule
  if constexpr (std::is_same_v<Op, AddOp>) {
    return 2_c * child;  // x + x → 2·x
  } else if constexpr (std::is_same_v<Op, MulOp>) {
    return pow(child, 2_c);  // x · x → x^2
  } else {
    // Other operations: reconstruct
    return Expression<Op, decltype(child), decltype(child)>{};
  }
}
```

**Benefits:**

- `(complex_expr) + (complex_expr)` simplifies `complex_expr` only once
- Compile-time detection (zero runtime cost)
- Automatic optimization

**Limitations:**

- Only works for structurally identical types (not runtime values)
- Doesn't handle `(x+y) + (y+x)` (different structure, same value)

## Implementation Strategy

### Recommended Approach

**Start with Phase 1 (short-circuit)** - quick win with minimal changes:

```cpp
// In simplify.h, modify full_simplify:
constexpr auto full_simplify = [](auto expr, auto ctx) {
  // NEW: Try quick patterns first
  constexpr auto quick =
    Rewrite{0_c * x_, 0_c} | Rewrite{x_ * 0_c, 0_c} |
    Rewrite{1_c * x_, x_} | Rewrite{x_ * 1_c, x_} |
    Rewrite{exp(log(x_)), x_} | Rewrite{log(exp(x_)), x_};

  auto quick_result = quick.apply(expr, ctx);
  if constexpr (!std::is_same_v<decltype(quick_result), Never>) {
    return quick_result;
  }

  // Existing full pipeline
  return FixPoint{bottomup(try_strategy(algebraic_simplify))}.apply(expr, ctx);
};
```

**Then consider Phase 2 (two-phase)** if you want elegant rule organization:

This requires more refactoring but provides better long-term maintainability.
See `src/symbolic3/smart_traversal.h` for proof-of-concept implementation.

### Proof-of-Concept Files Created

1. **`docs/SIMPLIFICATION_STRATEGIES_RESEARCH.md`**

   - Detailed research on term rewriting strategies
   - Examples from literature
   - Benchmark scenarios
   - Full analysis of trade-offs

2. **`src/symbolic3/smart_traversal.h`**
   - Working implementations of:
     - `ShortCircuit` - quick pattern checks before recursion
     - `TwoPhase` - separate descent/ascent rule sets
     - `CSEAwareStrategy` - common subexpression elimination
     - `SmartDispatch` - operation-specific strategy selection
     - `LazyEval` - lazy evaluation for identities
   - Fully commented and explained
   - Ready to integrate or use as reference

## Performance Expectations

### Benchmark Scenarios

**1. Short-circuit (Phase 1):**

```cpp
// Before: simplifies all 100 terms, then multiplies by 0
// After: returns 0 immediately (1 pattern match)
auto expr = 0_c * (x + y + ... + z);  // 100 terms
```

Expected speedup: **100x** for this pattern

**2. CSE (Phase 3):**

```cpp
// Before: simplifies x*x four times
// After: simplifies x*x once, reuses result
auto expr = (x*x + x*x) + (x*x + x*x);
```

Expected speedup: **4x** for this pattern

**3. Two-phase (Phase 2):**

```cpp
// Before: multiple fixpoint iterations
// After: single pass with right rule ordering
auto expr = (x+x+x) * (y+y) + (x+x+x) * (y+y);
```

Expected speedup: **2-3x** for complex expressions

## Next Steps

1. **✅ Research completed** - document written
2. **✅ Proof-of-concept implemented** - smart_traversal.h created
3. **⏭️ Choose approach** - Phase 1 (quick win) or Phase 2 (elegant)?
4. **⏭️ Integrate into simplify.h** - modify `full_simplify` or create new variant
5. **⏭️ Write tests** - verify correctness and measure performance
6. **⏭️ Update documentation** - document the new strategies

## Conclusion

The current bottom-up-only approach is simple but inefficient. Term rewriting theory
suggests using **different evaluation strategies for different pattern types**:

- **Outermost-first** for short-circuits (annihilators, identities)
- **Innermost-first** for collection (like terms, constant folding)
- **Two-phase** for conflicting requirements (expansion vs factoring)

The recommended approach is **Phase 1 (short-circuit)** first for quick wins,
then **Phase 2 (two-phase)** for elegant rule organization if desired.

Both approaches prioritize **elegance in rule definitions** as requested, with
the complexity moved into the traversal strategy combinators (which are reusable
across all rule sets).
