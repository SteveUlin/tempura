# Related Problems to Consider Before Implementation

## Overview

Before implementing the simplification strategy improvements, we should address several related design issues that could interact with or be solved by the new architecture.

---

## 1. 🔴 **CRITICAL: Distribution vs Factoring Oscillation**

### Current Problem

**Location:** `src/symbolic3/simplify.h:428`

Distribution is **currently disabled** due to conflict with factoring:

```cpp
// DISABLED - conflicts with Factoring
// Distribution: x·(a+b) → x·a + x·b  [expand]
// Factoring:    x·a + x·b → x·(a+b)  [collect]
```

These are inverse operations that cause infinite oscillation:

```
x*(2+3) → x*2 + x*3 → x*(2+3) → x*2 + x*3 → ...
```

### Why This Matters for New Design

The **two-phase strategy** directly addresses this problem!

```cpp
// Descent phase: Expand when beneficial
constexpr auto descent_rules =
  when([](expr) { return should_expand(expr); }, distribution);

// Ascent phase: Collect after children simplified
constexpr auto ascent_rules = factoring;
```

**Decision Point:**

- Should distribution be **always disabled** (current approach)?
- Or **conditionally enabled** in descent phase (new approach)?
- When is expansion beneficial? (e.g., before differentiation, never before evaluation)

### Recommended Action

✅ **Design conditional distribution rules** as part of two-phase implementation

- Descent: Expand if it enables other simplifications
- Ascent: Factor if it reduces term count
- Never allow both in same phase

---

## 2. ⚠️ **Pattern Matching Performance**

### Current Problem

Every simplification attempt pattern-matches **all rules** against **every node**.

Example: For expression tree with 100 nodes and 50 rules = 5,000 pattern matches minimum.

### Interaction with New Design

Short-circuit strategy makes this **more critical**:

```cpp
// Check quick patterns FIRST (most common)
constexpr auto quick = annihilators | identities;  // ~10 patterns

// Then full rules (less common)
constexpr auto full = all_algebraic_rules;  // ~50 patterns
```

**Optimization Opportunity:**
Pattern matching could be optimized by:

1. **Type-based dispatch** (check operation type before pattern matching)
2. **Rule indexing** (group rules by root operation)
3. **Trie structure** (share common pattern prefixes)

### Example: Type-Based Dispatch

```cpp
// Instead of checking all 50 rules:
template <Symbolic Expr>
constexpr auto smart_match(Expr expr, auto ctx) {
  if constexpr (is_multiplication<Expr>) {
    // Only check multiplication rules (~10 patterns)
    return mult_rules.apply(expr, ctx);
  } else if constexpr (is_addition<Expr>) {
    // Only check addition rules (~10 patterns)
    return add_rules.apply(expr, ctx);
  }
  // ...
}
```

**Decision Point:**

- Should we implement type-based rule dispatch?
- How much does pattern matching contribute to compile time?
- Is the complexity worth it?

### Recommended Action

⏸️ **Measure first, optimize later**

- Profile compile times with current implementation
- Add type-based dispatch only if pattern matching is bottleneck
- The `SmartDispatch` in `smart_traversal.h` already implements this

---

## 3. ⚠️ **Fixpoint Detection Efficiency**

### Current Problem

Fixpoint iteration compares **entire expression types** for equality:

```cpp
template <Strategy S>
struct FixPoint {
  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    auto result = strategy.apply(expr, ctx);
    if constexpr (std::is_same_v<decltype(result), Expr>) {
      return result;  // No change, done
    } else {
      return apply(result, ctx);  // Changed, try again
    }
  }
};
```

**Problem:** Can't detect **value-level** oscillations with **different types**:

```cpp
// Type changes but value oscillates:
x + (2 + 3)  →  x + (3 + 2)  →  x + (2 + 3)  →  ...
                 ↑ Different type (ordering changed)
                 ↑ But fixpoint doesn't detect oscillation!
```

### Interaction with New Design

Two-phase strategy could **reduce fixpoint iterations** significantly:

- Current: Multiple passes until no change (expensive)
- Two-phase: Single pass with right rule ordering (efficient)

**But:** Still need fixpoint for nested cases like `(x+x)+(x+x)` → `2*x + 2*x` → `2*(2*x)` → `4*x`

### Recommended Action

✅ **Document fixpoint limitations** and rely on two-phase to minimize iterations

- Two-phase reduces need for fixpoint (most cases one pass)
- Keep fixpoint as safety net for nested cases
- Consider iteration limit (e.g., max 10 passes) to catch oscillations

---

## 4. ⚠️ **Constant Folding Timing**

### Current Problem

**Location:** `src/symbolic3/simplify.h:653`

Constant folding position in pipeline matters:

```cpp
// Current: constant_fold BEFORE structural rules
constexpr auto algebraic_simplify = constant_fold | PowerRules | ...;
```

**Why this ordering?** Prevents oscillation with canonical ordering:

```
x*(2+1) without fold → x*(1+2) by ordering → x*(2+1) → ...  [oscillation]
x*(2+1) with fold    → x*3                              ✓  [converges]
```

### Interaction with New Design

Two-phase gives more control:

```cpp
// Descent: Don't fold yet (might need to expand)
constexpr auto descent = distribution | unwrapping;

// Ascent: Fold after everything simplified
constexpr auto ascent = constant_fold | collection | ordering;
```

**Decision Point:**

- Should constant folding be in descent, ascent, or both?
- Does it prevent useful symbolic transformations?

### Recommended Action

✅ **Constant folding in ascent phase only**

- Descent: Keep symbolic forms (enables pattern matching)
- Ascent: Fold after all symbolic transformations done
- Exception: Quick-check for pure constant expressions

---

## 5. 🟡 **Associativity Reordering Explosion**

### Current Problem

Associative operations have **factorial** number of equivalent forms:

```
a + b + c + d  has 24 different parenthesizations:
  ((a+b)+c)+d
  (a+(b+c))+d
  (a+b)+(c+d)
  a+((b+c)+d)
  a+(b+(c+d))
  ...
```

Current approach uses **predicates** to enforce canonical order, but still explores many branches.

### Interaction with New Design

Two-phase with proper ordering could reduce exploration:

```cpp
// Ascent: Reorder AFTER children canonical
constexpr auto ascent = ordering | associativity;
```

This ensures:

1. Children are already in canonical form
2. Parent reordering considers canonical children
3. Fewer rewrite attempts needed

### Recommended Action

⏸️ **Monitor but don't optimize prematurely**

- Current predicate-based approach works
- Two-phase may naturally improve this
- Consider more sophisticated ordering later (e.g., term frequency)

---

## 6. 🟡 **Subexpression Depth Limits**

### Current Problem

Deep expression trees cause **exponential traversal time**:

```
Tree depth 20: 2^20 = ~1M nodes to visit
Tree depth 30: 2^30 = ~1B nodes (compile-time explosion)
```

No current depth limits or early termination.

### Interaction with New Design

Short-circuit helps but doesn't solve:

- `0 * (depth-1000 tree)` → short-circuits ✓
- `1 * (depth-1000 tree)` → must traverse entire tree ✗

**Mitigation Strategies:**

1. **Depth limits** - stop recursing after N levels
2. **Breadth limits** - don't simplify expressions wider than M terms
3. **Timeout** - compile-time "budget" per simplification
4. **Lazy evaluation** - mark subtrees as "simplified" without actually simplifying

### Recommended Action

⏸️ **Document limits, implement if needed**

- Most real expressions are shallow (< 20 levels)
- Pathological cases (auto-generated, recursive definitions) may need limits
- Add depth parameter to context if becomes issue

---

## 7. 🟢 **Context Threading**

### Current Observation

Context is passed through but rarely used:

```cpp
auto result = simplify(expr, default_context());
                              ↑ Almost always default
```

### Opportunity with New Design

Context could control traversal strategy:

```cpp
struct SimplifyContext {
  enum class Mode { Fast, Thorough, Symbolic, Numeric };
  Mode mode = Mode::Thorough;
  int max_depth = 100;
  bool allow_distribution = false;
};

// Use context to select strategy
auto result = simplify(expr, SimplifyContext{
  .mode = Mode::Fast,  // Use short-circuit aggressively
  .allow_distribution = true  // Enable expansion
});
```

### Recommended Action

✅ **Design extensible context** from the start

- Start simple (empty context)
- Add fields as needs arise
- Keep default behavior unchanged

---

## 8. 🟢 **Memoization / Caching**

### Current Observation

No caching of simplified subexpressions. Common subexpressions simplified repeatedly:

```cpp
// Both children simplified independently
(f(x,y,z) + g(x,y,z)) * (f(x,y,z) + g(x,y,z))
           ↑                        ↑
     Simplified twice (duplicate work)
```

### Interaction with New Design

CSE (Phase 3) addresses **structurally identical** children, but not **equivalent** ones:

```cpp
// CSE catches this (same type):
(f(x) + f(x))  ✓

// CSE misses this (different types):
(f(x) + g(y)) + (g(y) + f(x))  ✗
```

**Compile-Time Memoization Challenges:**

- Hash table not available at compile-time
- Type-based lookup only
- Can't detect value-level equality

**Runtime Memoization:**

- Add `std::unordered_map<Hash, Expr>` to context
- Hash based on expression structure
- Only helpful for runtime evaluation, not compile-time

### Recommended Action

⏸️ **Start with CSE, consider runtime memo later**

- CSE handles 80% of cases (structurally identical)
- Full memoization complex and may not justify cost
- Reconsider if profiling shows duplicate work is significant

---

## 9. 🟢 **User-Defined Rules**

### Current Observation

All rules hardcoded in `simplify.h`. Users can't easily add domain-specific rules.

### Opportunity with New Design

Two-phase makes it easier to integrate custom rules:

```cpp
// User defines custom rules
constexpr auto physics_rules =
  Rewrite{E_ * 0_c, mass * c * c} |  // E=mc²
  Rewrite{p_ * v_, mass * v_ * v_};  // p=mv

// Integrate with standard pipeline
constexpr auto physics_simplify =
  two_phase(
    descent_rules,
    physics_rules | ascent_rules  // User rules in ascent
  );
```

### Recommended Action

⏸️ **Design for extensibility, implement later**

- Keep rule organization modular
- Document how to add rules
- Consider plugin architecture later

---

## 10. 🟢 **Differentiation Interaction**

### Current Observation

**Location:** `src/symbolic3/differentiation.h`

Differentiation produces unsimplified expressions:

```cpp
auto df = diff(f, x);  // Produces (d/dx[...])
auto simplified = simplify(df, ctx);  // Then simplify
```

### Interaction with New Design

Should simplification happen **during** or **after** differentiation?

**Option A: Defer (current)**

```cpp
auto df = diff(f, x);           // Unsimplified
auto result = simplify(df, ctx); // Simplify after
```

**Option B: Interleave (new)**

```cpp
// Simplify subexpressions during differentiation
auto df = diff_and_simplify(f, x, ctx);
```

**Trade-offs:**

- Defer: Simple, composable, but may create huge intermediate expressions
- Interleave: Efficient, but couples differentiation to simplification

### Recommended Action

⏸️ **Keep separate for now**

- Current approach works well
- Allows choosing when to simplify
- Interleaving can be added as optimization later

---

## Summary & Priorities

### 🔴 Must Address Before Implementation

1. **Distribution vs Factoring** - Two-phase directly solves this
2. **Context Design** - Need to decide structure upfront

### 🟡 Should Consider During Implementation

3. **Constant Folding Timing** - Affects phase organization
4. **Fixpoint Detection** - Document limitations

### 🟢 Can Defer Until After

5. **Pattern Matching Performance** - Optimize if becomes bottleneck
6. **Associativity Explosion** - Current approach adequate
7. **Depth Limits** - Add if real expressions hit limits
8. **Memoization** - CSE handles most cases
9. **User-Defined Rules** - Design for, implement later
10. **Differentiation Interaction** - Works well as-is

---

## Recommended Implementation Order

### Phase 1: Address Critical Issues

1. ✅ Design context structure
2. ✅ Resolve distribution/factoring with two-phase
3. ✅ Document fixpoint limitations

### Phase 2: Implement Core Features

4. ✅ Short-circuit strategy
5. ✅ Two-phase traversal
6. ✅ CSE for identical children

### Phase 3: Monitor & Optimize

7. ⏸️ Profile pattern matching performance
8. ⏸️ Measure compile-time impact
9. ⏸️ Consider additional optimizations if needed

---

## Decision Matrix

| Issue                  | Blocks Implementation? | Solved by Two-Phase? | Priority    |
| ---------------------- | ---------------------- | -------------------- | ----------- |
| Distribution/Factoring | ✅ Yes                 | ✅ Yes               | 🔴 Critical |
| Pattern Performance    | ❌ No                  | ⚠️ Partially         | 🟡 Monitor  |
| Fixpoint Efficiency    | ❌ No                  | ✅ Yes               | 🟡 Document |
| Constant Folding       | ⚠️ Maybe               | ✅ Yes               | 🟡 Design   |
| Associativity          | ❌ No                  | ⚠️ Partially         | 🟢 Later    |
| Depth Limits           | ❌ No                  | ❌ No                | 🟢 Later    |
| Context Threading      | ⚠️ Maybe               | ❌ No                | 🔴 Design   |
| Memoization            | ❌ No                  | ⚠️ CSE only          | 🟢 Later    |
| User Rules             | ❌ No                  | ⚠️ Helps             | 🟢 Later    |
| Differentiation        | ❌ No                  | ❌ No                | 🟢 Later    |

---

## Key Insights

1. **Two-phase strategy naturally solves several existing problems:**

   - Distribution/factoring conflict (descent vs ascent)
   - Constant folding timing (ascent only)
   - Reduces fixpoint iterations (better ordering)

2. **Context design is critical:**

   - Affects all strategy implementations
   - Should be extensible from start
   - Keep default behavior simple

3. **Most other issues can be deferred:**

   - Current approaches work adequately
   - Can optimize based on profiling data
   - Don't over-engineer upfront

4. **The new architecture enables future improvements:**
   - User-defined rules fit naturally
   - Performance optimizations easier to add
   - Clear extension points

---

## Questions for Design Review

1. **Context Structure:** What should default context contain? Just empty struct?
2. **Distribution Rules:** Enable in descent phase or keep disabled?
3. **Fixpoint Limit:** Should we add max iteration count? (e.g., 10)
4. **Error Handling:** What happens if simplification "fails" or times out?
5. **Backwards Compatibility:** Keep old `simplify` or replace?

These questions should be answered before beginning implementation.
