#pragma once

#include "meta/utility.h"
#include "symbolic3/core.h"
#include "symbolic3/strategy.h"

// Traversal strategies: control how transformations recurse through expression
// trees
//
// TRAVERSAL PATTERNS:
//
// Fold (Bottom-Up):
//        +              +
//       / \            / \
//      x   y    =>   x'  y'   =>  (x' + y')'
//   1. Transform children first
//   2. Transform parent with new children
//
// Unfold (Top-Down):
//        +              +'             *'
//       / \            / \            / \
//      x   y    =>    x   y    =>   x'  y'
//   1. Transform parent first
//   2. Transform children of result
//
// Innermost: Bottom-up traversal, apply strategy at leaves first
// Outermost: Top-down traversal, apply strategy at root first, retry if
// changed
//
// TopDown: Pre-order traversal, apply at each node going down
// BottomUp: Post-order traversal, apply at each node coming up
//
// USAGE GUIDELINES:
// - Use Fold/BottomUp for rules that need simplified children (constant
// folding)
// - Use Unfold/TopDown for rules that enable further simplification (expansion)
// - Use Innermost for exhaustive bottom-up application
// - Use Outermost for exhaustive top-down application with retry

namespace tempura::symbolic3 {

// ============================================================================
// Helper: Check if expression has children
// ============================================================================

template <typename T>
struct has_children {
  static constexpr bool value = false;
};

template <typename Op, Symbolic... Args>
  requires(sizeof...(Args) > 0)
struct has_children<Expression<Op, Args...>> {
  static constexpr bool value = true;
};

template <typename T>
constexpr bool has_children_v = has_children<T>::value;

// ============================================================================
// Helper: Apply strategy to children
// ============================================================================

template <typename S, typename Context, typename Op, Symbolic... Args>
constexpr auto apply_to_children(S strategy, Expression<Op, Args...>,
                                 Context ctx) {
  if constexpr (sizeof...(Args) == 0) {
    return Expression<Op>{};
  } else {
    // Transform each child and build new expression - no runtime tuple needed
    return Expression<Op, decltype(strategy.apply(Args{}, ctx))...>{};
  }
}

// ============================================================================
// Fold (Bottom-Up): Transform children first, then parent
// ============================================================================

template <Strategy S>
struct Fold {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if constexpr (has_children_v<Expr>) {
      // First, recursively transform children
      auto with_transformed_children =
          apply_to_children(Fold{inner_strategy}, expr, ctx);

      // Then apply strategy to the node with transformed children
      return inner_strategy.apply(with_transformed_children, ctx);
    } else {
      // Leaf node: just apply strategy
      return inner_strategy.apply(expr, ctx);
    }
  }
};

template <Strategy S>
constexpr auto fold(S const& strat) {
  return Fold<S>{.inner_strategy = strat};
}

// ============================================================================
// Unfold (Top-Down): Transform parent first, then children
// ============================================================================

template <Strategy S>
struct Unfold {
  S inner_strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // First, apply strategy to parent
    auto transformed = inner_strategy.apply(expr, ctx);

    // Then recursively transform children of the result
    if constexpr (has_children_v<decltype(transformed)>) {
      return apply_to_children(Unfold{inner_strategy}, transformed, ctx);
    } else {
      return transformed;
    }
  }
};

template <Strategy S>
constexpr auto unfold(S const& strat) {
  return Unfold<S>{.inner_strategy = strat};
}

// ============================================================================
// Innermost: Apply at leaves first, propagate upward
// ============================================================================

template <Strategy S>
struct Innermost {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if constexpr (has_children_v<Expr>) {
      // First, recursively simplify children to innermost level
      auto with_simplified_children =
          apply_to_children(Innermost{strategy}, expr, ctx);

      // Then apply strategy to this node
      return strategy.apply(with_simplified_children, ctx);
    } else {
      // Leaf node: apply strategy
      return strategy.apply(expr, ctx);
    }
  }
};

template <Strategy S>
constexpr auto innermost(S const& strat) {
  return Innermost<S>{.strategy = strat};
}

// ============================================================================
// Outermost: Apply at root first, recurse if changed
// ============================================================================

template <Strategy S>
struct Outermost {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    auto transformed = strategy.apply(expr, ctx);

    constexpr bool unchanged = std::is_same_v<decltype(transformed), Expr>;
    constexpr bool failed = std::is_same_v<decltype(transformed), Never>;

    if constexpr (unchanged || failed) {
      // No change at root, recurse into children
      if constexpr (has_children_v<Expr>) {
        return apply_to_children(Outermost{strategy}, expr, ctx);
      } else {
        return expr;
      }
    } else {
      // Changed, try again from top
      return apply(transformed, ctx);
    }
  }
};

template <Strategy S>
constexpr auto outermost(S const& strat) {
  return Outermost<S>{.strategy = strat};
}

// ============================================================================
// TopDown: Apply strategy at each node during pre-order traversal
// ============================================================================

template <Strategy S>
struct TopDown {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Apply strategy at current node
    auto transformed = strategy.apply(expr, ctx);

    // Recurse into children regardless of whether strategy changed anything
    if constexpr (has_children_v<decltype(transformed)>) {
      return apply_to_children(TopDown{strategy}, transformed, ctx);
    } else {
      return transformed;
    }
  }
};

template <Strategy S>
constexpr auto topdown(S const& strat) {
  return TopDown<S>{.strategy = strat};
}

// ============================================================================
// BottomUp: Apply strategy at each node during post-order traversal
// ============================================================================

template <Strategy S>
struct BottomUp {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // First recurse into children
    if constexpr (has_children_v<Expr>) {
      auto with_transformed_children =
          apply_to_children(BottomUp{strategy}, expr, ctx);

      // Then apply strategy at current node
      return strategy.apply(with_transformed_children, ctx);
    } else {
      // Leaf node: just apply strategy
      return strategy.apply(expr, ctx);
    }
  }
};

template <Strategy S>
constexpr auto bottomup(S const& strat) {
  return BottomUp<S>{.strategy = strat};
}

// ============================================================================
// Paramorphism: Access both original and transformed children
// ============================================================================

// Para strategy gets both original expression and result of recursion
template <Strategy S>
struct Para {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    if constexpr (has_children_v<Expr>) {
      // Recursively transform children
      auto with_transformed_children =
          apply_to_children(Para{strategy}, expr, ctx);

      // Apply strategy with both original and transformed
      // For now, simplified to just pass transformed
      // Full implementation would pass both to strategy
      return strategy.apply(with_transformed_children, ctx);
    } else {
      return strategy.apply(expr, ctx);
    }
  }
};

template <Strategy S>
constexpr auto para(S const& strat) {
  return Para<S>{.strategy = strat};
}

}  // namespace tempura::symbolic3
