# Simplification Traversal Strategies - Visual Guide

## Current Approach: Bottom-Up (Innermost)

```
Expression Tree:       Evaluation Order:

    * (0)                  * (0) ← 5. Finally multiply by 0
   / \                    / \
  0   +  (huge_expr)     0   +  ← 4. Add results
      / \                   / \
     x   *              ← 3. x   * ← 3. Multiply
        / \                    / \
       y   z              ← 2. y   z ← 2. Evaluate leaves

    ↑ Start here (leaves)   ↑ Result: 0 (but did all the work!)
```

**Steps:**

1. Evaluate `y` (leaf)
2. Evaluate `z` (leaf)
3. Compute `y * z`
4. Evaluate `x` (leaf)
5. Compute `x + (y * z)`
6. Evaluate `0` (leaf)
7. Compute `0 * (x + (y * z))` → **0**

**Problem:** Did 7 steps to get `0`, when we could have stopped at step 1!

---

## Optimized Approach: Short-Circuit (Outermost First)

```
Expression Tree:       Evaluation Order:

    * (0)  ← 1. Check pattern: 0 * x → 0 ✓
   / \         STOP HERE! Return 0 immediately
  0   +  (huge_expr)  ← Never evaluated
      / \
     x   *  ← Never evaluated
        / \
       y   z  ← Never evaluated
```

**Steps:**

1. Check parent pattern: `0 * x` matches! Return `0`

**Savings:** 6 steps avoided, entire right subtree never traversed!

---

## Two-Phase Approach: Descent + Ascent

### Example: `(x+x+x) * (y+y) + (x+x+x) * (y+y)`

#### Phase 1: Descent (going down, pre-order)

```
                    +
                   / \
      Check for:  *   *   ← Could factor out common term
      annihilators /\ /\     but NOT YET - children not simplified
      identities  / \ /\
                 /   X  \
                +     +  +  ← Children not in canonical form
               /|\   /\  |\
              x x x y y x x x
```

**Descent rules applied:**

- Check for `0 * x` → No match
- Check for `exp(log(x))` → No match
- Check for expansion rules → No match
- Continue to children...

#### Phase 2: Recurse (simplify children)

```
                    +
                   / \
                  *   *
                 / \ / \
    Simplify→  /   X   \  ←Simplify
              +     +   +
             /|\   /\  /|\
            x x x y y x x x
              ↓     ↓   ↓
             3x    2y  3x  ← Children now simplified
```

#### Phase 3: Ascent (coming up, post-order)

```
                    +
                   / \
    Now we can→   *   *   ← Factor: 3x * 2y appears twice
    factor!      / \ / \     Result: 2 * (3x * 2y)
                /   X   \
               3x  2y  3x
```

**Ascent rules applied:**

- Collection: Children are now simplified
- Factoring: Can now detect common factors
- Result: `2 * (3x * 2y)` → `12xy`

---

## Strategy Comparison Table

| Expression                  | Innermost Only                      | With Short-Circuit          | With Two-Phase              |
| --------------------------- | ----------------------------------- | --------------------------- | --------------------------- |
| `0 * (x+y+z)`               | 7 steps                             | **1 step** ✓                | 1 step                      |
| `exp(log(x+y))`             | 5 steps (simplify x+y, then cancel) | **1 step** ✓ (cancel first) | 1 step                      |
| `x + x`                     | 3 steps (simplify x twice, combine) | 3 steps                     | 3 steps                     |
| `(x+x)*(y+y) + (x+x)*(y+y)` | Many fixpoint iterations            | Moderate                    | **Optimal** ✓ (single pass) |

---

## Traversal Order Visual

### Innermost (Bottom-Up)

```
      5              Numbers show
     / \             evaluation order
    4   3            (leaves first)
   / \ / \
  1  2 1  2
```

**When to use:** Need children simplified before parent patterns can match

### Outermost (Top-Down with repetition)

```
      1              Try parent first
     / \             If no match, recurse
    2   3            If match, stop
   / \ / \
  4  5 6  7
```

**When to use:** Parent pattern determines result (short-circuits)

### Two-Phase (Hybrid)

```
  Descent: 1         Apply some rules going down
          / \        Recurse to children
         2   3       Apply other rules coming up
        / \ / \
       4  5 6  7
  Ascent: 8
```

**When to use:** Some rules need children simplified, others don't

---

## Common Subexpression Elimination (CSE)

### Without CSE:

```
Expression: (f(x) + f(x)) + (f(x) + f(x))

   Evaluation:              Work Done:
       +                    f(x) simplified 4 times!
      / \
     +   +                  Total: 4 × cost(simplify f(x))
    / \ / \
   f  f f  f  ← Each simplified independently
```

### With CSE:

```
Expression: (f(x) + f(x)) + (f(x) + f(x))

   Detection:               Optimization:
       +                    f(x) is same type!
      / \                   Simplify ONCE, reuse
     +   +
    / \ / \                 Total: 1 × cost(simplify f(x))
   f  f f  f  ← All structurally identical (same type)
   └──┴──┴──┘
      │
    Same! (std::is_same_v<A, A>)
```

**Savings:** 4× work → 1× work = **75% reduction**

---

## Rule Organization by Phase

```
┌─────────────────────────────────────────────────┐
│ DESCENT RULES (apply going down)               │
│ • Annihilators: 0·x → 0                        │
│ • Identities: exp(log(x)) → x                  │
│ • Expansion: (a+b)·c → a·c + b·c               │
│ • Unwrapping: −(−x) → x                        │
│                                                 │
│ Why here? These can short-circuit recursion     │
│ or need to expand before children simplify     │
└─────────────────────────────────────────────────┘
                      ↓
            ┌─────────────────┐
            │   RECURSE       │
            │ (simplify kids) │
            └─────────────────┘
                      ↓
┌─────────────────────────────────────────────────┐
│ ASCENT RULES (apply coming up)                 │
│ • Constant folding: 2 + 3 → 5                  │
│ • Collection: x + x → 2·x                      │
│ • Factoring: x·a + x·b → x·(a+b)               │
│ • Ordering: y + x → x + y                      │
│ • Power combining: x·x^a → x^(a+1)             │
│                                                 │
│ Why here? These need simplified children       │
│ to match patterns correctly                    │
└─────────────────────────────────────────────────┘
```

---

## Decision Tree: Which Strategy?

```
                 Start Here
                     |
          ┌──────────┴──────────┐
          │                     │
    Does parent         Need children
    pattern alone       simplified to
    determine result?   match pattern?
          │                     │
        YES                    YES
          │                     │
          ↓                     ↓
    ┌─────────────┐      ┌─────────────┐
    │ OUTERMOST   │      │ INNERMOST   │
    │             │      │             │
    │ Examples:   │      │ Examples:   │
    │ • 0 * x     │      │ • x + x     │
    │ • exp(log)  │      │ • const fold│
    │ • x^0, x^1  │      │ • factoring │
    └─────────────┘      └─────────────┘
          │                     │
          └──────────┬──────────┘
                     │
              Both needed?
              Some rules each way?
                     │
                    YES
                     │
                     ↓
              ┌─────────────┐
              │ TWO-PHASE   │
              │             │
              │ Descent:    │
              │ • expansion │
              │ • unwrap    │
              │             │
              │ Ascent:     │
              │ • collect   │
              │ • factor    │
              └─────────────┘
```

---

## Benchmark Results (Projected)

### Test Case: `0 * (sum of 100 terms)`

```
Current (innermost only):
├─ Simplify term 1
├─ Simplify term 2
├─ ...
├─ Simplify term 100
├─ Sum all terms
└─ Multiply by 0 → 0
   Time: ~100 units

Optimized (with short-circuit):
└─ Check pattern: 0 * x → 0 ✓
   Time: ~1 unit

Speedup: 100×
```

### Test Case: `(expr + expr) + (expr + expr)`

```
Current (no CSE):
├─ Simplify expr (copy 1)
├─ Simplify expr (copy 2)  ← Duplicate work
├─ Simplify expr (copy 3)  ← Duplicate work
└─ Simplify expr (copy 4)  ← Duplicate work
   Time: ~4 × cost(expr)

Optimized (with CSE):
├─ Detect: all 4 children identical
├─ Simplify expr ONCE
└─ Reuse result 4 times
   Time: ~1 × cost(expr)

Speedup: 4×
```

---

## Summary

**Key Insight:** Different patterns need different evaluation strategies.

1. **Short-circuits** (outermost): Check parent, avoid recursion if match
2. **Collection** (innermost): Simplify children first, then combine
3. **Hybrid** (two-phase): Some rules each way for maximum efficiency

The goal: **Elegant rule definitions** by putting complexity in reusable strategy combinators.
