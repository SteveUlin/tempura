#pragma once

#include "meta/utility.h"
#include "symbolic3/core.h"
#include "symbolic3/strategy.h"

// Traversal strategies: control how transformations recurse through expression
// trees
//
// ============================================================================
// TRAVERSAL PATTERNS
// ============================================================================
//
// Fold (Bottom-Up): Transform leaves first, then propagate upward
//        +              +              result
//       / \            / \                |
//      x   y    =>   x'  y'   =>     (x' + y')'
//
//   Order: 1) Transform x → x'
//          2) Transform y → y'
//          3) Transform (x' + y') with new children
//
//   Use case: Constant folding - needs evaluated children before folding parent
//   Example: (2+3)*5 → 5*5 → 25  [children must be folded before parent]
//
// ============================================================================
//
// Unfold (Top-Down): Transform root first, then recurse into result
//        +              +'             result
//       / \            / \                |
//      x   y    =>    a   b    =>   transform(a, b)
//
//   Order: 1) Transform (x + y) → (a + b)   [parent changes structure]
//          2) Transform a (new child)
//          3) Transform b (new child)
//
//   Use case: Expansion rules that expose new simplification opportunities
//   Example: exp(a+b) → exp(a)*exp(b)  [creates new structure to simplify]
//
// ============================================================================
//
// Innermost: Bottom-up with fixpoint - exhaustively simplify leaves first
//   Applies strategy repeatedly at each node until stable before moving up
//   Guarantees no further simplification possible at lower levels
//
// Outermost: Top-down with fixpoint - simplify root until stable, then recurse
//   Applies strategy repeatedly at current node, retries if changed
//   Can expose new simplification opportunities by transforming parent first
//
// TopDown: Pre-order traversal - visit each node going down (once)
//   Apply strategy at node, then recurse into children (no retry at node)
//
// BottomUp: Post-order traversal - visit each node coming up (once)
//   Recurse into children first, then apply strategy at node (no retry)
//
// ============================================================================
// USAGE GUIDELINES
// ============================================================================
//
// Choose based on rule dependencies:
//
// Fold/BottomUp:
//   - Rules need simplified children (constant folding: 2+3 → 5)
//   - Term collection (x+x → 2*x after simplifying x)
//   - Most algebraic simplification
//
// Unfold/TopDown:
//   - Rules that expose new structure (exp(a+b) → exp(a)*exp(b))
//   - Distribution (x*(a+b) → x*a + x*b creates addition to simplify)
//   - Expansion rules that enable further simplification
//
// Innermost:
//   - Exhaustive bottom-up until no more changes possible
//   - Safe default for algebraic simplification
//
// Outermost:
//   - When parent transformation is critical (some expansion strategies)
//   - Use with caution: can be less efficient than Innermost

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
