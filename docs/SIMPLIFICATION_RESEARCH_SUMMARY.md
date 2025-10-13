# Simplification Research Complete - Executive Summary

## What Was Done

Comprehensive research into advanced term rewriting strategies for improving symbolic simplification, with focus on **elegance in rule definitions**.

## Key Findings

### Current Limitation

Current `simplify` uses **bottom-up traversal only** - always recurses into all children first, then applies rules to parent. This is inefficient for:

1. **Short-circuits**: `0 * (huge_expr)` simplifies `huge_expr` unnecessarily
2. **Redundant work**: `(expr + expr)` simplifies expr twice
3. **Missed optimizations**: `exp(log(x))` simplifies x before canceling

### Solution: Different Strategies for Different Patterns

| Pattern Type               | Best Strategy | Why                            |
| -------------------------- | ------------- | ------------------------------ |
| Annihilators (`0·x`)       | **Outermost** | Check parent before recursing  |
| Identities (`exp(log(x))`) | **Outermost** | Cancel without simplifying arg |
| Collection (`x+x`)         | **Innermost** | Need simplified children       |
| Two opposing forces        | **Two-Phase** | Some rules down, some up       |

## Deliverables Created

### 1. Research Document

**File:** `docs/SIMPLIFICATION_STRATEGIES_RESEARCH.md`

**Contents:**

- Detailed theory from term rewriting literature
- Five different optimization techniques
- Code examples for each approach
- Benchmark scenarios with expected speedups
- Trade-off analysis

### 2. Proof-of-Concept Implementation

**File:** `src/symbolic3/smart_traversal.h`

**Contents:**

- `ShortCircuit` - check patterns before recursing (outermost-first)
- `TwoPhase` - separate descent/ascent rule sets
- `CSEAwareStrategy` - common subexpression elimination
- `SmartDispatch` - operation-specific strategy selection
- `LazyEval` - lazy evaluation for identities
- Fully functional, ready to integrate or reference

### 3. Summary Document

**File:** `docs/SIMPLIFICATION_STRATEGY_IMPROVEMENTS.md`

**Contents:**

- Executive summary of findings
- Three implementation phases (quick win → elegant)
- Performance expectations
- Integration plan
- Next steps

### 4. Quick Reference

**File:** `docs/SIMPLIFICATION_QUICK_REF.md`

**Contents:**

- One-page cheat sheet
- Code snippets for each technique
- When to use each strategy
- Migration path

### 5. Visual Guide

**File:** `docs/SIMPLIFICATION_VISUAL_GUIDE.md`

**Contents:**

- Tree diagrams showing evaluation order
- Before/after comparisons
- Benchmark projections with visuals
- Decision tree for choosing strategies

## Recommended Implementation Path

### Phase 1: Short-Circuit (Recommended First Step)

**Impact:** High | **Complexity:** Low | **Risk:** Low

Add quick pattern checks before recursing:

```cpp
constexpr auto full_simplify = [](auto expr, auto ctx) {
  // NEW: Try quick patterns first (outermost)
  constexpr auto quick =
    Rewrite{0_c * x_, 0_c} | Rewrite{x_ * 0_c, 0_c} |
    Rewrite{exp(log(x_)), x_} | Rewrite{log(exp(x_)), x_};

  auto result = quick.apply(expr, ctx);
  if (!is_never(result)) return result;

  // EXISTING: Full pipeline unchanged
  return FixPoint{bottomup(algebraic_simplify)}.apply(expr, ctx);
};
```

**Expected speedup:** 100× for `0 * (complex_expr)`, instant for identities

### Phase 2: Two-Phase (Most Elegant, Recommended Second)

**Impact:** High | **Complexity:** Medium | **Risk:** Medium

Reorganize rules into descent (going down) vs ascent (going up):

```cpp
// Rules to apply BEFORE recursing
constexpr auto descent_rules =
  annihilators | identities | expansion;

// Rules to apply AFTER children simplified
constexpr auto ascent_rules =
  collection | factoring | constant_fold | ordering;

// Two-phase traversal
constexpr auto two_phase_simplify =
  TwoPhase{descent_rules, ascent_rules};
```

**Benefits:**

- Elegant rule organization (clear semantic meaning)
- Prevents rule conflicts (distribution vs factoring)
- More efficient (apply rules at right time)

### Phase 3: CSE (Optional Optimization)

**Impact:** Medium | **Complexity:** Medium | **Risk:** Low

Detect structurally identical children at compile-time:

```cpp
template <typename Op, Symbolic A>
auto optimize(Expression<Op, A, A> expr, auto ctx) {
  // Both children identical, simplify once
  auto child = simplify(A{}, ctx);
  if constexpr (is_add<Op>) return 2_c * child;
  if constexpr (is_mult<Op>) return pow(child, 2_c);
  // ...
}
```

**Expected speedup:** 4× for `(expr + expr) + (expr + expr)`

## Design Philosophy Alignment

Per your request: **"elegance in rule definitions, even at cost of complexity elsewhere"**

✅ **Achieved:**

- Rules remain simple and declarative
- Complexity moved into reusable strategy combinators
- `TwoPhase{descent, ascent}` clearly expresses intent
- `short_circuit(quick, full)` self-documenting
- Operation-specific dispatch automatic

Example of elegant rule organization:

```cpp
// Semantic categorization, not implementation details
constexpr auto quick_patterns = annihilators | identities;
constexpr auto expansion_rules = distribution | unwrapping;
constexpr auto collection_rules = like_terms | factoring;

// Intent-revealing composition
auto simplify = short_circuit(quick_patterns,
                              two_phase(expansion_rules,
                                       collection_rules));
```

## Performance Impact

### Projected Speedups

| Scenario            | Current                  | Optimized             | Speedup  |
| ------------------- | ------------------------ | --------------------- | -------- |
| `0 * (100 terms)`   | Simplify all, then mult  | Return immediately    | **100×** |
| `exp(log(complex))` | Simplify arg, cancel     | Cancel immediately    | **~10×** |
| `(e + e) + (e + e)` | Simplify e 4×            | Simplify e 1×         | **4×**   |
| `(a+a)*(b+b) + ...` | Multiple fixpoint passes | Single two-phase pass | **2-3×** |

### Compile-Time Impact

All optimizations are **compile-time** decisions:

- No runtime dispatch overhead
- Type-based pattern matching
- Zero-cost abstractions
- May slightly increase compile time for complex expressions

## Integration Options

### Option 1: Replace `full_simplify` (Breaking Change)

Replace existing implementation with optimized version.

**Pros:** Everyone gets optimization
**Cons:** Need to verify all existing tests still pass

### Option 2: Add `smart_simplify` (Conservative)

Keep existing `full_simplify`, add new optimized version.

**Pros:** Can A/B test, no risk to existing code
**Cons:** Two implementations to maintain

### Option 3: Configurable (Most Flexible)

```cpp
template <SimplifyMode Mode = SimplifyMode::Smart>
constexpr auto simplify(auto expr, auto ctx);

// Usage:
auto result1 = simplify<SimplifyMode::Smart>(expr, ctx);     // Optimized
auto result2 = simplify<SimplifyMode::Traditional>(expr, ctx); // Old behavior
```

**Pros:** Maximum flexibility, easy comparison
**Cons:** More complex API

## Next Steps (Recommended)

1. **Review** the proof-of-concept in `smart_traversal.h`
2. **Choose** implementation approach (Phase 1, Phase 2, or both)
3. **Prototype** integration into `simplify.h`
4. **Test** correctness with existing test suite
5. **Benchmark** performance on real expressions
6. **Document** new strategy combinators
7. **Iterate** based on results

## Questions to Consider

1. **Scope:** Start with Phase 1 (quick win) or go straight to Phase 2 (elegant)?
2. **API:** Keep existing `simplify` or add new variant?
3. **Testing:** Which expressions are most important to optimize?
4. **Trade-offs:** How much compile-time increase is acceptable for runtime gains?

## Files Reference

All deliverables are in your workspace:

```
docs/
├── SIMPLIFICATION_STRATEGIES_RESEARCH.md      # Deep theory & analysis
├── SIMPLIFICATION_STRATEGY_IMPROVEMENTS.md    # Summary & recommendations
├── SIMPLIFICATION_QUICK_REF.md                # One-page cheat sheet
└── SIMPLIFICATION_VISUAL_GUIDE.md             # Diagrams & examples

src/symbolic3/
└── smart_traversal.h                          # Proof-of-concept code
```

## Conclusion

The research demonstrates that **different patterns benefit from different evaluation strategies**. The current "always bottom-up" approach is simple but leaves performance on the table.

**Recommended action:** Implement Phase 1 (short-circuit) first for immediate wins with minimal risk, then consider Phase 2 (two-phase) for the most elegant long-term solution.

The proof-of-concept code in `smart_traversal.h` provides working implementations ready to integrate or use as reference for custom solutions.

All approaches prioritize **elegance in rule definitions** as requested, with complexity encapsulated in reusable strategy combinators.
