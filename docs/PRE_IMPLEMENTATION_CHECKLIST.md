# Pre-Implementation Checklist

Before implementing the simplification strategy improvements, review and decide on these critical design questions.

---

## 🔴 Critical Decisions (Must Resolve)

### 1. Context Structure

**Question:** What should the context type contain?

**Options:**

**A. Empty struct (simplest)**

```cpp
struct SimplifyContext {};
constexpr auto default_context() { return SimplifyContext{}; }
```

- ✅ No changes to existing code
- ✅ Can add fields later
- ❌ Not extensible without breaking changes

**B. Configuration struct**

```cpp
struct SimplifyContext {
  bool allow_distribution = false;
  int max_depth = 100;
  int max_iterations = 10;
};
```

- ✅ Extensible
- ✅ User control
- ⚠️ Need to decide defaults

**C. Strategy selector**

```cpp
struct SimplifyContext {
  enum Mode { Fast, Thorough, Symbolic, Numeric };
  Mode mode = Mode::Thorough;
};
```

- ✅ High-level control
- ⚠️ Need to define mode behaviors
- ⚠️ Less fine-grained

**Decision:** `[ ]` Choose option: \_\_\_\_

---

### 2. Distribution Rules

**Question:** Should distribution be enabled in descent phase?

**Current:** Distribution is **DISABLED** due to conflict with factoring

**Options:**

**A. Keep disabled (safe)**

```cpp
constexpr auto descent_rules = annihilators | identities;  // No distribution
constexpr auto ascent_rules = factoring | collection;
```

- ✅ No oscillation risk
- ❌ Can't expand when beneficial

**B. Enable in descent only (controlled)**

```cpp
constexpr auto descent_rules =
  annihilators | identities | distribution;  // Enable
constexpr auto ascent_rules =
  factoring | collection;  // Won't conflict (different phase)
```

- ✅ Can expand products over sums
- ✅ Two-phase prevents oscillation
- ⚠️ Need to test thoroughly

**C. Conditional distribution**

```cpp
constexpr auto smart_distribution =
  when([](expr) { return should_expand(expr); }, distribution);
constexpr auto descent_rules =
  annihilators | identities | smart_distribution;
```

- ✅ Most flexible
- ❌ Need to define `should_expand` heuristic
- ⚠️ More complex

**Decision:** `[ ]` Choose option: \_\_\_\_

**If B or C, define when to expand:**

- `[ ]` Always expand in descent phase
- `[ ]` Only if reduces depth
- `[ ]` Only if enables other simplifications
- `[ ]` Other: ******\_\_\_\_******

---

### 3. API Design

**Question:** How to expose new functionality?

**Options:**

**A. Replace `simplify` (breaking)**

```cpp
// Old behavior removed
constexpr auto simplify = full_simplify;  // Now uses two-phase
```

- ✅ Everyone gets optimization
- ❌ Breaking change
- ⚠️ Need to verify all tests pass

**B. Add `smart_simplify` (conservative)**

```cpp
// Keep old
constexpr auto simplify = full_simplify;  // Original

// Add new
constexpr auto smart_simplify = optimized_simplify;  // New approach
```

- ✅ Backwards compatible
- ✅ Can A/B test
- ❌ Two implementations to maintain

**C. Configuration parameter**

```cpp
template <SimplifyMode Mode = SimplifyMode::Smart>
constexpr auto simplify(auto expr, auto ctx);
```

- ✅ Most flexible
- ⚠️ More complex API
- ⚠️ Requires Mode enum design

**Decision:** `[ ]` Choose option: \_\_\_\_

---

### 4. Fixpoint Iteration Limit

**Question:** Should we limit fixpoint iterations?

**Current:** Unbounded iteration until no change (can loop forever on oscillation)

**Options:**

**A. No limit (current)**

```cpp
// Keep infinite loop potential
constexpr auto result = FixPoint{strategy}.apply(expr, ctx);
```

- ✅ Simple
- ❌ Can hang on oscillation
- ❌ No protection against bugs

**B. Fixed limit**

```cpp
constexpr auto result =
  FixPoint<10>{strategy}.apply(expr, ctx);  // Max 10 iterations
```

- ✅ Prevents infinite loops
- ✅ Detects oscillations
- ⚠️ Need to choose limit (10? 20? 100?)
- ⚠️ What if legitimate case needs more?

**C. Context-configurable**

```cpp
struct SimplifyContext {
  int max_iterations = 10;
};
constexpr auto result =
  FixPoint{strategy}.apply(expr, ctx);  // Uses ctx.max_iterations
```

- ✅ User control
- ✅ Can override for special cases
- ⚠️ Need good default

**Decision:** `[ ]` Choose option: \_\_\_\_

**If B or C, what limit?** `[ ]` **\_** iterations

---

### 5. Constant Folding Placement

**Question:** When should constant folding happen?

**Options:**

**A. Ascent only (cleanest)**

```cpp
constexpr auto descent_rules = expansion | unwrapping;
constexpr auto ascent_rules = constant_fold | collection | factoring;
```

- ✅ Fold after all symbolic work done
- ✅ Clean separation
- ⚠️ May create large intermediate constant expressions

**B. Both phases**

```cpp
constexpr auto descent_rules = constant_fold | expansion;
constexpr auto ascent_rules = constant_fold | collection;
```

- ✅ Reduce expression size early
- ⚠️ May prevent symbolic pattern matching
- ⚠️ Redundant folding

**C. Before pipeline (current)**

```cpp
constexpr auto algebraic_simplify = constant_fold | PowerRules | ...;
```

- ✅ Current approach (works)
- ❌ Doesn't leverage two-phase benefits

**Decision:** `[ ]` Choose option: \_\_\_\_

---

## 🟡 Important Decisions (Should Consider)

### 6. Short-Circuit Priority

**Question:** Which patterns should be in quick-check phase?

Current suggestion:

```cpp
constexpr auto quick_patterns =
  Rewrite{0_c * x_, 0_c} |     // Annihilators
  Rewrite{x_ * 0_c, 0_c} |
  Rewrite{1_c * x_, x_} |      // Identities
  Rewrite{x_ * 1_c, x_} |
  Rewrite{exp(log(x_)), x_} |  // Inverse functions
  Rewrite{log(exp(x_)), x_};
```

**Add any others?**

- `[ ]` `x^0 → 1`
- `[ ]` `x^1 → x`
- `[ ]` `0 + x → x`
- `[ ]` `x + 0 → x`
- `[ ]` Other: ******\_\_\_\_******

**Rule of thumb:** Only patterns that:

1. Don't need children simplified
2. Are very common
3. Short-circuit large subtrees

**Decision:** Final quick pattern list: ******\_\_\_\_******

---

### 7. CSE Scope

**Question:** How aggressive should CSE be?

**Options:**

**A. Binary operations only**

```cpp
// Only check (x + x), (x * x), etc.
template <typename Op, Symbolic A>
auto optimize(Expression<Op, A, A> expr);
```

- ✅ Simple to implement
- ✅ Catches most cases
- ❌ Misses (x+y) + (x+y) + (x+y)

**B. All n-ary operations**

```cpp
// Check for any identical children in n-ary operations
template <typename Op, Symbolic... Args>
auto optimize(Expression<Op, Args...> expr);
```

- ✅ More comprehensive
- ⚠️ More complex
- ⚠️ Compile-time cost

**Decision:** `[ ]` Choose option: \_\_\_\_

---

### 8. Error Handling

**Question:** What happens if simplification fails?

**Options:**

**A. Always succeeds (current)**

```cpp
// Return original expression if nothing matches
return expr;
```

- ✅ Simple
- ❌ Can't detect errors
- ❌ Silent failures

**B. Return std::optional-like**

```cpp
auto result = simplify(expr, ctx);
if (result.failed()) {
  // Handle error
}
```

- ✅ Explicit error handling
- ⚠️ Breaks constexpr (std::optional not fully constexpr)
- ⚠️ More complex API

**C. Compile-time assertion**

```cpp
// Use Never type to signal failure
// Compilation fails if unhandled
```

- ✅ Catches errors at compile-time
- ⚠️ Can be harsh
- ⚠️ Need better error messages

**Decision:** `[ ]` Choose option: \_\_\_\_

---

## 🟢 Nice-to-Have Decisions (Can Defer)

### 9. Performance Instrumentation

**Question:** Should we add compile-time profiling?

```cpp
struct SimplifyStats {
  int nodes_visited = 0;
  int rules_applied = 0;
  int pattern_matches = 0;
};

// Pass through context
auto result = simplify(expr, ctx_with_stats);
// Print stats at compile-time
```

**Decision:**

- `[ ]` Add instrumentation now
- `[ ]` Add later if needed
- `[ ]` Not needed

---

### 10. Documentation Strategy

**Question:** How much inline documentation?

**Options:**

- `[ ]` Minimal comments (let code speak)
- `[ ]` Moderate (explain non-obvious parts)
- `[ ]` Heavy (explain every rule and phase)

**Decision:** ******\_\_\_\_******

---

## Implementation Order Confirmation

Based on decisions above, confirm implementation phases:

### Phase 1: Foundation (Week 1)

- `[ ]` Define context structure (Decision #1)
- `[ ]` Implement short-circuit strategy (Decision #6)
- `[ ]` Add fixpoint limit (Decision #4)
- `[ ]` Write basic tests

**Estimated effort:** 2-3 days

### Phase 2: Two-Phase (Week 2)

- `[ ]` Categorize rules into descent/ascent
- `[ ]` Implement TwoPhase traversal
- `[ ]` Resolve distribution/factoring (Decision #2)
- `[ ]` Place constant folding (Decision #5)
- `[ ]` Write comprehensive tests

**Estimated effort:** 4-5 days

### Phase 3: Integration (Week 3)

- `[ ]` Choose API design (Decision #3)
- `[ ]` Update existing code
- `[ ]` Verify all tests pass
- `[ ]` Benchmark performance
- `[ ]` Update documentation

**Estimated effort:** 3-4 days

### Phase 4: Polish (Week 4)

- `[ ]` Add CSE optimization (Decision #7)
- `[ ]` Performance profiling
- `[ ]` Error handling (Decision #8)
- `[ ]` User documentation
- `[ ]` Examples

**Estimated effort:** 2-3 days

---

## Risk Assessment

### High Risk Items

- `[ ]` Distribution/factoring interaction
- `[ ]` Breaking existing tests
- `[ ]` Compile-time explosion

**Mitigation:**

- Test thoroughly with existing test suite
- A/B test new vs old implementation
- Monitor compile times

### Medium Risk Items

- `[ ]` API compatibility
- `[ ]` Context design extensibility

**Mitigation:**

- Choose conservative API approach (Option B)
- Design context for future extension

### Low Risk Items

- `[ ]` CSE optimization
- `[ ]` Performance instrumentation

**Mitigation:**

- Implement incrementally
- Easy to remove if problematic

---

## Pre-Implementation Meeting Agenda

Review this checklist and decide:

1. ✅ Context structure (Decision #1)
2. ✅ Distribution rules (Decision #2)
3. ✅ API design (Decision #3)
4. ✅ Fixpoint limit (Decision #4)
5. ✅ Constant folding placement (Decision #5)

Then: 6. 📝 Update design docs with decisions 7. 🎯 Create implementation tickets 8. 🏗️ Begin Phase 1

---

## Sign-Off

**Decisions reviewed by:** ******\_\_\_\_******

**Date:** ******\_\_\_\_******

**Ready to implement:** `[ ]` Yes `[ ]` No `[ ]` With changes

**Changes needed:** ******\_\_\_\_******

---

## References

- [RELATED_PROBLEMS_ANALYSIS.md](./RELATED_PROBLEMS_ANALYSIS.md) - Full problem analysis
- [SIMPLIFICATION_STRATEGY_IMPROVEMENTS.md](./SIMPLIFICATION_STRATEGY_IMPROVEMENTS.md) - Implementation plan
- [smart_traversal.h](../src/symbolic3/smart_traversal.h) - Proof-of-concept code

---

**Status:** ⏸️ Awaiting Decisions
