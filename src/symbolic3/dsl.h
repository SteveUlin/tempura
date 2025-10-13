#pragma once

#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/strategy.h"
#include "symbolic3/traversal.h"

// ============================================================================
// Smart Dispatch DSL - Elegant Operators for Strategy Composition
// ============================================================================
//
// This file implements a domain-specific language for composing simplification
// strategies with elegant, readable syntax.
//
// Core operators:
//   ~>  (flow)    Two-phase composition: descent ~> ascent
//   ?>  (try)     Short-circuit: quick ?> fallback
//   @   (at)      Traversal mode: rules @ innermost
//   *   (star)    Fixpoint: rules* or rules*10
//
// Example:
//   auto simplify =
//     quick_patterns ?>
//     (expansion @ topdown ~> collection @ bottomup)*;

namespace tempura::symbolic3 {

// ============================================================================
// Forward Declarations
// ============================================================================

// Traversal mode tags
struct InnerMostTag {};
struct OuterMostTag {};
struct TopDownTag {};
struct BottomUpTag {};

constexpr inline InnerMostTag innermost_mode{};
constexpr inline OuterMostTag outermost_mode{};
constexpr inline TopDownTag topdown_mode{};
constexpr inline BottomUpTag bottomup_mode{};

// ============================================================================
// Operator ~> (Flow): Two-Phase Composition
// ============================================================================
//
// Usage: descent_rules ~> ascent_rules
//
// Creates a two-phase traversal strategy where:
//   1. Descent rules applied going down (pre-order)
//   2. Recurse into children
//   3. Ascent rules applied coming up (post-order)
//
// This naturally separates conflicting rules (e.g., distribution vs factoring)

template <Strategy Descent, Strategy Ascent>
struct TwoPhaseComposition {
  Descent descent;
  Ascent ascent;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Phase 1: Apply descent rules (pre-order)
    auto after_descent = descent.apply(expr, ctx);

    // Determine current expression (prefer descent result if it changed)
    auto current = [&] {
      if constexpr (!std::is_same_v<decltype(after_descent), Never>) {
        return after_descent;
      } else {
        return expr;
      }
    }();

    // Phase 2: Recurse into children
    auto after_children = [&] {
      if constexpr (has_children_v<decltype(current)>) {
        return apply_to_children(*this, current, ctx);
      } else {
        return current;
      }
    }();

    // Phase 3: Apply ascent rules (post-order)
    auto after_ascent = ascent.apply(after_children, ctx);

    // Return result (prefer ascent if it changed something)
    if constexpr (!std::is_same_v<decltype(after_ascent), Never>) {
      return after_ascent;
    } else {
      return after_children;
    }
  }
};

// Helper wrapper to enable ~> operator using two steps
// We can't overload ~> directly, so we use a wrapper type approach
template <Strategy S>
struct DescentPhase {
  S strategy;
};

// Create descent phase: descent(rules)
template <Strategy S>
constexpr auto descent(S strategy) {
  return DescentPhase<S>{.strategy = strategy};
}

// Flow operator: descent(rules) >> ascent_rules
template <Strategy D, Strategy A>
constexpr auto operator>>(DescentPhase<D> d, A ascent) {
  return TwoPhaseComposition<D, A>{.descent = d.strategy, .ascent = ascent};
}

// Alternative: Named function for direct use (cleaner for now)
// Usage: flow(descent_rules, ascent_rules)
template <Strategy D, Strategy A>
constexpr auto flow(D descent_rules, A ascent_rules) {
  return TwoPhaseComposition<D, A>{.descent = descent_rules,
                                   .ascent = ascent_rules};
}

// ============================================================================
// Operator ?> (Try): Short-Circuit Strategy
// ============================================================================
//
// Usage: quick_patterns ?> full_pipeline
//
// Tries quick patterns first. If they match, returns immediately.
// Otherwise falls back to full pipeline.
//
// This enables optimizations like: 0 * x → 0 without recursing into x

template <Strategy Quick, Strategy Fallback>
struct ShortCircuitStrategy {
  Quick quick;
  Fallback fallback;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Try quick patterns first
    auto quick_result = quick.apply(expr, ctx);

    // If quick pattern matched, return immediately
    if constexpr (!std::is_same_v<decltype(quick_result), Never>) {
      return quick_result;
    } else {
      // Quick pattern didn't match, try fallback
      return fallback.apply(expr, ctx);
    }
  }
};

// Helper wrapper for enabling ?> operator
template <Strategy S>
struct QuickCheck {
  S strategy;
};

template <Strategy S>
constexpr auto quick(S strategy) {
  return QuickCheck<S>{.strategy = strategy};
}

template <Strategy Q, Strategy F>
constexpr auto operator>(QuickCheck<Q> q, F fallback) {
  return ShortCircuitStrategy<Q, F>{.quick = q.strategy, .fallback = fallback};
}

// Function version for direct use
template <Strategy Q, Strategy F>
constexpr auto try_first(Q quick, F fallback) {
  return ShortCircuitStrategy<Q, F>{.quick = quick, .fallback = fallback};
}

// ============================================================================
// Operator @ (At): Traversal Mode Selection
// ============================================================================
//
// Usage: rules @ innermost_mode
//        rules @ outermost_mode
//        rules @ topdown_mode
//        rules @ bottomup_mode
//
// Applies rules with specified traversal strategy

template <Strategy S>
struct StrategyWrapper {
  S strategy;

  // Innermost traversal
  constexpr auto operator%(InnerMostTag) const { return innermost(strategy); }

  // Outermost traversal
  constexpr auto operator%(OuterMostTag) const { return outermost(strategy); }

  // Top-down traversal
  constexpr auto operator%(TopDownTag) const { return topdown(strategy); }

  // Bottom-up traversal
  constexpr auto operator%(BottomUpTag) const { return bottomup(strategy); }
};

// Enable: rules @ mode
template <Strategy S>
constexpr auto at(S strategy) {
  return StrategyWrapper<S>{.strategy = strategy};
}

// Note: Can't easily overload operator@ as it's unary address-of
// Alternative: Use operator% which is uncommon in this context
// Or provide named functions

// Named function versions (cleaner for now)
template <Strategy S>
constexpr auto at_innermost(S strategy) {
  return innermost(strategy);
}

template <Strategy S>
constexpr auto at_outermost(S strategy) {
  return outermost(strategy);
}

template <Strategy S>
constexpr auto at_topdown(S strategy) {
  return topdown(strategy);
}

template <Strategy S>
constexpr auto at_bottomup(S strategy) {
  return bottomup(strategy);
}

// ============================================================================
// Fixpoint with Iteration Limit
// ============================================================================
//
// Extends existing FixPoint to support iteration limits
//
// Usage: FixPoint<10>{strategy} - max 10 iterations

template <Strategy S, int MaxIterations = 100>
struct BoundedFixPoint {
  S strategy;

  template <Symbolic Expr, typename Context, int Iteration = 0>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Check iteration limit
    if constexpr (Iteration >= MaxIterations) {
      // Hit limit, return current result
      return expr;
    } else {
      // Try applying strategy
      auto result = strategy.apply(expr, ctx);

      // Check if anything changed
      constexpr bool unchanged = std::is_same_v<decltype(result), Expr>;
      constexpr bool failed = std::is_same_v<decltype(result), Never>;

      if constexpr (unchanged || failed) {
        // No change or failed, done
        return expr;
      } else {
        // Changed, try again with incremented counter
        return apply<decltype(result), Context, Iteration + 1>(result, ctx);
      }
    }
  }
};

// Helper for creating bounded fixpoint
template <int MaxIter = 100, Strategy S>
constexpr auto fixpoint(S strategy) {
  return BoundedFixPoint<S, MaxIter>{.strategy = strategy};
}

// ============================================================================
// Depth-Limited Traversal
// ============================================================================
//
// Stops recursing after reaching specified depth
//
// Usage: with_depth_limit<5>(strategy)

template <Strategy S, int MaxDepth = 20>
struct DepthLimited {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx, int depth = 0) const {
    if (depth >= MaxDepth) {
      // Hit depth limit, return unchanged
      return expr;
    }

    // Apply strategy at current level
    auto result = strategy.apply(expr, ctx);

    // Recurse with incremented depth
    if constexpr (has_children_v<decltype(result)>) {
      return apply_to_children_with_depth(result, ctx, depth + 1);
    } else {
      return result;
    }
  }

 private:
  template <typename Expr2, typename Context2>
  constexpr auto apply_to_children_with_depth(Expr2 expr, Context2 ctx,
                                              int depth) const {
    if constexpr (has_children_v<Expr2>) {
      return apply_to_children(
          [this, depth](auto child, auto ctx2) {
            return this->apply(child, ctx2, depth);
          },
          expr, ctx);
    } else {
      return expr;
    }
  }
};

template <int MaxDepth, Strategy S>
constexpr auto with_depth_limit(S strategy) {
  return DepthLimited<S, MaxDepth>{.strategy = strategy};
}

// ============================================================================
// Smart Dispatch: Operation-Specific Strategy Selection
// ============================================================================
//
// Automatically selects traversal strategy based on operation type
//
// Usage: smart_dispatch(rules)

template <Strategy S>
struct SmartDispatch {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Dispatch based on expression type
    if constexpr (is_mult_expr_v<Expr>) {
      // Multiplication: check annihilators first (outermost)
      return at_outermost(strategy).apply(expr, ctx);
    } else if constexpr (is_pow_expr_v<Expr>) {
      // Power: expand patterns top-down
      return at_topdown(strategy).apply(expr, ctx);
    } else {
      // Addition and others: bottom-up to collect terms
      return at_innermost(strategy).apply(expr, ctx);
    }
  }

 private:
  // Type traits
  template <typename T>
  struct is_mult_expr : std::false_type {};

  template <Symbolic... Args>
  struct is_mult_expr<Expression<MulOp, Args...>> : std::true_type {};

  template <typename T>
  static constexpr bool is_mult_expr_v = is_mult_expr<T>::value;

  template <typename T>
  struct is_pow_expr : std::false_type {};

  template <Symbolic... Args>
  struct is_pow_expr<Expression<PowOp, Args...>> : std::true_type {};

  template <typename T>
  static constexpr bool is_pow_expr_v = is_pow_expr<T>::value;
};

template <Strategy S>
constexpr auto smart_dispatch(S strategy) {
  return SmartDispatch<S>{.strategy = strategy};
}

// ============================================================================
// Convenience: try_strategy wrapper
// ============================================================================
//
// Wraps a strategy to return original expression instead of Never on failure
// Useful for traversal strategies that need to handle all expressions

template <Strategy S>
struct TryStrategy {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    auto result = strategy.apply(expr, ctx);
    if constexpr (std::is_same_v<decltype(result), Never>) {
      return expr;  // Return original if strategy failed
    } else {
      return result;
    }
  }
};

template <Strategy S>
constexpr auto try_strategy(S strategy) {
  return TryStrategy<S>{.strategy = strategy};
}

}  // namespace tempura::symbolic3
