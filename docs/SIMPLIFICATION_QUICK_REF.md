# Simplification Strategy Quick Reference

## TL;DR

**Problem:** Current simplification always recurses into all children, even when unnecessary.

**Solution:** Use different evaluation strategies for different pattern types.

## Quick Comparison

| Scenario          | Current Behavior                                   | Optimized Behavior                   | Strategy      |
| ----------------- | -------------------------------------------------- | ------------------------------------ | ------------- |
| `0 * (huge_expr)` | Simplifies `huge_expr`, then multiplies by 0       | Returns `0` immediately              | **Outermost** |
| `exp(log(x))`     | Simplifies `x`, applies exp/log, cancels           | Cancels immediately, returns `x`     | **Outermost** |
| `x + x`           | Needs x simplified first                           | Simplifies x, then combines to `2*x` | **Innermost** |
| `(x+y) + (x+y)`   | Simplifies left, simplifies right (duplicate work) | Simplifies once, reuses result       | **CSE**       |

## Evaluation Order Cheat Sheet

```
Outermost (Lazy)
├─ Check parent pattern FIRST
├─ Only recurse if pattern doesn't match
└─ Use for: annihilators, identities, short-circuits

Innermost (Eager)
├─ Recurse into children FIRST
├─ Then check parent pattern
└─ Use for: collection, folding, like-terms

Two-Phase (Hybrid)
├─ Descent: apply rules going down (pre-order)
├─ Recurse: simplify children
├─ Ascent: apply rules coming up (post-order)
└─ Use for: complex pipelines with conflicting requirements
```

## Implementation Snippets

### Short-Circuit (Outermost-First)

```cpp
// Add before existing simplification pipeline
constexpr auto quick_check = [](auto expr, auto ctx) {
  // Try annihilators
  if (matches<0_c * x_>(expr)) return 0_c;
  if (matches<x_ * 0_c>(expr)) return 0_c;

  // Try identities
  if (matches<exp(log(x_))>(expr)) return extract_arg(expr);
  if (matches<log(exp(x_))>(expr)) return extract_arg(expr);

  // Pattern didn't match, proceed normally
  return Never{};
};

auto result = quick_check.apply(expr, ctx);
if (!is_never(result)) return result;
// else: continue with normal simplification
```

### Two-Phase Traversal

```cpp
// Categorize rules by when they should apply
constexpr auto descent_rules =
  annihilators | identities | expansion;  // Apply going DOWN

constexpr auto ascent_rules =
  collection | factoring | folding;  // Apply going UP

// Traverse with both phases
auto result = two_phase(descent_rules, ascent_rules).apply(expr, ctx);
```

### Common Subexpression Elimination

```cpp
// Detect identical children at compile-time
template <typename Op, Symbolic A>
auto optimize_binary(Expression<Op, A, A> expr, auto ctx) {
  auto child = simplify(A{}, ctx);  // Simplify ONCE

  if constexpr (is_add<Op>) return 2_c * child;      // x+x → 2x
  if constexpr (is_mult<Op>) return pow(child, 2_c); // x·x → x²
  // ...
}
```

## When to Use Each Strategy

### Use Outermost When:

- ✅ Pattern at parent level determines result
- ✅ Children don't need simplification if pattern matches
- ✅ Want to avoid unnecessary work
- Examples: `0 * x`, `1 * x`, `exp(log(x))`, `x^0`, `x^1`

### Use Innermost When:

- ✅ Need children simplified before matching pattern
- ✅ Pattern depends on simplified form of children
- ✅ Collecting like terms
- Examples: `x + x`, `x * x`, `c1 + c2` (constant folding)

### Use Two-Phase When:

- ✅ Some rules need to apply before recursion (expansion)
- ✅ Other rules need to apply after recursion (collection)
- ✅ Want to prevent rule conflicts
- Examples: Distribution vs Factoring, Expansion vs Collection

### Use CSE When:

- ✅ Subexpressions are structurally identical
- ✅ Simplification is expensive
- ✅ Want to avoid duplicate work
- Examples: `(f(x)) + (f(x))`, `(expr) * (expr)`

## Rule Organization Guidelines

### Descent Rules (Apply Before Recursion)

```cpp
// Short-circuits that avoid recursion
- Annihilators: 0·x → 0, x·0 → 0
- Identities: exp(log(x)) → x, x^1 → x, x^0 → 1
- Expansions: (a+b)·c → a·c + b·c
- Unwrapping: -(−x) → x
```

### Ascent Rules (Apply After Recursion)

```cpp
// Patterns that need simplified children
- Constant folding: 2 + 3 → 5
- Collection: x + x → 2·x
- Factoring: x·a + x·b → x·(a+b)
- Ordering: y + x → x + y (if x < y)
- Power combining: x·x^a → x^(a+1)
```

## Files to Review

1. **`docs/SIMPLIFICATION_STRATEGIES_RESEARCH.md`**

   - Deep dive into theory
   - Benchmark scenarios
   - Full examples

2. **`src/symbolic3/smart_traversal.h`**

   - Working implementations
   - Reusable strategy combinators
   - Ready to integrate

3. **`docs/SIMPLIFICATION_STRATEGY_IMPROVEMENTS.md`**
   - Summary of findings
   - Implementation plan
   - Performance expectations

## Migration Path

### Step 1: Add Quick Checks (Low Risk)

```cpp
// Modify full_simplify in simplify.h
constexpr auto full_simplify = [](auto expr, auto ctx) {
  // NEW: Quick patterns first
  auto quick = quick_patterns.apply(expr, ctx);
  if (!is_never(quick)) return quick;

  // EXISTING: Full pipeline
  return FixPoint{bottomup(algebraic_simplify)}.apply(expr, ctx);
};
```

### Step 2: Experiment with Two-Phase (Optional)

```cpp
// Create new variant for testing
constexpr auto two_phase_simplify = [](auto expr, auto ctx) {
  return TwoPhase{descent_rules, ascent_rules}.apply(expr, ctx);
};

// A/B test against existing implementation
```

### Step 3: Measure and Iterate

```cpp
// Benchmark critical expressions
benchmark("short_circuit", [] {
  return simplify(0_c * big_expr, ctx);
});

benchmark("cse", [] {
  return simplify((expr + expr) + (expr + expr), ctx);
});
```

## Key Takeaway

> **Use the right evaluation order for each pattern type.**
> Not all rules should be applied the same way.

- **Lazy (outermost)** for short-circuits
- **Eager (innermost)** for collection
- **Hybrid (two-phase)** for complex pipelines

This matches how mathematicians actually simplify expressions!
