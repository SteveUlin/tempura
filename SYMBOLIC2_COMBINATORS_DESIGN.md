# Generalizing Symbolic2: Combinators and Contextual Data Flows

**Created:** October 5, 2025
**Status:** Design Proposal
**Target:** symbolic2 library generalization

## Executive Summary

The current `symbolic2` system uses a **hardcoded fixpoint combinator** (`simplifySymbolWithDepth`) with a **flat if-else chain** for rule dispatch. This design proposal introduces:

1. **Generic recursion combinators** (fixpoint, fold, unfold, paramorphism, etc.)
2. **Composable transformation pipelines** (sequential, parallel, conditional)
3. **Contextual transformation contexts** (different rules inside trig functions vs outside)
4. **Stratified rewriting** (top-down, bottom-up, innermost, outermost)
5. **Modular dispatch systems** (extensible without modifying core)

**Key insight:** Transform `simplify` from a monolithic function into a **combinator algebra** where users compose custom transformation strategies.

---

## Table of Contents

1. [Current Architecture Analysis](#current-architecture-analysis)
2. [Problem Statement](#problem-statement)
3. [Core Design: Combinator Algebra](#core-design-combinator-algebra)
4. [Contextual Transformations](#contextual-transformations)
5. [Implementation Strategy](#implementation-strategy)
6. [Examples and Use Cases](#examples-and-use-cases)
7. [Migration Path](#migration-path)
8. [Advanced Features](#advanced-features)

---

## Current Architecture Analysis

### Current Simplification Pipeline

```cpp
template <Symbolic S>
constexpr auto simplify(S sym) {
  // Step 1: Bottom-up recursion (hardcoded)
  if constexpr (requires { simplifyTerms(sym); }) {
    return simplifySymbol(simplifyTerms(sym));  // Recurse into children
  } else {
    return simplifySymbol(sym);
  }
}

template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
  if constexpr (depth >= 20) {  // Hardcoded depth limit
    return S{};
  } else if constexpr (requires { applySinRules(sym); }) {  // Flat dispatch
    return trySimplify<S, depth, applySinRules<S>>(sym);
  } else if constexpr (requires { applyCosRules(sym); }) {
    return trySimplify<S, depth, applyCosRules<S>>(sym);
  }
  // ... 15 more branches ...
}
```

### Limitations

1. **Fixed recursion strategy**: Only bottom-up traversal
2. **Hardcoded dispatch**: Adding new operations requires modifying `simplifySymbolWithDepth`
3. **No context awareness**: Cannot apply different rules inside `sin(...)` vs outside
4. **Single fixpoint**: Cannot compose multiple recursive strategies
5. **Depth-only termination**: No cycle detection, memoization, or sophisticated termination
6. **Monolithic**: Cannot extract sub-strategies (e.g., "simplify polynomials only")

---

## Problem Statement

### Real-World Use Cases

#### 1. Contextual Simplification

**Problem:** Simplifying inside trigonometric arguments requires different rules.

```cpp
// Outside trig: simplify constants aggressively
simplify(π / 6 + x)  // → 0.523... + x  (constant folded)

// Inside trig: preserve symbolic form
simplify(sin(π / 6 + x))  // Want: sin(π/6 + x), NOT sin(0.523... + x)
```

**Need:** Pass transformation **context** down the tree:

- Context: `InsideTrig` → don't fold π-related constants
- Context: `OutsideTrig` → fold all constants

#### 2. Multiple Traversal Strategies

**Problem:** Different algorithms need different traversal orders.

```cpp
// Polynomial expansion: top-down (distribute before simplifying terms)
expand((x + 1) * (x + 2))  // → x² + 3x + 2

// Rational simplification: bottom-up (simplify terms before dividing)
simplify((x² - 1) / (x - 1))  // → x + 1

// Common subexpression elimination: innermost-first (share leaves)
cse(sin(x) + cos(x) + sin(x))  // → let t = sin(x) in t + cos(x) + t
```

**Need:** Pluggable **traversal strategies** (top-down, bottom-up, innermost, etc.)

#### 3. Staged Transformations

**Problem:** Some simplifications must happen in stages.

```cpp
// Stage 1: Normalize (distribute negation, flatten associativity)
normalize(-(a + b))  // → -a + -b

// Stage 2: Canonicalize (sort terms, group like terms)
canonicalize(-a + -b)  // → -(a + b)

// Stage 3: Optimize (apply domain-specific rules)
optimize(-(a + b), Context::Boolean)  // → !(a || b)  (De Morgan)
```

**Need:** **Composition operators** for sequential pipelines.

#### 4. Conditional Application

**Problem:** Apply rules only when conditions hold.

```cpp
// Apply trig identities only if we can prove angle is in [0, 2π]
simplifyTrig(sin(x), when = inRange(x, 0, 2*π))

// Apply algebraic rules only if no overflow risk
simplifyInt(x * y + x * z, when = noOverflow<int32_t>)
```

**Need:** **Conditional combinators** with predicate guards.

---

## Core Design: Combinator Algebra

### Philosophy

Transform `simplify` from a **function** into a **first-class transformation strategy** that can be:

- Composed with other strategies
- Parameterized by context
- Recursively applied
- Conditionally executed
- Introspected and analyzed

### Core Abstractions

#### 1. Transformation Strategy

A **strategy** is a compile-time function object that transforms symbolic expressions.

```cpp
template <typename Impl>
struct Strategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    return static_cast<const Impl*>(this)->apply_impl(expr, ctx);
  }
};
```

**Key properties:**

- Takes expression + context
- Returns transformed expression
- Pure (no side effects)
- Composable

#### 2. Context

A **context** carries state through the transformation.

```cpp
template <typename... Properties>
struct TransformContext {
  // Depth tracking
  static constexpr SizeT depth = /* ... */;

  // Traversal mode
  static constexpr TraversalMode mode = /* ... */;

  // Domain-specific state (e.g., "inside trig function")
  template <typename Tag>
  static constexpr bool has() { /* ... */ }

  template <typename Tag, typename Value>
  constexpr auto with(Tag, Value) const { /* ... */ }
};
```

#### 3. Basic Combinators

##### Identity Strategy

```cpp
struct Identity : Strategy<Identity> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context) const {
    return expr;  // No transformation
  }
};
```

##### Composition (Sequential)

```cpp
template <typename S1, typename S2>
struct Sequence : Strategy<Sequence<S1, S2>> {
  S1 first;
  S2 second;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    auto intermediate = first.apply(expr, ctx);
    return second.apply(intermediate, ctx);
  }
};

// Infix operator
template <typename S1, typename S2>
constexpr auto operator>>(Strategy<S1> s1, Strategy<S2> s2) {
  return Sequence{static_cast<const S1&>(s1), static_cast<const S2&>(s2)};
}
```

##### Choice (Try first, fallback to second)

```cpp
template <typename S1, typename S2>
struct Choice : Strategy<Choice<S1, S2>> {
  S1 first;
  S2 second;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    auto result = first.apply(expr, ctx);
    if constexpr (match(result, expr)) {
      return second.apply(expr, ctx);  // First didn't change anything
    } else {
      return result;
    }
  }
};

// Infix operator
template <typename S1, typename S2>
constexpr auto operator|(Strategy<S1> s1, Strategy<S2> s2) {
  return Choice{static_cast<const S1&>(s1), static_cast<const S2&>(s2)};
}
```

##### Conditional

```cpp
template <typename Pred, typename S>
struct When : Strategy<When<Pred, S>> {
  Pred predicate;
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    if constexpr (predicate(expr, ctx)) {
      return strategy.apply(expr, ctx);
    } else {
      return expr;
    }
  }
};

template <typename Pred, typename S>
constexpr auto when(Pred pred, Strategy<S> strat) {
  return When{pred, static_cast<const S&>(strat)};
}
```

#### 4. Recursion Combinators

##### Fixpoint (Current system)

```cpp
template <typename S, SizeT MaxDepth = 20>
struct FixPoint : Strategy<FixPoint<S, MaxDepth>> {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    if constexpr (ctx.depth >= MaxDepth) {
      return expr;  // Depth limit reached
    } else {
      auto result = strategy.apply(expr, ctx);
      if constexpr (match(result, expr)) {
        return result;  // Fixed point reached
      } else {
        auto new_ctx = ctx.template increment_depth<1>();
        return apply_impl(result, new_ctx);
      }
    }
  }
};
```

##### Fold (Catamorphism)

Bottom-up traversal: transform children first, then parent.

```cpp
template <typename S>
struct Fold : Strategy<Fold<S>> {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Base case: leaf nodes
    if constexpr (!requires { transform_children(expr); }) {
      return inner_strategy.apply(expr, ctx);
    } else {
      // Recursive case: transform children first
      auto expr_with_simplified_children = transform_children(
          expr, [&](auto child) {
            return apply_impl(child, ctx);
          });

      // Then transform parent
      return inner_strategy.apply(expr_with_simplified_children, ctx);
    }
  }

private:
  template <typename Op, Symbolic... Args, typename F>
  static constexpr auto transform_children(Expression<Op, Args...>, F func) {
    return Expression{Op{}, func(Args{})...};
  }
};
```

##### Unfold (Anamorphism)

Top-down traversal: transform parent first, then children.

```cpp
template <typename S>
struct Unfold : Strategy<Unfold<S>> {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Transform parent first
    auto transformed_parent = inner_strategy.apply(expr, ctx);

    // Then recurse into children
    if constexpr (!requires { transform_children(transformed_parent); }) {
      return transformed_parent;
    } else {
      return transform_children(
          transformed_parent, [&](auto child) {
            return apply_impl(child, ctx);
          });
    }
  }
};
```

##### Paramorphism (Fold with access to original structure)

```cpp
template <typename S>
struct Para : Strategy<Para<S>> {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    if constexpr (!requires { get_children(expr); }) {
      return inner_strategy.apply(expr, ctx);
    } else {
      // Pass both original and transformed children to strategy
      auto original_children = get_children(expr);
      auto transformed_children = transform_each(original_children, ctx);

      return inner_strategy.apply(expr, ctx, original_children, transformed_children);
    }
  }
};
```

##### Innermost/Outermost

```cpp
// Apply strategy to innermost (leaf-most) expressions first
template <typename S>
struct Innermost : Strategy<Innermost<S>> {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Recurse to leaves
    auto with_transformed_children = Fold{strategy}.apply(expr, ctx);

    // Then try to apply at this level
    return strategy.apply(with_transformed_children, ctx);
  }
};

// Apply strategy to outermost (root-most) expressions first
template <typename S>
struct Outermost : Strategy<Outermost<S>> {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Try to apply at this level first
    auto transformed = strategy.apply(expr, ctx);

    if constexpr (!match(transformed, expr)) {
      // Changed, try again from the top
      return apply_impl(transformed, ctx);
    } else {
      // Didn't change, recurse into children
      return Fold{strategy}.apply(expr, ctx);
    }
  }
};
```

---

## Contextual Transformations

### Problem: Context-Sensitive Rules

Different parts of an expression need different simplification strategies.

```cpp
// Example: sin(π/6 + x) should NOT fold π/6 to 0.523...
// But: y = π/6 + x SHOULD fold to y = 0.523... + x
```

### Solution: Context Tags

#### Define Context Tags

```cpp
// Tag types for marking traversal context
struct InsideTrigTag {};
struct InsideLogTag {};
struct InsidePolynomialTag {};
struct ConstantFoldingEnabledTag {};

inline constexpr InsideTrigTag inside_trig;
inline constexpr InsideLogTag inside_log;
inline constexpr ConstantFoldingEnabledTag constant_folding;
```

#### Context-Aware Strategies

```cpp
// Strategy: only fold constants if context allows
struct FoldConstantsWhenAllowed : Strategy<FoldConstantsWhenAllowed> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Check if constant folding is enabled in this context
    if constexpr (ctx.template has<ConstantFoldingEnabledTag>() &&
                  requires { foldConstants(expr); }) {
      return foldConstants(expr);
    } else {
      return expr;
    }
  }
};
```

#### Context Modification During Traversal

```cpp
// When entering a trig function, disable constant folding
template <typename InnerStrategy>
struct TrigFunctionHandler : Strategy<TrigFunctionHandler<InnerStrategy>> {
  InnerStrategy inner;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    if constexpr (match(expr, sin(𝐚𝐧𝐲)) ||
                  match(expr, cos(𝐚𝐧𝐲)) ||
                  match(expr, tan(𝐚𝐧𝐲))) {
      // Extract argument
      auto arg = operand(expr);

      // Create new context: disable constant folding, mark inside trig
      auto trig_ctx = ctx
        .template remove<ConstantFoldingEnabledTag>()
        .template with(inside_trig, true);

      // Recursively simplify argument with new context
      auto simplified_arg = inner.apply(arg, trig_ctx);

      // Reconstruct trig function
      return reconstruct_trig(expr, simplified_arg);
    } else {
      return inner.apply(expr, ctx);
    }
  }
};
```

### Example: Complete Context-Aware Pipeline

```cpp
// Define the strategy
constexpr auto context_aware_simplify =
  FixPoint{
    TrigFunctionHandler{
      Fold{
        FoldConstantsWhenAllowed{}
        >> ApplyAlgebraicRules{}
        >> ApplyTrigRules{}
      }
    }
  };

// Use it
constexpr auto init_ctx = TransformContext{}
  .with(constant_folding, true);

constexpr auto result1 = context_aware_simplify.apply(sin(π / 6), init_ctx);
// π/6 preserved inside sin, result: sin(π/6) → 1/2 (via trig table)

constexpr auto result2 = context_aware_simplify.apply(π / 6, init_ctx);
// Outside trig, constant folding allowed: π/6 → 0.523...
```

---

## Implementation Strategy

### Phase 1: Core Infrastructure (2-3 weeks)

**Goal:** Establish combinator framework without breaking existing code.

#### 1.1 Create `symbolic2/strategy.h`

```cpp
#pragma once

namespace tempura {

// Base strategy interface
template <typename Impl>
struct Strategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const;
};

// Default context
struct DefaultContext {
  static constexpr SizeT depth = 0;

  template <SizeT N>
  constexpr auto increment_depth() const {
    return DefaultContext{depth + N};
  }

  template <typename Tag>
  static constexpr bool has() { return false; }

  template <typename Tag, typename Value>
  constexpr auto with(Tag, Value) const;
};

// Identity
struct Identity : Strategy<Identity> { /* ... */ };

// Composition
template <typename S1, typename S2>
struct Sequence : Strategy<Sequence<S1, S2>> { /* ... */ };

template <typename S1, typename S2>
constexpr auto operator>>(Strategy<S1> s1, Strategy<S2> s2);

// Choice
template <typename S1, typename S2>
struct Choice : Strategy<Choice<S1, S2>> { /* ... */ };

template <typename S1, typename S2>
constexpr auto operator|(Strategy<S1> s1, Strategy<S2> s2);

}  // namespace tempura
```

#### 1.2 Create `symbolic2/recursion.h`

```cpp
#pragma once
#include "strategy.h"

namespace tempura {

// Fixpoint combinator
template <typename S, SizeT MaxDepth = 20>
struct FixPoint : Strategy<FixPoint<S, MaxDepth>> { /* ... */ };

// Fold (bottom-up)
template <typename S>
struct Fold : Strategy<Fold<S>> { /* ... */ };

// Unfold (top-down)
template <typename S>
struct Unfold : Strategy<Unfold<S>> { /* ... */ };

// Innermost
template <typename S>
struct Innermost : Strategy<Innermost<S>> { /* ... */ };

// Outermost
template <typename S>
struct Outermost : Strategy<Outermost<S>> { /* ... */ };

}  // namespace tempura
```

#### 1.3 Create `symbolic2/context.h`

```cpp
#pragma once
#include <cstddef>

namespace tempura {

// Context tag storage using template parameter pack
template <typename... Tags>
struct ContextTags {
  template <typename Tag>
  static constexpr bool has() {
    return (std::is_same_v<Tag, Tags> || ...);
  }

  template <typename Tag>
  constexpr auto add() const {
    if constexpr (has<Tag>()) {
      return *this;
    } else {
      return ContextTags<Tags..., Tag>{};
    }
  }

  template <typename Tag>
  constexpr auto remove() const {
    return ContextTags</* filter out Tag */>{};
  }
};

// Full transformation context
template <SizeT Depth = 0, typename Tags = ContextTags<>>
struct TransformContext {
  static constexpr SizeT depth = Depth;
  using tags = Tags;

  template <SizeT N>
  constexpr auto increment_depth() const {
    return TransformContext<Depth + N, Tags>{};
  }

  template <typename Tag>
  static constexpr bool has() {
    return Tags::template has<Tag>();
  }

  template <typename Tag>
  constexpr auto with(Tag) const {
    return TransformContext<Depth, typename Tags::template add<Tag>()>{};
  }

  template <typename Tag>
  constexpr auto without(Tag) const {
    return TransformContext<Depth, typename Tags::template remove<Tag>()>{};
  }
};

// Context tags
struct InsideTrigTag {};
struct InsideLogTag {};
struct ConstantFoldingEnabledTag {};
struct SymbolicModeTag {};

inline constexpr InsideTrigTag inside_trig;
inline constexpr InsideLogTag inside_log;
inline constexpr ConstantFoldingEnabledTag enable_constant_folding;
inline constexpr SymbolicModeTag symbolic_mode;

}  // namespace tempura
```

### Phase 2: Refactor Existing Code (1-2 weeks)

**Goal:** Rewrite `simplify` using combinator algebra.

#### 2.1 Convert Rule Applications to Strategies

```cpp
// Old approach
template <Symbolic S>
constexpr auto applyAdditionRules(S expr) {
  return AdditionRules.apply(expr);
}

// New approach
struct ApplyAdditionRules : Strategy<ApplyAdditionRules> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context) const {
    if constexpr (match(expr, 𝐚𝐧𝐲 + 𝐚𝐧𝐲)) {
      return AdditionRules.apply(expr);
    } else {
      return expr;
    }
  }
};
```

#### 2.2 Rewrite `simplifySymbolWithDepth` as Strategy Composition

```cpp
// Old: flat if-else chain
template <Symbolic S, SizeT depth>
constexpr auto simplifySymbolWithDepth(S sym) {
  if constexpr (depth >= 20) {
    return S{};
  } else if constexpr (requires { applySinRules(sym); }) {
    return trySimplify<S, depth, applySinRules<S>>(sym);
  }
  // ... 15 more branches
}

// New: composable strategies
constexpr auto algebraic_simplify =
  ApplySinRules{}
  | ApplyCosRules{}
  | ApplyTanRules{}
  | ApplySinhRules{}
  | ApplyCoshRules{}
  | ApplyTanhRules{}
  | FoldConstants{}
  | ApplyPowerRules{}
  | ApplyAdditionRules{}
  | NormalizeSubtraction{}
  | ApplyMultiplicationRules{}
  | NormalizeDivision{}
  | ApplyExpRules{}
  | ApplyLogRules{}
  | ApplyPythagoreanRules{};

constexpr auto simplify_strategy = FixPoint<decltype(algebraic_simplify), 20>{algebraic_simplify};
```

#### 2.3 Update `simplify` Function

```cpp
// Backward-compatible wrapper
template <Symbolic S>
constexpr auto simplify(S sym) {
  constexpr auto ctx = TransformContext{}.with(enable_constant_folding);
  return Fold{simplify_strategy}.apply(sym, ctx);
}
```

### Phase 3: Context-Aware Transformations (2-3 weeks)

**Goal:** Enable different rules in different contexts.

#### 3.1 Implement Context Propagation

```cpp
// Strategy that modifies context when entering specific expression types
template <typename Pattern, typename ContextModifier, typename InnerStrategy>
struct ContextualStrategy : Strategy<ContextualStrategy<...>> {
  Pattern pattern;
  ContextModifier modifier;
  InnerStrategy inner;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    if constexpr (match(expr, pattern)) {
      auto new_ctx = modifier(ctx);
      return inner.apply(expr, new_ctx);
    } else {
      return inner.apply(expr, ctx);
    }
  }
};
```

#### 3.2 Create Trig-Specific Strategy

```cpp
// Disable constant folding inside trig functions
constexpr auto trig_aware_simplify = ContextualStrategy{
  sin(𝐚𝐧𝐲) | cos(𝐚𝐧𝐲) | tan(𝐚𝐧𝐲),
  [](auto ctx) { return ctx.without(enable_constant_folding).with(inside_trig); },
  algebraic_simplify
};

constexpr auto full_simplify = FixPoint{Fold{trig_aware_simplify}};
```

### Phase 4: User-Extensible Dispatch (1-2 weeks)

**Goal:** Allow users to register custom strategies without modifying core.

#### 4.1 Strategy Registry

```cpp
// User-defined strategy with priority
template <int Priority, typename S>
struct WeightedStrategy {
  static constexpr int priority = Priority;
  S strategy;
};

// Registry of strategies (sorted by priority at compile-time)
template <typename... Strategies>
struct StrategyRegistry {
  // Apply in priority order
  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    return apply_sorted<sort_by_priority<Strategies...>()>(expr, ctx);
  }
};
```

#### 4.2 User Extension Example

```cpp
// User adds custom polynomial simplification
struct MyPolynomialSimplify : Strategy<MyPolynomialSimplify> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Custom logic
  }
};

// Register with high priority
constexpr auto my_simplify = StrategyRegistry{
  WeightedStrategy<100, MyPolynomialSimplify>{},  // High priority
  WeightedStrategy<50, ApplyAdditionRules>{},
  WeightedStrategy<50, ApplyMultiplicationRules>{},
  // ... existing strategies
};
```

---

## Examples and Use Cases

### Example 1: Context-Aware Trig Simplification

```cpp
// Problem: sin(π/6) should simplify to 1/2, but sin(π/6 + x) should preserve π/6

// Solution: Context-aware strategy
constexpr auto smart_trig = ContextualStrategy{
  // Pattern: any trig function
  sin(𝐚𝐧𝐲) | cos(𝐚𝐧𝐲) | tan(𝐚𝐧𝐲),

  // Modifier: when entering trig, disable aggressive constant folding
  [](auto ctx) { return ctx.with(symbolic_mode); },

  // Inner strategy: normal algebraic simplification
  algebraic_simplify
};

// Test
Symbol x;
constexpr auto expr1 = simplify(sin(π_c / 6_c));
// Result: Constant<1/2> (trig table lookup)

constexpr auto expr2 = smart_trig.apply(sin(π_c / 6_c + x), default_ctx);
// Result: sin(π/6 + x) (π/6 preserved)
```

### Example 2: Polynomial Expansion (Top-Down)

```cpp
// Strategy: expand products before simplifying terms
constexpr auto expand_strategy = Unfold{
  // At each node, distribute multiplication over addition
  RewriteStrategy{
    Rewrite{a_ * (b_ + c_), a_ * b_ + a_ * c_},
    Rewrite{(a_ + b_) * c_, a_ * c_ + b_ * c_}
  }
} >> Fold{algebraic_simplify};  // Then simplify bottom-up

// Test
Symbol x;
constexpr auto expr = (x + 1_c) * (x + 2_c);
constexpr auto expanded = expand_strategy.apply(expr, default_ctx);
// Result: x*x + 3*x + 2
```

### Example 3: Common Subexpression Elimination

```cpp
// Strategy: innermost-first to share leaves
constexpr auto cse_strategy = Innermost{
  // Build symbol table during traversal
  MemoizeStrategy{algebraic_simplify}
};

// Test
Symbol x;
constexpr auto expr = sin(x) + cos(x) + sin(x);
constexpr auto optimized = cse_strategy.apply(expr, default_ctx);
// Result: (let t = sin(x) in t + cos(x) + t)
```

### Example 4: Staged Transformation Pipeline

```cpp
// Multi-stage simplification
constexpr auto staged_simplify =
  // Stage 1: Normalize syntax (-, / → +, *)
  NormalizationStage{}

  // Stage 2: Algebraic simplification
  >> FixPoint{Fold{algebraic_simplify}}

  // Stage 3: Trig identities (requires canonical form)
  >> when([](auto expr, auto ctx) {
       return ctx.has<SymbolicModeTag>();
     },
     ApplyTrigIdentities{})

  // Stage 4: Final constant folding
  >> FoldConstants{};

// Test
Symbol x;
constexpr auto expr = sin(x) * sin(x) + cos(x) * cos(x);
constexpr auto result = staged_simplify.apply(expr, default_ctx.with(symbolic_mode));
// Result: 1 (Pythagorean identity applied in stage 3)
```

### Example 5: Domain-Specific Optimization

```cpp
// Boolean algebra simplification (different rules than numeric)
struct BooleanContext {};

constexpr auto boolean_simplify = when(
  [](auto expr, auto ctx) { return ctx.has<BooleanContext>(); },
  RewriteStrategy{
    Rewrite{x_ * x_, x_},              // Idempotence: x ∧ x = x
    Rewrite{x_ + x_, x_},              // Idempotence: x ∨ x = x
    Rewrite{x_ * (x_ + y_), x_},       // Absorption: x ∧ (x ∨ y) = x
    Rewrite{x_ * 1_c, x_},             // Identity: x ∧ true = x
    Rewrite{x_ + 0_c, x_},             // Identity: x ∨ false = x
    Rewrite{-(x_ + y_), -x_ * -y_}     // De Morgan: ¬(x ∨ y) = ¬x ∧ ¬y
  }
) | algebraic_simplify;  // Fallback to numeric rules

// Test
Symbol p, q;
constexpr auto bool_ctx = TransformContext{}.with(BooleanContext{});
constexpr auto expr = p * (p + q);
constexpr auto result = boolean_simplify.apply(expr, bool_ctx);
// Result: p (absorption law)
```

---

## Migration Path

### Phase 1: Backward Compatibility (Week 1-2)

**Goal:** Introduce combinator system without breaking existing code.

1. Create new files: `strategy.h`, `recursion.h`, `context.h`
2. Keep existing `simplify()` function unchanged
3. Add new `simplify_with_strategy()` for opt-in usage

```cpp
// Old API (unchanged)
template <Symbolic S>
constexpr auto simplify(S sym) {
  return simplifySymbolWithDepth<S, 0>(sym);
}

// New API (opt-in)
template <typename Strategy, Symbolic S, typename Context>
constexpr auto simplify_with_strategy(Strategy strat, S sym, Context ctx) {
  return strat.apply(sym, ctx);
}
```

### Phase 2: Gradual Migration (Week 3-4)

**Goal:** Reimplement `simplify()` internals using strategies.

1. Convert rule applications to strategies
2. Rewrite `simplifySymbolWithDepth` as strategy composition
3. Update `simplify()` to use new implementation
4. Verify all existing tests pass

```cpp
// Updated implementation (same signature, new internals)
template <Symbolic S>
constexpr auto simplify(S sym) {
  constexpr auto ctx = TransformContext{}.with(enable_constant_folding);
  return Fold{simplify_strategy}.apply(sym, ctx);
}
```

### Phase 3: Feature Additions (Week 5-8)

**Goal:** Add context-aware transformations.

1. Implement contextual strategies
2. Add trig-aware simplification
3. Add polynomial expansion strategy
4. Document new capabilities

### Phase 4: Deprecation (Future)

**Goal:** Eventually remove old implementation.

1. Mark `simplifySymbolWithDepth` as deprecated
2. Migrate all internal uses to strategy API
3. Remove after 2-3 release cycles

---

## Advanced Features

### 1. Memoization Strategy

```cpp
template <typename S, SizeT CacheSize = 256>
struct Memoize : Strategy<Memoize<S, CacheSize>> {
  S inner;

  // Compile-time cache using constexpr array
  mutable std::array<std::pair<TypeId, TypeId>, CacheSize> cache = {};
  mutable SizeT cache_size = 0;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    constexpr auto expr_id = kMeta<Expr>;

    // Check cache
    for (SizeT i = 0; i < cache_size; ++i) {
      if (cache[i].first == expr_id) {
        return /* reconstruct from cache[i].second */;
      }
    }

    // Not cached, compute
    auto result = inner.apply(expr, ctx);

    // Store in cache
    if (cache_size < CacheSize) {
      cache[cache_size++] = {expr_id, kMeta<decltype(result)>};
    }

    return result;
  }
};
```

### 2. Tracing Strategy (for debugging)

```cpp
template <typename S>
struct Trace : Strategy<Trace<S>> {
  S inner;
  const char* name;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Print entry (compile-time!)
    static_assert(/* log: entering `name` with `Expr` */);

    auto result = inner.apply(expr, ctx);

    // Print exit
    static_assert(/* log: exiting `name` with `decltype(result)` */);

    return result;
  }
};
```

### 3. Profiling Strategy

```cpp
template <typename S>
struct Profile : Strategy<Profile<S>> {
  S inner;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    auto result = inner.apply(expr, ctx);

    // Count type complexity
    constexpr SizeT input_complexity = count_expression_nodes(Expr{});
    constexpr SizeT output_complexity = count_expression_nodes(result);

    return result;
  }
};
```

### 4. Parallel Strategy (Future: C++26 constexpr threads?)

```cpp
// Hypothetical: if C++26 adds constexpr parallelism
template <typename... Strategies>
struct Parallel : Strategy<Parallel<Strategies...>> {
  std::tuple<Strategies...> strategies;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Apply all strategies in parallel, return first successful
    return apply_parallel(strategies, expr, ctx);
  }
};
```

---

## Performance Considerations

### Compile-Time Impact

**Concern:** Will strategy composition increase compilation time?

**Analysis:**

1. **Current system:** Flat if-else chain with 15+ branches

   - Each branch: template instantiation + SFINAE check
   - Depth: 20 iterations × 15 branches = 300 instantiations worst-case

2. **Strategy system:** Composed function objects
   - Each strategy: single template instantiation
   - Composition: linear in number of strategies
   - **Likely faster** due to better memoization by compiler

**Mitigation:**

- Use `extern template` for common strategy instantiations
- Provide pre-composed strategies for common cases
- Profile with `-ftime-trace` (Clang) or `-ftime-report` (GCC)

### Runtime Impact

**Concern:** Do strategies add runtime overhead?

**Answer:** **No.** All strategies are:

- Pure constexpr functions
- Inlined by compiler
- Zero-cost abstractions
- Equivalent to manual template metaprogramming

---

## Testing Strategy

### Unit Tests for Combinators

```cpp
"Identity strategy does nothing"_test = [] {
  Symbol x;
  constexpr auto result = Identity{}.apply(x + 1_c, DefaultContext{});
  static_assert(match(result, x + 1_c));
};

"Sequence composes strategies"_test = [] {
  constexpr auto strat = NormalizeSubtraction{} >> ApplyAdditionRules{};
  Symbol x;
  constexpr auto result = strat.apply(x - 1_c, DefaultContext{});
  static_assert(match(result, x + (-1_c)));  // Normalized and simplified
};

"Choice tries alternatives"_test = [] {
  constexpr auto strat = ApplyAdditionRules{} | ApplyMultiplicationRules{};
  Symbol x;

  constexpr auto result1 = strat.apply(x + 0_c, DefaultContext{});
  static_assert(match(result1, x));  // Addition rule matched

  constexpr auto result2 = strat.apply(x * 1_c, DefaultContext{});
  static_assert(match(result2, x));  // Multiplication rule matched
};
```

### Integration Tests

```cpp
"Context-aware trig simplification"_test = [] {
  Symbol x;
  constexpr auto ctx = TransformContext{}.with(symbolic_mode);

  constexpr auto expr = sin(π_c / 6_c + x);
  constexpr auto result = trig_aware_simplify.apply(expr, ctx);

  // π/6 preserved inside sin
  static_assert(match(result, sin(π_c / 6_c + x)));
};

"Staged transformation pipeline"_test = [] {
  Symbol x;
  constexpr auto expr = sin(x) * sin(x) + cos(x) * cos(x);
  constexpr auto result = staged_simplify.apply(expr, DefaultContext{});

  // Pythagorean identity applied
  static_assert(match(result, 1_c));
};
```

---

## Conclusion

This design generalizes `symbolic2` from a **monolithic simplification function** to a **composable transformation algebra**:

### Key Benefits

1. **Modularity:** Strategies can be developed, tested, and composed independently
2. **Extensibility:** Users add custom strategies without modifying core
3. **Context-awareness:** Different rules in different parts of expression tree
4. **Flexibility:** Multiple traversal strategies (top-down, bottom-up, innermost, etc.)
5. **Reusability:** Combinators apply to any transformation, not just simplification

### Next Steps

1. **Implement Phase 1** (core infrastructure) - 2 weeks
2. **Migrate existing code** (Phase 2) - 2 weeks
3. **Add context-aware features** (Phase 3) - 3 weeks
4. **Document and test** - 1 week

**Total timeline:** ~8 weeks for complete migration

### Future Directions

- **Staged metaprogramming:** Pre-compute common strategies at build time
- **User-defined operations:** Allow users to define custom expression types + rules
- **Performance optimization:** Profile and optimize hot paths
- **Integration with symbolic3:** Apply combinator algebra to value-based system

---

## Appendix: Complete Example

```cpp
// symbolic2/simplify_v2.h - New combinator-based implementation

#include "strategy.h"
#include "recursion.h"
#include "context.h"

namespace tempura {

// Define all strategies
struct ApplyAdditionRules : Strategy<ApplyAdditionRules> { /* ... */ };
struct ApplyMultiplicationRules : Strategy<ApplyMultiplicationRules> { /* ... */ };
struct ApplyTrigRules : Strategy<ApplyTrigRules> { /* ... */ };
struct FoldConstants : Strategy<FoldConstants> { /* ... */ };

// Compose algebraic simplification
constexpr auto algebraic_simplify =
  ApplyTrigRules{}
  | FoldConstants{}
  | ApplyAdditionRules{}
  | ApplyMultiplicationRules{};

// Context-aware trig simplification
constexpr auto trig_aware = ContextualStrategy{
  sin(𝐚𝐧𝐲) | cos(𝐚𝐧𝐲) | tan(𝐚𝐧𝐲),
  [](auto ctx) { return ctx.with(symbolic_mode); },
  algebraic_simplify
};

// Full simplification pipeline
constexpr auto simplify_strategy = FixPoint{Fold{trig_aware}};

// Public API (backward compatible)
template <Symbolic S>
constexpr auto simplify(S sym) {
  constexpr auto ctx = TransformContext{}.with(enable_constant_folding);
  return simplify_strategy.apply(sym, ctx);
}

// Advanced API (for power users)
template <typename Strategy, Symbolic S, typename Context>
constexpr auto simplify_with(Strategy strat, S sym, Context ctx) {
  return strat.apply(sym, ctx);
}

}  // namespace tempura
```

**Usage:**

```cpp
Symbol x;

// Simple usage (backward compatible)
constexpr auto result1 = simplify(sin(x) * sin(x) + cos(x) * cos(x));
static_assert(match(result1, 1_c));  // Pythagorean identity

// Advanced usage (custom strategy)
constexpr auto my_strategy = Unfold{ApplyAdditionRules{}} >> FoldConstants{};
constexpr auto my_ctx = TransformContext{}.with(symbolic_mode);
constexpr auto result2 = simplify_with(my_strategy, x + 1_c + 2_c, my_ctx);
static_assert(match(result2, x + 3_c));

// Context-aware usage
constexpr auto result3 = simplify(sin(π_c / 6_c + x));
// π/6 preserved inside sin argument
```

---

**End of Document**
