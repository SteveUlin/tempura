// Prototype: Combinator-based symbolic transformation system
// Demonstrates context-aware strategies and composable data flows

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace tempura::prototype {

// ============================================================================
// Core Types (simplified for prototype)
// ============================================================================

struct SymbolicTag {};

template <typename T>
concept Symbolic = std::derived_from<T, SymbolicTag>;

template <typename unique = decltype([] {})>
struct Symbol : SymbolicTag {
  static constexpr int id = 0;  // Simplified for prototype
};

template <auto val>
struct Constant : SymbolicTag {
  static constexpr auto value = val;
};

struct AddOp {};
struct MulOp {};
struct SinOp {};
struct CosOp {};

template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {};

// Pattern matching support
template <Symbolic S1, Symbolic S2>
constexpr bool match(S1, S2) {
  return std::is_same_v<S1, S2>;
}

// ============================================================================
// Context System
// ============================================================================

// Context tags for marking traversal state
struct InsideTrigTag {};
struct InsideLogTag {};
struct ConstantFoldingEnabledTag {};
struct SymbolicModeTag {};

// Context tag storage
template <typename... Tags>
struct ContextTags {
  static constexpr std::size_t size = sizeof...(Tags);

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
    // Simplified for prototype: just return empty context
    return ContextTags<>{};
  }
};

// Transformation context with depth tracking and tags
template <std::size_t Depth = 0, typename Tags = ContextTags<>>
struct TransformContext {
  static constexpr std::size_t depth = Depth;
  using tags = Tags;

  template <std::size_t N>
  constexpr auto increment_depth() const {
    return TransformContext<Depth + N, Tags>{};
  }

  template <typename Tag>
  static constexpr bool has() {
    return Tags::template has<Tag>();
  }

  template <typename Tag>
  constexpr auto with(Tag) const {
    return TransformContext<Depth, decltype(Tags{}.template add<Tag>())>{};
  }

  template <typename Tag>
  constexpr auto without(Tag) const {
    return TransformContext<Depth, decltype(Tags{}.template remove<Tag>())>{};
  }
};

// ============================================================================
// Strategy Base
// ============================================================================

template <typename Impl>
struct Strategy {
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context ctx) const {
    return static_cast<const Impl*>(this)->template apply_impl<S, Context>(expr,
                                                                           ctx);
  }
};

// ============================================================================
// Basic Combinators
// ============================================================================

// Identity: returns expression unchanged
struct Identity : Strategy<Identity> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context) const {
    return expr;
  }
};

// Sequence: apply first strategy, then second
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

// Operator overload for convenient syntax
template <typename S1, typename S2>
constexpr auto operator>>(Strategy<S1> const& s1, Strategy<S2> const& s2) {
  return Sequence<S1, S2>{.first = static_cast<const S1&>(s1),
                          .second = static_cast<const S2&>(s2)};
}

// Choice: try first strategy, if no change, try second
template <typename S1, typename S2>
struct Choice : Strategy<Choice<S1, S2>> {
  S1 first;
  S2 second;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    auto result = first.apply(expr, ctx);
    if constexpr (match(result, expr)) {
      return second.apply(expr, ctx);
    } else {
      return result;
    }
  }
};

template <typename S1, typename S2>
constexpr auto operator|(Strategy<S1> const& s1, Strategy<S2> const& s2) {
  return Choice<S1, S2>{.first = static_cast<const S1&>(s1),
                        .second = static_cast<const S2&>(s2)};
}

// Conditional: apply strategy only if predicate holds
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

// ============================================================================
// Recursion Combinators
// ============================================================================

// FixPoint: apply strategy repeatedly until no change or depth limit
template <typename S, std::size_t MaxDepth = 20>
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

// Fold: bottom-up traversal (transform children first, then parent)
template <typename S>
struct Fold : Strategy<Fold<S>> {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // For this prototype, we'll just apply the strategy
    // In full implementation, would recurse into children first
    return inner_strategy.apply(expr, ctx);
  }
};

// Unfold: top-down traversal (transform parent first, then children)
template <typename S>
struct Unfold : Strategy<Unfold<S>> {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Transform parent first
    auto transformed = inner_strategy.apply(expr, ctx);
    // In full implementation, would then recurse into children
    return transformed;
  }
};

// Innermost: apply at leaves first, propagate upward
template <typename S>
struct Innermost : Strategy<Innermost<S>> {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    // Simplified: just apply fold then strategy
    auto with_simplified_children = Fold{strategy}.apply(expr, ctx);
    return strategy.apply(with_simplified_children, ctx);
  }
};

// Outermost: apply at root first, recurse if changed
template <typename S>
struct Outermost : Strategy<Outermost<S>> {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply_impl(Expr expr, Context ctx) const {
    auto transformed = strategy.apply(expr, ctx);
    if constexpr (!match(transformed, expr)) {
      // Changed, try again from top
      return apply_impl(transformed, ctx);
    } else {
      // No change, recurse into children
      return Fold{strategy}.apply(expr, ctx);
    }
  }
};

// ============================================================================
// Context-Aware Strategies
// ============================================================================

// Modify context when entering specific expression patterns
template <typename Pattern, typename ContextModifier, typename InnerStrategy>
struct ContextualStrategy
    : Strategy<ContextualStrategy<Pattern, ContextModifier, InnerStrategy>> {
  Pattern pattern;
  ContextModifier modifier;
  InnerStrategy inner;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Check if expression matches pattern
    // In full implementation, would use proper pattern matching
    if constexpr (std::is_same_v<S, Pattern>) {
      auto new_ctx = modifier(ctx);
      return inner.apply(expr, new_ctx);
    } else {
      return inner.apply(expr, ctx);
    }
  }
};

// ============================================================================
// Example Strategies
// ============================================================================

// Strategy: fold constant addition
struct FoldConstantAddition : Strategy<FoldConstantAddition> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Only fold if constant folding is enabled in context
    if constexpr (ctx.template has<ConstantFoldingEnabledTag>()) {
      // Check if expression is addition of two constants
      if constexpr (requires {
                      typename S::template is_expression<AddOp, Constant<1>,
                                                         Constant<2>>;
                    }) {
        // In full implementation: extract values and compute sum
        return expr;  // Simplified for prototype
      }
    }
    return expr;
  }
};

// Strategy: simplify trig identities
struct SimplifyTrigIdentities : Strategy<SimplifyTrigIdentities> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context) const {
    // Example: sin²(x) + cos²(x) → 1
    // In full implementation: use pattern matching and rewrite rules
    return expr;
  }
};

// Strategy: normalize subtraction to addition
struct NormalizeSubtraction : Strategy<NormalizeSubtraction> {
  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context) const {
    // a - b → a + (-b)
    // In full implementation: check for SubOp and transform
    return expr;
  }
};

// ============================================================================
// Contextual Strategy: Trig Functions
// ============================================================================

// When entering a trig function, disable constant folding
template <typename InnerStrategy>
struct TrigAwareStrategy : Strategy<TrigAwareStrategy<InnerStrategy>> {
  InnerStrategy inner;

  template <Symbolic S, typename Context>
  constexpr auto apply_impl(S expr, Context ctx) const {
    // Check if expression is sin, cos, or tan
    if constexpr (
        requires { typename S::template is_expression<SinOp>; } ||
        requires { typename S::template is_expression<CosOp>; }) {
      // Entering trig function: disable constant folding, mark inside trig
      auto trig_ctx =
          ctx.without(ConstantFoldingEnabledTag{}).with(InsideTrigTag{});

      // Apply inner strategy with modified context
      return inner.apply(expr, trig_ctx);
    } else {
      return inner.apply(expr, ctx);
    }
  }
};

// ============================================================================
// Example Composition
// ============================================================================

// Build a complete simplification strategy
inline constexpr auto algebraic_simplify =
    FoldConstantAddition{} | NormalizeSubtraction{} | SimplifyTrigIdentities{};

// Add trig-aware context modification
inline constexpr auto trig_aware_simplify =
    TrigAwareStrategy<decltype(algebraic_simplify)>{.inner =
                                                        algebraic_simplify};

// Full simplification pipeline with fixpoint
using TrigAwareType = decltype(trig_aware_simplify);
using FoldType = Fold<TrigAwareType>;
inline constexpr auto full_simplify = FixPoint<FoldType, 20>{
    .strategy = Fold<TrigAwareType>{.inner_strategy = trig_aware_simplify}};

// ============================================================================
// Tests
// ============================================================================

// Test context tag system
static_assert(ContextTags<>{}.size == 0);
static_assert(ContextTags<InsideTrigTag>{}.has<InsideTrigTag>());
static_assert(!ContextTags<InsideTrigTag>{}.has<InsideLogTag>());

// Test context modification
constexpr auto ctx1 = TransformContext<>{};
static_assert(ctx1.depth == 0);
static_assert(!ctx1.has<InsideTrigTag>());

constexpr auto ctx2 = ctx1.with(InsideTrigTag{});
static_assert(ctx2.has<InsideTrigTag>());

constexpr auto ctx3 = ctx2.without(InsideTrigTag{});
static_assert(!ctx3.has<InsideTrigTag>());

constexpr auto ctx4 = ctx1.increment_depth<1>();
static_assert(ctx4.depth == 1);

// Test strategy composition
constexpr Identity id1{};
constexpr Identity id2{};
constexpr auto composed = id1 >> id2;
// Type check: composition creates Sequence
using ComposedType = decltype(composed);

constexpr auto choice = id1 | id2;
// Type check: choice creates Choice
using ChoiceType = decltype(choice);

// Test strategies with expressions
constexpr Symbol<> x;
constexpr Constant<1> one;

constexpr auto result1 = Identity{}.apply(x, TransformContext<>{});
static_assert(match(result1, x));

// Test fixpoint combinator
constexpr auto fixpoint = FixPoint<Identity, 5>{.strategy = Identity{}};
constexpr auto result2 = fixpoint.apply(one, TransformContext<>{});
static_assert(match(result2, one));

// Test context propagation with depth
constexpr auto ctx_depth5 = TransformContext<5>{};
constexpr auto identity_strat = Identity{};
// In a real fixpoint that checks depth, this would terminate immediately
constexpr auto result3 = identity_strat.apply(x, ctx_depth5);
static_assert(match(result3, x));

}  // namespace tempura::prototype

// ============================================================================
// Example Usage Documentation
// ============================================================================

/*

EXAMPLE 1: Context-Aware Simplification
========================================

Problem: sin(π/6) should evaluate to 1/2, but sin(π/6 + x) should preserve π/6.

Solution:
```cpp
using namespace tempura::prototype;

constexpr auto ctx_with_folding = TransformContext{}
  .with(ConstantFoldingEnabledTag{});

// Outside trig: fold constants aggressively
auto result1 = algebraic_simplify.apply(expr1, ctx_with_folding);

// Inside trig: context modified to disable folding
auto result2 = trig_aware_simplify.apply(sin_expr, ctx_with_folding);
```

EXAMPLE 2: Staged Transformation Pipeline
==========================================

Problem: Different transformations need to happen in sequence.

Solution:
```cpp
constexpr auto staged =
  NormalizeSubtraction{}          // Stage 1: syntax normalization
  >> FoldConstantAddition{}       // Stage 2: constant folding
  >> SimplifyTrigIdentities{};    // Stage 3: domain-specific rules

auto result = staged.apply(expr, ctx);
```

EXAMPLE 3: Conditional Strategies
==================================

Problem: Apply rules only when certain conditions hold.

Solution:
```cpp
constexpr auto conditional = when(
  [](auto expr, auto ctx) {
    return ctx.template has<SymbolicModeTag>();
  },
  SimplifyTrigIdentities{}
);

auto result = conditional.apply(expr, ctx.with(SymbolicModeTag{}));
```

EXAMPLE 4: Custom Traversal Strategies
=======================================

Problem: Need different traversal orders for different algorithms.

Solution:
```cpp
// Bottom-up (simplify children first)
constexpr auto bottom_up = Fold{algebraic_simplify};

// Top-down (simplify parent first)
constexpr auto top_down = Unfold{algebraic_simplify};

// Innermost-first (for CSE)
constexpr auto innermost = Innermost{algebraic_simplify};

// Outermost-first (for expansion)
constexpr auto outermost = Outermost{algebraic_simplify};
```

EXAMPLE 5: Multiple Contexts
=============================

Problem: Track multiple context properties simultaneously.

Solution:
```cpp
constexpr auto complex_ctx = TransformContext{}
  .with(InsideTrigTag{})
  .with(SymbolicModeTag{})
  .increment_depth<3>();

static_assert(complex_ctx.has<InsideTrigTag>());
static_assert(complex_ctx.has<SymbolicModeTag>());
static_assert(complex_ctx.depth == 3);
```

*/

int main() {
  using namespace tempura::prototype;

  // Demonstrate that all strategies are constexpr-compatible
  constexpr Symbol<> x;

  constexpr auto ctx = TransformContext{}.with(ConstantFoldingEnabledTag{});

  constexpr Identity id1{};
  constexpr Identity id2{};
  constexpr auto strategy = id1 >> id2;
  constexpr auto result = strategy.apply(x, ctx);

  static_assert(match(result, x),
                "Identity strategy should preserve expression");

  return 0;
}
