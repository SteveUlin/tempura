# Symbolic2 Combinators: Quick Reference

**Purpose:** Fast reference for using the combinator-based transformation system
**Full docs:** [SYMBOLIC2_COMBINATORS_DESIGN.md](SYMBOLIC2_COMBINATORS_DESIGN.md)

---

## Core Concepts

### Strategy

A strategy transforms symbolic expressions at compile-time.

```cpp
struct MyStrategy : Strategy<MyStrategy> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Your transformation logic
    return expr;  // or transformed version
  }
};
```

### Context

Context carries state through the transformation tree.

```cpp
// Create context
auto ctx = TransformContext{}
  .with(enable_constant_folding)     // Add tag
  .with(symbolic_mode)                // Add another
  .increment_depth<1>();              // Track depth

// Check tags
if constexpr (ctx.has<SymbolicModeTag>()) { /* ... */ }
```

---

## Composition Operators

### Sequential: `>>`

Apply strategies in sequence.

```cpp
auto pipeline = strategy1 >> strategy2 >> strategy3;
// Equivalent to: strategy3(strategy2(strategy1(expr)))
```

### Choice: `|`

Try first, if no change, try second.

```cpp
auto alternatives = strategy1 | strategy2 | strategy3;
// Try strategy1, if unchanged try strategy2, if unchanged try strategy3
```

### Conditional: `when`

Apply strategy only if predicate holds.

```cpp
auto conditional = when(
  [](auto expr, auto ctx) { return ctx.has<MyTag>(); },
  my_strategy
);
```

---

## Recursion Combinators

### FixPoint

Apply strategy repeatedly until fixed point or depth limit.

```cpp
FixPoint<MyStrategy, MaxDepth>{my_strategy}
```

### Fold (Bottom-Up)

Transform children first, then parent.

```cpp
Fold{my_strategy}
// Like: simplify(f(simplify(a), simplify(b)))
```

### Unfold (Top-Down)

Transform parent first, then children.

```cpp
Unfold{my_strategy}
// Like: simplify_children(simplify(f(a, b)))
```

### Innermost

Apply at leaves first, propagate upward.

```cpp
Innermost{my_strategy}
```

### Outermost

Apply at root first, recurse if changed.

```cpp
Outermost{my_strategy}
```

---

## Common Patterns

### Pattern 1: Simple Pipeline

```cpp
constexpr auto my_simplify =
  NormalizeSubtraction{}      // a - b → a + (-b)
  >> FoldConstants{}          // 1 + 2 → 3
  >> ApplyAlgebraicRules{};   // Simplify

Symbol x;
constexpr auto result = my_simplify.apply(x - 1_c + 2_c, DefaultContext{});
// Result: x + 1
```

### Pattern 2: Context-Aware Transformation

```cpp
// Disable folding inside trig functions
template <typename InnerStrategy>
struct TrigAware : Strategy<TrigAware<InnerStrategy>> {
  InnerStrategy inner;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    if constexpr (is_trig_function(expr)) {
      auto trig_ctx = ctx.without(ConstantFoldingEnabledTag{})
                         .with(InsideTrigTag{});
      return inner.apply(expr, trig_ctx);
    }
    return inner.apply(expr, ctx);
  }
};

// Use it
constexpr auto strategy = TrigAware{algebraic_simplify};
constexpr auto result = strategy.apply(sin(π_c / 6_c), ctx);
// π/6 preserved inside sin
```

### Pattern 3: Conditional Application

```cpp
// Apply trig identities only in symbolic mode
constexpr auto smart_trig = when(
  [](auto expr, auto ctx) { return ctx.has<SymbolicModeTag>(); },
  SimplifyTrigIdentities{}
);

auto ctx = TransformContext{}.with(symbolic_mode);
auto result = smart_trig.apply(sin(x)*sin(x) + cos(x)*cos(x), ctx);
// Result: 1 (Pythagorean identity applied)
```

### Pattern 4: Staged Transformation

```cpp
constexpr auto staged =
  // Stage 1: Normalize syntax
  NormalizationRules{}

  // Stage 2: Expand
  >> Unfold{DistributionRules{}}

  // Stage 3: Simplify
  >> FixPoint{Fold{AlgebraicRules{}}}

  // Stage 4: Final cleanup
  >> FoldConstants{};
```

### Pattern 5: Domain-Specific Context

```cpp
// Boolean algebra has different rules
struct BooleanContext {};

constexpr auto boolean_simplify = when(
  [](auto, auto ctx) { return ctx.has<BooleanContext>(); },
  RewriteStrategy{
    Rewrite{x_ * x_, x_},           // Idempotence: x ∧ x = x
    Rewrite{x_ * (x_ + y_), x_},    // Absorption: x ∧ (x ∨ y) = x
    Rewrite{-(x_ + y_), -x_ * -y_}  // De Morgan: ¬(x ∨ y) = ¬x ∧ ¬y
  }
);

auto ctx = TransformContext{}.with(BooleanContext{});
auto result = boolean_simplify.apply(p * (p + q), ctx);
// Result: p (absorption)
```

---

## Context Tags Reference

### Built-in Tags

```cpp
InsideTrigTag              // Currently inside sin/cos/tan/etc.
InsideLogTag               // Currently inside log/ln/exp
ConstantFoldingEnabledTag  // Allow aggressive constant folding
SymbolicModeTag            // Preserve symbolic forms (π, e, √2)
```

### Custom Tags

Define your own:

```cpp
struct MyCustomTag {};
inline constexpr MyCustomTag my_custom_tag;

// Use it
auto ctx = TransformContext{}.with(my_custom_tag);
if constexpr (ctx.has<MyCustomTag>()) { /* ... */ }
```

---

## Common Strategies

### Algebraic

```cpp
FoldConstants{}           // 1 + 2 → 3
ApplyAdditionRules{}      // 0 + x → x, x + x → 2x
ApplyMultiplicationRules{} // 1 * x → x, 0 * x → 0
NormalizeSubtraction{}    // x - y → x + (-y)
NormalizeDivision{}       // x / y → x * (y^-1)
```

### Trigonometric

```cpp
ApplySinRules{}           // sin(0) → 0, sin(π/2) → 1, etc.
ApplyCosRules{}           // cos(0) → 1, cos(π) → -1, etc.
ApplyPythagoreanRules{}   // sin²x + cos²x → 1
```

### Power & Exponential

```cpp
ApplyPowerRules{}         // x^0 → 1, x^1 → x, (x^a)^b → x^(ab)
ApplyExpRules{}           // exp(0) → 1, exp(ln(x)) → x
ApplyLogRules{}           // log(1) → 0, log(e) → 1
```

---

## Testing Patterns

### Unit Test

```cpp
"Strategy composes correctly"_test = [] {
  constexpr auto strat = Strategy1{} >> Strategy2{};
  Symbol x;
  constexpr auto result = strat.apply(x + 0_c, DefaultContext{});
  static_assert(match(result, x));  // Compile-time check
};
```

### Context Test

```cpp
"Context tags work"_test = [] {
  constexpr auto ctx = TransformContext{}
    .with(InsideTrigTag{})
    .with(SymbolicModeTag{});

  static_assert(ctx.has<InsideTrigTag>());
  static_assert(!ctx.has<ConstantFoldingEnabledTag>());
};
```

### Integration Test

```cpp
"Context-aware simplification works"_test = [] {
  Symbol x;

  // Outside trig: fold constants
  constexpr auto ctx1 = TransformContext{}.with(enable_constant_folding);
  constexpr auto result1 = full_simplify.apply(π_c / 6_c, ctx1);
  // Folded to numeric

  // Inside trig: preserve symbolic
  constexpr auto expr2 = sin(π_c / 6_c);
  constexpr auto result2 = full_simplify.apply(expr2, ctx1);
  // π/6 preserved
};
```

---

## Performance Tips

### DO

✅ Use `static_assert` to verify compile-time evaluation
✅ Compose strategies with operators (`>>`, `|`)
✅ Use context tags for state tracking
✅ Make strategies constexpr

### DON'T

❌ Use runtime logic in strategies (breaks constexpr)
❌ Create deep composition chains (compile time grows)
❌ Add unnecessary context checks (add complexity)
❌ Forget to forward context in custom strategies

---

## Migration from Old API

### Old Code

```cpp
#include "symbolic2/simplify.h"

constexpr auto result = simplify(expr);
```

### New Code (Optional, for advanced features)

```cpp
#include "symbolic2/strategy.h"
#include "symbolic2/strategies/algebraic.h"

// Still works (backward compatible)
constexpr auto result1 = simplify(expr);

// Or use new API
constexpr auto ctx = TransformContext{}.with(symbolic_mode);
constexpr auto result2 = my_strategy.apply(expr, ctx);
```

---

## Troubleshooting

### Problem: Strategy doesn't change expression

**Solution:** Check if pattern matches and rules apply.

```cpp
// Add debugging
struct DebugStrategy : Strategy<DebugStrategy> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    static_assert(always_false<S>, "Expression type: " /* + type info */);
    return expr;
  }
};
```

### Problem: Context tags not propagating

**Solution:** Ensure you forward context in custom strategies.

```cpp
// Wrong: creates new context
auto result = inner.apply(expr, TransformContext{});

// Right: forwards context
auto result = inner.apply(expr, ctx);
```

### Problem: Compilation timeout

**Solution:** Reduce recursion depth or simplify strategy composition.

```cpp
// Before: deep recursion
FixPoint<MyStrategy, 100>{...}

// After: reasonable depth
FixPoint<MyStrategy, 20>{...}
```

---

## Full Example

```cpp
#include "symbolic2/strategy.h"
#include "symbolic2/context.h"
#include "symbolic2/recursion.h"
#include "symbolic2/strategies/algebraic.h"
#include "symbolic2/strategies/trigonometric.h"

using namespace tempura;

// Define custom context-aware strategy
template <typename Inner>
struct MyTrigAware : Strategy<MyTrigAware<Inner>> {
  Inner inner;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    if constexpr (match(expr, sin(𝐚𝐧𝐲)) || match(expr, cos(𝐚𝐧𝐲))) {
      auto trig_ctx = ctx.without(ConstantFoldingEnabledTag{})
                         .with(InsideTrigTag{});
      return inner.apply(expr, trig_ctx);
    }
    return inner.apply(expr, ctx);
  }
};

// Build complete pipeline
constexpr auto algebraic =
  FoldConstants{}
  | ApplyAdditionRules{}
  | ApplyMultiplicationRules{};

constexpr auto trig_aware = MyTrigAware{algebraic};
constexpr auto full_pipeline = FixPoint<decltype(Fold{trig_aware}), 20>{
  Fold{trig_aware}
};

// Use it
int main() {
  Symbol x;
  constexpr auto ctx = TransformContext{}.with(enable_constant_folding);

  // Example 1: Simple algebraic
  constexpr auto expr1 = x + 0_c + 1_c;
  constexpr auto result1 = full_pipeline.apply(expr1, ctx);
  // Result: x + 1

  // Example 2: Trig with context
  constexpr auto expr2 = sin(π_c / 6_c + x);
  constexpr auto result2 = full_pipeline.apply(expr2, ctx);
  // Result: sin(π/6 + x) with π/6 preserved

  static_assert(match(result1, x + 1_c));
  return 0;
}
```

---

## Resources

- **Full Design:** [SYMBOLIC2_COMBINATORS_DESIGN.md](SYMBOLIC2_COMBINATORS_DESIGN.md)
- **Summary:** [SYMBOLIC2_GENERALIZATION_SUMMARY.md](SYMBOLIC2_GENERALIZATION_SUMMARY.md)
- **Prototype:** [prototypes/combinator_strategies.cpp](prototypes/combinator_strategies.cpp)
- **Current Code:** `src/symbolic2/simplify.h`

---

**Quick Start:** Copy the "Full Example" above and modify to your needs!
