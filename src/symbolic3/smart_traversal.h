#pragma once

#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/strategy.h"
#include "symbolic3/traversal.h"

// ============================================================================
// Smart Traversal Strategies - Proof of Concept
// ============================================================================
//
// This file demonstrates advanced traversal strategies that address:
// 1. Short-circuit evaluation (0 * complex_expr → 0)
// 2. Common subexpression elimination (x + x simplifies x once)
// 3. Two-phase traversal (different rules going down vs up)
// 4. Operation-specific strategy selection
//
// These are experimental and not yet integrated into main simplify pipeline.

namespace tempura::symbolic3 {

// ============================================================================
// Short-Circuit Strategy: Check parent patterns before recursing
// ============================================================================
//
// Problem: 0 * (huge_expr) shouldn't traverse into huge_expr
// Solution: Try quick patterns at parent level first (outermost-first)
//
// Usage:
//   constexpr auto quick = Rewrite{0_c * x_, 0_c} | Rewrite{x_ * 0_c, 0_c};
//   constexpr auto full = innermost(all_rules);
//   constexpr auto optimized = ShortCircuit{quick, full};

template <Strategy QuickPatterns, Strategy MainStrategy>
struct ShortCircuit {
  QuickPatterns quick;
  MainStrategy main;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // First: try quick patterns at parent level (don't recurse)
    auto quick_result = quick.apply(expr, ctx);

    constexpr bool quick_succeeded =
        !std::is_same_v<decltype(quick_result), Never>;

    if constexpr (quick_succeeded) {
      // Short-circuit successful, return immediately
      return quick_result;
    } else {
      // No short-circuit, proceed with main strategy (will recurse)
      return main.apply(expr, ctx);
    }
  }
};

template <Strategy Q, Strategy M>
constexpr auto short_circuit(Q const& quick, M const& main) {
  return ShortCircuit<Q, M>{.quick = quick, .main = main};
}

// ============================================================================
// Common Subexpression Detection (Compile-Time)
// ============================================================================
//
// Problem: (complex_expr) + (complex_expr) simplifies both sides independently
// Solution: Check for structural equality, simplify once if identical
//
// Note: This is compile-time only (checks if types are identical).
// For runtime CSE, would need hash-consing in context.

template <Strategy S>
struct CSEAwareStrategy {
  S strategy;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Check if expression has two identical children
    if constexpr (requires { detect_binary_with_identical_children(expr); }) {
      return simplify_with_cse(expr, ctx);
    } else {
      // Normal case: apply strategy as usual
      return strategy.apply(expr, ctx);
    }
  }

 private:
  // Detect binary operations with structurally identical children
  template <typename Op, Symbolic A, Symbolic B>
  static constexpr auto detect_binary_with_identical_children(
      Expression<Op, A, B>) -> std::enable_if_t<std::is_same_v<A, B>, bool> {
    return true;
  }

  // Simplify binary operation with identical children
  template <typename Op, Symbolic A, typename Context>
  constexpr auto simplify_with_cse(Expression<Op, A, A> expr,
                                   Context ctx) const {
    // Simplify child only once
    auto child_simplified = strategy.apply(A{}, ctx);

    // Apply like-term rules based on operation
    if constexpr (std::is_same_v<Op, AddOp>) {
      // x + x → 2*x (don't need to simplify x twice)
      return Constant<2>{} * child_simplified;
    } else if constexpr (std::is_same_v<Op, MulOp>) {
      // x * x → x^2 (don't need to simplify x twice)
      return pow(child_simplified, Constant<2>{});
    } else {
      // Other operations: reconstruct with simplified child
      return Expression<Op, decltype(child_simplified),
                        decltype(child_simplified)>{};
    }
  }
};

template <Strategy S>
constexpr auto with_cse(S const& strat) {
  return CSEAwareStrategy<S>{.strategy = strat};
}

// ============================================================================
// Two-Phase Traversal: Different rules going down vs coming up
// ============================================================================
//
// Problem: Some rules need to apply before children are simplified,
//          others need to apply after.
//
// Solution: Maintain two separate rule sets:
//   - descent_rules: Applied during pre-order traversal (parent first)
//   - ascent_rules:  Applied during post-order traversal (children first)
//
// Example:
//   descent_rules = annihilators | identities | distribution
//   ascent_rules  = factoring | collection | constant_folding
//
// Why?
//   - Annihilators: 0 * (a+b) → 0  (check before distributing)
//   - Distribution: (a+b) * c → a*c + b*c  (expand before simplifying children)
//   - Factoring: x*a + x*b → x*(a+b)  (collect after children simplified)
//
// This is the most elegant approach but requires careful rule categorization.

template <Strategy DescentRules, Strategy AscentRules>
struct TwoPhase {
  DescentRules descent;
  AscentRules ascent;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Phase 1: Apply descent rules (pre-order, parent before children)
    auto after_descent = descent.apply(expr, ctx);

    // If descent rules failed, use original expression
    constexpr bool descent_changed =
        !std::is_same_v<decltype(after_descent), Never>;
    auto current = [&] {
      if constexpr (descent_changed) {
        return after_descent;
      } else {
        return expr;
      }
    }();

    // Phase 2: Recurse into children
    auto after_children = [&] {
      if constexpr (has_children_v<decltype(current)>) {
        return apply_to_children(TwoPhase{descent, ascent}, current, ctx);
      } else {
        return current;
      }
    }();

    // Phase 3: Apply ascent rules (post-order, after children)
    auto after_ascent = ascent.apply(after_children, ctx);

    // Return result (prefer ascent result if it changed something)
    constexpr bool ascent_changed =
        !std::is_same_v<decltype(after_ascent), Never>;
    if constexpr (ascent_changed) {
      return after_ascent;
    } else {
      return after_children;
    }
  }
};

template <Strategy D, Strategy A>
constexpr auto two_phase(D const& descent, A const& ascent) {
  return TwoPhase<D, A>{.descent = descent, .ascent = ascent};
}

// ============================================================================
// Operation-Specific Strategy Selection
// ============================================================================
//
// Problem: Different operations benefit from different traversal strategies
//   - Multiplication: outermost (check for 0 * x before recursing)
//   - Addition: innermost (collect like terms after simplifying)
//   - Power: topdown (expand before recursing into exponent)
//
// Solution: Dispatch based on operation type
//
// Note: This is a compile-time dispatch, no runtime overhead.

template <Strategy Rules>
struct SmartDispatch {
  Rules rules;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Dispatch based on expression structure
    if constexpr (is_multiplication_expr<Expr>) {
      // Multiplication: check annihilators first (outermost)
      return outermost(rules).apply(expr, ctx);
    } else if constexpr (is_power_expr<Expr>) {
      // Power: expand patterns top-down
      return topdown(rules).apply(expr, ctx);
    } else {
      // Addition and others: simplify children first (innermost)
      // Note: using innermost for both addition and default case
      return innermost(rules).apply(expr, ctx);
    }
  }

 private:
  // Type traits to detect expression kinds
  template <typename T>
  struct is_mult_expr : std::false_type {};

  template <Symbolic... Args>
  struct is_mult_expr<Expression<MulOp, Args...>> : std::true_type {};

  template <typename T>
  static constexpr bool is_multiplication_expr = is_mult_expr<T>::value;

  template <typename T>
  struct is_add_expr : std::false_type {};

  template <Symbolic... Args>
  struct is_add_expr<Expression<AddOp, Args...>> : std::true_type {};

  template <typename T>
  static constexpr bool is_addition_expr = is_add_expr<T>::value;

  template <typename T>
  struct is_pow_expr : std::false_type {};

  template <Symbolic... Args>
  struct is_pow_expr<Expression<PowOp, Args...>> : std::true_type {};

  template <typename T>
  static constexpr bool is_power_expr = is_pow_expr<T>::value;
};

template <Strategy S>
constexpr auto smart_dispatch(S const& rules) {
  return SmartDispatch<S>{.rules = rules};
}

// ============================================================================
// Lazy Evaluation Strategy: Only evaluate arguments if needed
// ============================================================================
//
// Problem: exp(log(complex_expr)) should cancel without simplifying argument
// Solution: Check for identity patterns BEFORE recursing
//
// This is essentially an outermost strategy with identity-specific rules.

template <Strategy IdentityRules, Strategy MainRules>
struct LazyEval {
  IdentityRules identities;
  MainRules main;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Try identity patterns first (don't recurse into arguments)
    auto identity_result = identities.apply(expr, ctx);

    constexpr bool is_identity =
        !std::is_same_v<decltype(identity_result), Never>;

    if constexpr (is_identity) {
      // Identity matched, return without evaluating children
      // Note: The identity rule itself extracts the argument
      return identity_result;
    } else {
      // Not an identity, proceed with normal evaluation
      return innermost(main).apply(expr, ctx);
    }
  }
};

template <Strategy I, Strategy M>
constexpr auto lazy_eval(I const& identities, M const& main) {
  return LazyEval<I, M>{.identities = identities, .main = main};
}

// ============================================================================
// Combined Smart Strategy (Composition of All Techniques)
// ============================================================================
//
// This combines all the above techniques into a single strategy:
// 1. Short-circuit checks (annihilators, identities)
// 2. CSE detection
// 3. Two-phase traversal (descent vs ascent rules)
// 4. Operation-specific dispatch
//
// Usage:
//   constexpr auto optimized = smart_simplify(
//     quick_patterns,      // Annihilators, identities
//     expansion_rules,     // Rules to apply going down
//     collection_rules,    // Rules to apply coming up
//     main_rules          // All other rules
//   );

template <Strategy Quick, Strategy Descent, Strategy Ascent, Strategy Main>
struct SmartSimplify {
  Quick quick;
  Descent descent;
  Ascent ascent;
  Main main;

  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Step 1: Try quick patterns (short-circuit)
    auto quick_result = quick.apply(expr, ctx);
    if constexpr (!std::is_same_v<decltype(quick_result), Never>) {
      return quick_result;
    }

    // Step 2: Check for CSE opportunities
    if constexpr (has_identical_children(expr)) {
      return apply_with_cse(expr, ctx);
    }

    // Step 3: Two-phase traversal with operation-specific dispatch
    auto two_phase_strat = two_phase(descent, ascent);
    return smart_dispatch(two_phase_strat).apply(expr, ctx);
  }

 private:
  // Helper to detect identical children (compile-time CSE)
  template <typename T>
  static constexpr bool has_identical_children(T) {
    return false;
  }

  template <typename Op, Symbolic A>
  static constexpr bool has_identical_children(Expression<Op, A, A>) {
    return true;
  }

  // Apply with CSE optimization
  template <typename Op, Symbolic A, typename Context>
  constexpr auto apply_with_cse(Expression<Op, A, A> expr, Context ctx) const {
    // Simplify child once
    auto child = two_phase(descent, ascent).apply(A{}, ctx);

    // Apply operation-specific like-term rule
    if constexpr (std::is_same_v<Op, AddOp>) {
      return Constant<2>{} * child;
    } else if constexpr (std::is_same_v<Op, MulOp>) {
      return pow(child, Constant<2>{});
    } else {
      return Expression<Op, decltype(child), decltype(child)>{};
    }
  }
};

// ============================================================================
// Usage Examples
// ============================================================================

// Example 1: Simple short-circuit
// constexpr auto quick_rules = Rewrite{0_c * x_, 0_c} | Rewrite{x_ * 0_c, 0_c};
// constexpr auto optimized = short_circuit(quick_rules, innermost(all_rules));

// Example 2: Two-phase simplification
// constexpr auto descent = annihilators | identities | distribution;
// constexpr auto ascent = factoring | collection | constant_fold;
// constexpr auto two_phase_simplify = two_phase(descent, ascent);

// Example 3: Lazy evaluation of identities
// constexpr auto identities = Rewrite{exp(log(x_)), x_} | Rewrite{log(exp(x_)),
// x_}; constexpr auto lazy = lazy_eval(identities, all_rules);

// Example 4: Full smart strategy
// constexpr auto smart = SmartSimplify{
//   .quick = quick_patterns,
//   .descent = expansion_rules,
//   .ascent = collection_rules,
//   .main = all_rules
// };

}  // namespace tempura::symbolic3
