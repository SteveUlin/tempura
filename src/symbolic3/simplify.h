#pragma once

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/operators.h"
#include "symbolic3/ordering.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/strategy.h"
#include "symbolic3/term_structure.h"
#include "symbolic3/traversal.h"

// ============================================================================
// TERM REWRITING THEORY & SIMPLIFICATION DESIGN
// ============================================================================
//
// This file implements algebraic simplification using **term rewriting
// systems**, a formal framework from theoretical computer science for
// transforming expressions.
//
// CORE CONCEPTS:
// --------------
// A term rewriting system consists of:
//   1. A set of rewrite rules: pattern → replacement
//   2. A strategy for applying rules (innermost, outermost, etc.)
//   3. Termination guarantees to avoid infinite loops
//
// In our implementation:
//   - Rules are expressed as Rewrite{pattern, replacement, predicate}
//   - Strategies are composable using combinators (| for choice, >> for
//   sequence)
//   - Termination is ensured through careful rule ordering
//
// AVOIDING INFINITE LOOPS:
// ------------------------
// Infinite rewrite loops occur when rules cyclically transform expressions:
//   Example: (a + b) → (b + a) and (b + a) → (a + b) loops forever
//
// We prevent this through THREE mechanisms:
//
// 1. **Directional Rules with Total Ordering**:
//    Use predicates to ensure rules only fire in one direction.
//    Example: Rewrite{x_ + y_, y_ + x_, [](ctx) { return y < x; }}
//    This establishes a canonical order, preventing oscillation.
//
// 2. **Rule Category Ordering**:
//    Apply rule categories in a specific sequence to avoid conflicts.
//    Critical ordering in this file:
//      - Identity rules (0 + x → x)
//      - Distribution rules ((a+b)·c → a·c + b·c)
//      - Associativity rules ((a+b)+c → a+(b+c))
//
//    Why? Distribution MUST precede Associativity. Otherwise:
//      - Distribution: (a+b)·c → a·c + b·c
//      - Associativity: a·c + b·c → (a·c + b)·c  [WRONG! Re-factors]
//
// 3. **Bounded Iteration**:
//    Use Repeat<Strategy, N> for fixed iteration count, or FixPoint for
//    convergence detection (stops when expression stops changing).
//
// NORMALIZATION & CANONICAL FORMS:
// ---------------------------------
// To minimize rule count and ensure deterministic results, we normalize:
//   - Subtraction → addition:     a - b  →  a + (-1)·b
//   - Division → multiplication:  a / b  →  a · b^(-1)
//   - Negation → multiplication:  -x     →  (-1)·x
//
// This means we only need rules for +, ·, and ^ (not -, /, or unary minus).
// All expressions are reduced to a single canonical form.
//
// CONSTANT LITERAL SYNTAX:
// -------------------------
// We use the _c suffix for compile-time numeric literals: 0_c, 1_c, 2_c
//
// **CRITICAL DISTINCTION**:
//   - Constant<-1>{}  is an atomic constant with value -1
//   - -1_c            is parsed as -(1_c), creating Expression<Neg,
//   Constant<1>>
//
// Due to C++ operator precedence, -N_c is ALWAYS a negation expression, never
// an atomic constant. When writing patterns:
//   - Use Constant<N>{} for matching specific atomic constants
//   - Use N_c for building replacement expressions (more readable)
//
// Example:
//   Rewrite{x_ * Constant<0>{}, Constant<0>{}}  // Matches atomic 0
//   Rewrite{x_ * 0_c, 0_c}                       // Also works, but less
//   explicit
//
// COMBINATOR ARCHITECTURE:
// ------------------------
// Rules are Strategies, enabling direct composition without adapters:
//
//   auto rule1 = Rewrite{pattern1, replacement1};
//   auto rule2 = Rewrite{pattern2, replacement2};
//   auto combined = rule1 | rule2;  // Try rule1, if fails try rule2
//
// Available combinators:
//   - r1 | r2        Choice: try r1, if unchanged try r2
//   - r1 >> r2       Sequence: apply r1, then r2 to result
//   - Repeat<R, N>   Apply R up to N times
//   - FixPoint<R>    Apply R until convergence (no more changes)
//
// This design eliminates the need for explicit RewriteSystem as an
// intermediary. Every Rewrite IS a Strategy, and combinators compose them
// naturally.

namespace tempura::symbolic3 {

// ============================================================================
// Power Simplification Rules
// ============================================================================

// x^0 → 1, x^1 → x, 1^x → 1, 0^x → 0, (x^a)^b → x^(a·b)
constexpr auto power_zero = Rewrite{pow(x_, 0_c), 1_c};
constexpr auto power_one = Rewrite{pow(x_, 1_c), x_};
constexpr auto one_power = Rewrite{pow(1_c, x_), 1_c};
constexpr auto zero_power = Rewrite{pow(0_c, x_), 0_c};
constexpr auto power_composition =
    Rewrite{pow(pow(x_, a_), b_), pow(x_, a_* b_)};

// Combine with choice (|) - tries each in order
constexpr auto PowerRules =
    power_zero | power_one | one_power | zero_power | power_composition;

// ============================================================================
// Constant Folding - Evaluate pure constant expressions at compile-time
// ============================================================================

// Fold expressions with only constant arguments into a single constant
// Uses C++26 constexpr evaluation to compute the result at compile-time
template <typename Op, Symbolic... Args>
  requires((match(𝐜, Args{}) && ...) && sizeof...(Args) > 0)
constexpr auto foldConstants(Expression<Op, Args...> expr) {
  return Constant<evaluate(expr, BinderPack{})>{};
}

// Strategy wrapper for constant folding - integrates with the combinator system
// Returns Never if not foldable (for use with Choice combinator)
struct ConstantFold {
  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context) const {
    if constexpr (requires { foldConstants(expr); }) {
      return foldConstants(expr);
    } else {
      return Never{};  // Signal failure for Choice combinator
    }
  }
};

constexpr inline ConstantFold constant_fold{};

// ============================================================================
// Exact Arithmetic - Promote integer division to fractions
// ============================================================================

// Promote integer division to Fraction when result is non-integer
// This maintains exact arithmetic instead of premature float conversion
// Examples:
//   6 / 2  → 3 (exact integer result - fold to Constant<3>)
//   5 / 2  → Fraction<5, 2> (non-integer result - keep exact)
//   5.0 / 2 → 2.5 (float involved - fold to float)
//
// NOTE: Automatic division-to-fraction promotion is NOT YET IMPLEMENTED
// due to C++ template metaprogramming challenges with return type deduction.
// See FRACTION_IMPLEMENTATION_SUMMARY.md for details and workarounds.
//
// For now, use fractions manually:
//   auto half = Fraction<1, 2>{};
//   auto expr = x * half;

// ============================================================================
// Addition Simplification Rules
// ============================================================================
//
// Rule Categories (applied in this order):
//   1. Identity   : 0 + x → x
//   2. LikeTerms  : x + x → 2·x
//   3. Ordering   : y + x → x + y  (when x < y)
//   4. Factoring  : x·a + x·b → x·(a+b)
//   5. Associativity: Strategic reassociation to group like terms
//
// The ordering ensures termination and establishes canonical forms.

namespace AdditionRuleCategories {

// ────────────────────────────────────────────────────────────────────────────
// Identity Rules: Eliminate additive identity (zero)
// ────────────────────────────────────────────────────────────────────────────

constexpr auto zero_left = Rewrite{0_c + x_, x_};
constexpr auto zero_right = Rewrite{x_ + 0_c, x_};
constexpr auto Identity = zero_left | zero_right;

// ────────────────────────────────────────────────────────────────────────────
// Like Terms: Collect identical terms
// ────────────────────────────────────────────────────────────────────────────

// x + x → 2·x
constexpr auto LikeTerms = Rewrite{x_ + x_, x_ * 2_c};

// ────────────────────────────────────────────────────────────────────────────
// Canonical Ordering: Establish total order to prevent rewrite loops
// ────────────────────────────────────────────────────────────────────────────

// y + x → x + y  when x < y (using term-structure-aware comparison)
// Uses compareAdditionTerms() from term_structure.h for algebraic ordering:
//   - Groups terms by their base: x, 2*x, 3*x are adjacent
//   - Within same base, sorts by coefficient: x < 2*x < 3*x
//   - Constants come first: 5 < x < 2*x < y < 3*y
// Example: 3*x + y + 2*y + x → x + 3*x + y + 2*y (ready for factoring)
constexpr auto canonical_order = Rewrite{
    y_ + x_, x_ + y_, [](auto ctx) {
      return compareAdditionTerms(get(ctx, x_), get(ctx, y_)) == Ordering::Less;
    }};
constexpr auto Ordering = canonical_order;

// ────────────────────────────────────────────────────────────────────────────
// Factoring: Extract common factors to enable term collection
// ────────────────────────────────────────────────────────────────────────────

// x·a + x → x·(a+1)  (factor out x from scaled and unscaled term)
constexpr auto factor_simple = Rewrite{x_ * a_ + x_, x_*(a_ + 1_c)};

// x + x·a → x·(1+a)  (symmetric case)
constexpr auto factor_simple_rev = Rewrite{x_ + x_ * a_, x_ * (1_c + a_)};

// x·a + x·b → x·(a+b)  (factor out common base from two scaled terms)
constexpr auto factor_both = Rewrite{x_ * a_ + x_ * b_, x_*(a_ + b_)};

constexpr auto Factoring = factor_simple | factor_simple_rev | factor_both;

// Associativity Rules with Canonical Ordering
// -----------------------------------------------
// Bidirectional associativity with conditional reordering for canonical form.
//
// Strategy: Use both left and right association, with ordering predicates
// to ensure termination and establish a canonical form where a < b < c.
// Uses term-structure-aware comparison to group like terms.
// The fixpoint combinator will find a stable form.
//
// Ordering ensures:
//   - Terms are arranged in canonical order (smaller terms first)
//   - Like terms are grouped together (e.g., x + (2*x + y) → (x + 2*x) + y)
//   - Prevents infinite rewrite loops

// Left-associate: a + (b + c) → (a + b) + c  when a ≤ b (term-aware)
// Groups like terms when a and b have the same base: x + (2*x + y) → (x + 2*x)
// + y
constexpr auto assoc_left = Rewrite{
    a_ + (b_ + c_), (a_ + b_) + c_, [](auto ctx) {
      return compareAdditionTerms(get(ctx, b_), get(ctx, a_)) != Ordering::Less;
    }};

// Right-associate: (a + c) + b → a + (c + b)  when b < c (term-aware)
// Bubbles smaller term b rightward to maintain canonical ordering a < b < c
constexpr auto assoc_right = Rewrite{
    (a_ + c_) + b_, a_ + (c_ + b_), [](auto ctx) {
      return compareAdditionTerms(get(ctx, b_), get(ctx, c_)) == Ordering::Less;
    }};

// Reorder within right-side: a + (c + b) → a + (b + c)  when b < c (term-aware)
// Swaps rightmost terms to maintain canonical ordering a < b < c
constexpr auto assoc_reorder = Rewrite{
    a_ + (c_ + b_), a_ + (b_ + c_), [](auto ctx) {
      return compareAdditionTerms(get(ctx, b_), get(ctx, c_)) == Ordering::Less;
    }};

constexpr auto Associativity = assoc_left | assoc_right | assoc_reorder;

}  // namespace AdditionRuleCategories

// ────────────────────────────────────────────────────────────────────────────
// Combined Addition Rules (Choice Combinator)
// ────────────────────────────────────────────────────────────────────────────
//
// Order matters for efficiency and correctness:
//   - Identity first (simple, fast pattern match)
//   - LikeTerms before Factoring (simpler pattern: x+x vs x·a+x·b)
//   - Ordering before Associativity (establishes canonical form first)
//   - Factoring before Associativity (groups terms before rearrangement)
//   - Associativity last (most complex, benefits from prior simplifications)

constexpr auto AdditionRules =
    AdditionRuleCategories::Identity | AdditionRuleCategories::LikeTerms |
    AdditionRuleCategories::Ordering | AdditionRuleCategories::Factoring |
    AdditionRuleCategories::Associativity;

// ============================================================================
// Multiplication Simplification Rules
// ============================================================================
//
// Rule Categories (applied in this order):
//   1. Identity      : 0·x → 0, 1·x → x
//   2. Distribution  : (a+b)·c → a·c + b·c
//   3. Ordering      : y·x → x·y  (when x < y, by base then exponent)
//   4. PowerCombining: x·x^a → x^(a+1)
//   5. Associativity : Strategic reassociation to group like bases
//
// The ordering ensures termination and establishes canonical forms.

namespace MultiplicationRuleCategories {

// ────────────────────────────────────────────────────────────────────────────
// Identity Rules: Eliminate multiplicative identity/annihilator
// ────────────────────────────────────────────────────────────────────────────

constexpr auto zero_left = Rewrite{0_c * x_, 0_c};
constexpr auto zero_right = Rewrite{x_ * 0_c, 0_c};
constexpr auto one_left = Rewrite{1_c * x_, x_};
constexpr auto one_right = Rewrite{x_ * 1_c, x_};
constexpr auto Identity = zero_left | zero_right | one_left | one_right;

// ────────────────────────────────────────────────────────────────────────────
// Distribution: Expand products over sums
// ────────────────────────────────────────────────────────────────────────────

// (a+b)·c → a·c + b·c  (distribute from left)
constexpr auto dist_right = Rewrite{(a_ + b_) * c_, (a_ * c_) + (b_ * c_)};

// a·(b+c) → a·b + a·c  (distribute from right)
constexpr auto dist_left = Rewrite{a_ * (b_ + c_), (a_ * b_) + (a_ * c_)};

constexpr auto Distribution = dist_right | dist_left;

// CRITICAL: Distribution must come before Associativity to prevent re-factoring

// ────────────────────────────────────────────────────────────────────────────
// Canonical Ordering: Establish total order to prevent rewrite loops
// ────────────────────────────────────────────────────────────────────────────

// y·x → x·y  when x < y (using term-structure-aware comparison)
// Uses compareMultiplicationTerms() from term_structure.h for algebraic
// ordering:
//   - Groups terms by their base: x, x^2, x^3 are adjacent
//   - Within same base, sorts by exponent: x < x^2 < x^3
//   - Constants come first: 2 < x < x^2 < y < y^2
// Example: x^3 · y · x · y^2 · x^2 → x · x^2 · x^3 · y · y^2 (ready for power
// combining)
constexpr auto canonical_order =
    Rewrite{y_ * x_, x_* y_, [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, x_), get(ctx, y_)) ==
                     Ordering::Less;
            }};
constexpr auto Ordering = canonical_order;

// ────────────────────────────────────────────────────────────────────────────
// Power Combining: Collect terms with the same base
// ────────────────────────────────────────────────────────────────────────────

// x·x^a → x^(a+1)  (combine base with power of same base)
constexpr auto power_base_left = Rewrite{x_ * pow(x_, a_), pow(x_, a_ + 1_c)};

// x^a·x → x^(a+1)  (symmetric case)
constexpr auto power_base_right = Rewrite{pow(x_, a_) * x_, pow(x_, a_ + 1_c)};

// x^a·x^b → x^(a+b)  (combine two powers of same base)
constexpr auto power_both =
    Rewrite{pow(x_, a_) * pow(x_, b_), pow(x_, a_ + b_)};

constexpr auto PowerCombining = power_base_left | power_base_right | power_both;

// Associativity Rules with Canonical Ordering
// -----------------------------------------------
// Bidirectional associativity with conditional reordering for canonical form.
//
// Strategy: Use both left and right association, with ordering predicates
// to ensure termination and establish a canonical form where a < b < c.
// Uses term-structure-aware comparison to group like bases (powers).
// The fixpoint combinator will find a stable form.
//
// Ordering ensures:
//   - Terms are arranged in canonical order (smaller terms first)
//   - Like bases are grouped together (e.g., x · (x^2 · y) → (x · x^2) · y)
//   - Prevents infinite rewrite loops

// Left-associate: a · (b · c) → (a · b) · c  when a ≤ b (term-aware)
// Groups like bases when they have the same base: x · (x^2 · y) → (x · x^2) · y
constexpr auto assoc_left =
    Rewrite{a_ * (b_ * c_), (a_ * b_) * c_, [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, b_), get(ctx, a_)) !=
                     Ordering::Less;
            }};

// Right-associate: (a · c) · b → a · (c · b)  when b < c (term-aware)
// Bubbles smaller term b rightward to maintain canonical ordering a < b < c
constexpr auto assoc_right =
    Rewrite{(a_ * c_) * b_, a_*(c_* b_), [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, b_), get(ctx, c_)) ==
                     Ordering::Less;
            }};

// Reorder within right-side: a · (c · b) → a · (b · c)  when b < c (term-aware)
// Swaps rightmost terms to maintain canonical ordering a < b < c
constexpr auto assoc_reorder =
    Rewrite{a_ * (c_ * b_), a_*(b_* c_), [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, b_), get(ctx, c_)) ==
                     Ordering::Less;
            }};

constexpr auto Associativity = assoc_left | assoc_right | assoc_reorder;

}  // namespace MultiplicationRuleCategories

// ────────────────────────────────────────────────────────────────────────────
// Combined Multiplication Rules (Choice Combinator)
// ────────────────────────────────────────────────────────────────────────────
//
// Order matters for efficiency and correctness:
//   - Identity first (simple, fast pattern match - also catches annihilator 0)
//   - Distribution before Associativity (CRITICAL: prevents re-factoring)
//   - Ordering before PowerCombining (establishes canonical form first)
//   - PowerCombining before Associativity (groups powers before rearrangement)
//   - Associativity last (most complex, benefits from prior simplifications)

// TEMPORARY FIX: Distribution disabled to prevent oscillation with Factoring
// Distribution and Factoring are inverses:
//   - Factoring: x·a + x·b → x·(a+b)  [collect like terms]
//   - Distribution: x·(a+b) → x·a + x·b  [expand products]
// When both are in the same pipeline, they fight each other causing
// oscillation. For simplification, we prefer Factoring so we can fold nested
// constants.
// TODO: Implement conditional Distribution (only when beneficial)
constexpr auto MultiplicationRules =
    MultiplicationRuleCategories::Identity |
    // MultiplicationRuleCategories::Distribution |  // DISABLED - conflicts
    // with Factoring
    MultiplicationRuleCategories::Ordering |
    MultiplicationRuleCategories::PowerCombining |
    MultiplicationRuleCategories::Associativity;

// ============================================================================
// Exponential and Logarithm Rules
// ============================================================================

namespace ExpRuleCategories {
// exp(log(x)) → x, exp(0) → 1
constexpr auto Inverse = Rewrite{exp(log(x_)), x_};
constexpr auto Identity = Rewrite{exp(0_c), 1_c};

// Exponential laws: exp(a+b) → exp(a)*exp(b), exp(a-b) → exp(a)/exp(b)
constexpr auto sum_to_product = Rewrite{exp(a_ + b_), exp(a_) * exp(b_)};
constexpr auto diff_to_quotient = Rewrite{exp(a_ - b_), exp(a_) / exp(b_)};
constexpr auto Expansion = sum_to_product | diff_to_quotient;

// Special case: exp(n*log(a)) → a^n (inverse of log power rule)
constexpr auto log_power_inverse = Rewrite{exp(n_ * log(a_)), pow(a_, n_)};

}  // namespace ExpRuleCategories

constexpr auto ExpRules =
    ExpRuleCategories::Inverse | ExpRuleCategories::Identity |
    ExpRuleCategories::Expansion | ExpRuleCategories::log_power_inverse;

namespace LogRuleCategories {
// log(1) → 0, log(exp(x)) → x
constexpr auto Identity = Rewrite{log(1_c), 0_c};
constexpr auto Inverse = Rewrite{log(exp(x_)), x_};

// Expansion: log(x^a) → a·log(x), log(x·y) → log(x) + log(y), log(x/y) →
// log(x) - log(y)
constexpr auto power_rule = Rewrite{log(pow(x_, a_)), a_* log(x_)};
constexpr auto product_rule = Rewrite{log(x_ * y_), log(x_) + log(y_)};
constexpr auto quotient_rule = Rewrite{log(x_ / y_), log(x_) - log(y_)};
constexpr auto Expansion = power_rule | product_rule | quotient_rule;

}  // namespace LogRuleCategories

constexpr auto LogRules = LogRuleCategories::Identity |
                          LogRuleCategories::Inverse |
                          LogRuleCategories::Expansion;

// ============================================================================
// Trigonometric Function Rules
// ============================================================================

namespace SinRuleCategories {
// sin(0) → 0
constexpr auto Identity = Rewrite{sin(0_c), 0_c};

// Symmetry: sin(-x) → -sin(x) (odd function)
constexpr auto Symmetry = Rewrite{sin(-x_), -sin(x_)};

// Double angle formula: sin(2*x) → 2*sin(x)*cos(x)
constexpr auto double_angle = Rewrite{sin(2_c * x_), 2_c * sin(x_) * cos(x_)};

}  // namespace SinRuleCategories

constexpr auto SinRules = SinRuleCategories::Identity |
                          SinRuleCategories::Symmetry |
                          SinRuleCategories::double_angle;

namespace CosRuleCategories {
// cos(0) → 1
constexpr auto Identity = Rewrite{cos(0_c), 1_c};

// Symmetry: cos(-x) → cos(x) (even function)
constexpr auto Symmetry = Rewrite{cos(-x_), cos(x_)};

// Double angle formula: cos(2*x) → cos²(x) - sin²(x)
// Note: we use pow(..., 2_c) for squaring
constexpr auto double_angle =
    Rewrite{cos(2_c * x_), pow(cos(x_), 2_c) - pow(sin(x_), 2_c)};

}  // namespace CosRuleCategories

constexpr auto CosRules = CosRuleCategories::Identity |
                          CosRuleCategories::Symmetry |
                          CosRuleCategories::double_angle;

namespace TanRuleCategories {
// tan(0) → 0
constexpr auto Identity = Rewrite{tan(0_c), 0_c};

// Symmetry: tan(-x) → -tan(x) (odd function)
constexpr auto Symmetry = Rewrite{tan(-x_), -tan(x_)};

// Definition: tan(x) → sin(x)/cos(x)
constexpr auto definition = Rewrite{tan(x_), sin(x_) / cos(x_)};

}  // namespace TanRuleCategories

constexpr auto TanRules = TanRuleCategories::Identity |
                          TanRuleCategories::Symmetry |
                          TanRuleCategories::definition;

// ============================================================================
// Hyperbolic Function Rules
// ============================================================================

namespace SinhRuleCategories {
// sinh(0) → 0
constexpr auto Identity = Rewrite{sinh(0_c), 0_c};

// Symmetry: sinh(-x) → -sinh(x) (odd function)
constexpr auto Symmetry = Rewrite{sinh(-x_), -sinh(x_)};

// Definition: sinh(x) → (exp(x) - exp(-x))/2
constexpr auto definition = Rewrite{sinh(x_), (exp(x_) - exp(-x_)) / 2_c};

}  // namespace SinhRuleCategories

constexpr auto SinhRules = SinhRuleCategories::Identity |
                           SinhRuleCategories::Symmetry |
                           SinhRuleCategories::definition;

namespace CoshRuleCategories {
// cosh(0) → 1
constexpr auto Identity = Rewrite{cosh(0_c), 1_c};

// Symmetry: cosh(-x) → cosh(x) (even function)
constexpr auto Symmetry = Rewrite{cosh(-x_), cosh(x_)};

// Definition: cosh(x) → (exp(x) + exp(-x))/2
constexpr auto definition = Rewrite{cosh(x_), (exp(x_) + exp(-x_)) / 2_c};

}  // namespace CoshRuleCategories

constexpr auto CoshRules = CoshRuleCategories::Identity |
                           CoshRuleCategories::Symmetry |
                           CoshRuleCategories::definition;

namespace TanhRuleCategories {
// tanh(0) → 0
constexpr auto Identity = Rewrite{tanh(0_c), 0_c};

// Symmetry: tanh(-x) → -tanh(x) (odd function)
constexpr auto Symmetry = Rewrite{tanh(-x_), -tanh(x_)};

// Definition: tanh(x) → sinh(x)/cosh(x)
constexpr auto definition = Rewrite{tanh(x_), sinh(x_) / cosh(x_)};

// Alternative definition: tanh(x) → (exp(2x) - 1)/(exp(2x) + 1)
constexpr auto exp_definition =
    Rewrite{tanh(x_), (exp(2_c * x_) - 1_c) / (exp(2_c * x_) + 1_c)};

}  // namespace TanhRuleCategories

constexpr auto TanhRules = TanhRuleCategories::Identity |
                           TanhRuleCategories::Symmetry |
                           TanhRuleCategories::definition;

// ============================================================================
// Hyperbolic Identity Rules
// ============================================================================

namespace HyperbolicIdentityCategories {
// cosh²(x) - sinh²(x) → 1
constexpr auto cosh_sinh_identity =
    Rewrite{pow(cosh(x_), 2_c) - pow(sinh(x_), 2_c), 1_c};

// Derived forms: cosh²(x) → 1 + sinh²(x), sinh²(x) → cosh²(x) - 1
constexpr auto cosh_squared =
    Rewrite{pow(cosh(x_), 2_c), 1_c + pow(sinh(x_), 2_c)};
constexpr auto sinh_squared =
    Rewrite{pow(sinh(x_), 2_c), pow(cosh(x_), 2_c) - 1_c};

}  // namespace HyperbolicIdentityCategories

constexpr auto HyperbolicIdentityRules =
    HyperbolicIdentityCategories::cosh_sinh_identity;

// ============================================================================
// Pythagorean Identity Rules
// ============================================================================

namespace PythagoreanRuleCategories {
// sin²(x) + cos²(x) → 1
constexpr auto sin_cos_identity =
    Rewrite{pow(sin(x_), 2_c) + pow(cos(x_), 2_c), 1_c};

// cos²(x) + sin²(x) → 1 (commutative variant)
constexpr auto cos_sin_identity =
    Rewrite{pow(cos(x_), 2_c) + pow(sin(x_), 2_c), 1_c};

// Derived forms: sin²(x) → 1 - cos²(x), cos²(x) → 1 - sin²(x)
constexpr auto sin_squared =
    Rewrite{pow(sin(x_), 2_c), 1_c - pow(cos(x_), 2_c)};
constexpr auto cos_squared =
    Rewrite{pow(cos(x_), 2_c), 1_c - pow(sin(x_), 2_c)};

}  // namespace PythagoreanRuleCategories

constexpr auto PythagoreanRules = PythagoreanRuleCategories::sin_cos_identity |
                                  PythagoreanRuleCategories::cos_sin_identity;

// ============================================================================
// Square Root Rules
// ============================================================================

namespace SqrtRuleCategories {
// sqrt(0) → 0, sqrt(1) → 1
constexpr auto Identity = Rewrite{sqrt(0_c), 0_c} | Rewrite{sqrt(1_c), 1_c};

// sqrt(x^2) → x (assuming x ≥ 0)
constexpr auto power_inverse = Rewrite{sqrt(pow(x_, 2_c)), x_};

// sqrt(x·y) → sqrt(x)·sqrt(y)
constexpr auto product_rule = Rewrite{sqrt(x_ * y_), sqrt(x_) * sqrt(y_)};

}  // namespace SqrtRuleCategories

constexpr auto SqrtRules = SqrtRuleCategories::Identity |
                           SqrtRuleCategories::power_inverse |
                           SqrtRuleCategories::product_rule;

// ============================================================================
// Combined Simplification Strategy
// ============================================================================

// Transcendental functions
constexpr auto transcendental_simplify =
    ExpRules | LogRules | SinRules | CosRules | TanRules | SinhRules |
    CoshRules | TanhRules | SqrtRules | PythagoreanRules |
    HyperbolicIdentityRules;

// Basic algebraic simplification: apply all rule categories, then fold
// constants Constant folding comes AFTER structural rules so we simplify
// expressions before evaluating (e.g., (1+2)+3 → 3+3 → 6 rather than 3+3
// staying as-is)
constexpr auto algebraic_simplify = PowerRules | AdditionRules |
                                    MultiplicationRules |
                                    transcendental_simplify | constant_fold;

// Apply using FixPoint (keeps going until no more changes)
constexpr auto simplify_fixpoint = FixPoint{algebraic_simplify};

// Alternative: just one pass
constexpr auto simplify_once = algebraic_simplify;

// ============================================================================
// LEGACY SIMPLIFICATION (Not Recommended)
// ============================================================================
//
// The following simplification strategies are kept for backward compatibility
// but are NOT RECOMMENDED for new code. They have limitations:
//
//   - simplify_bounded: Uses fixed iteration count (10 passes), which may be
//     insufficient for complex expressions or wasteful for simple ones
//   - Does not traverse nested expressions recursively
//   - May leave subexpressions unsimplified
//
// For new code, use `simplify` (which is now an alias for `full_simplify`)
// or the explicit pipeline functions below.

// Apply simplification with bounded iteration (legacy)
constexpr auto simplify_bounded =
    Repeat<decltype(algebraic_simplify), 10>{algebraic_simplify};

// ============================================================================
// Comprehensive Simplification Pipelines
// ============================================================================
//
// These pipelines combine algebraic simplification with traversal strategies
// for robust, recursive simplification of nested expressions.
//
// HIERARCHY (from most to least recommended):
//
// 1. simplify / full_simplify (CANONICAL)
//    - Multi-stage fixpoint pipeline
//    - Handles all nesting and rule interactions
//    - Use this for correctness
//
// 2. algebraic_simplify_recursive
//    - Single pass per node, recursive traversal
//    - Faster but may miss some simplifications
//    - Use for performance-critical paths
//
// 3. bottomup_simplify / topdown_simplify
//    - Explicit traversal order control
//    - Use when you need specific traversal semantics
//
// 4. trig_aware_simplify
//    - Specialized for trigonometric expressions
//    - Currently similar to simplify, may diverge in future
//
// The pipelines build on the basic `algebraic_simplify` rules but add:
//   1. Recursive traversal (innermost/bottomup/topdown)
//   2. Fixpoint iteration for exhaustive simplification
//   3. Context-aware behavior for different use cases

// Forward declarations from traversal.h (included by clients)
// These are template functions, so we can't declare them constexpr here
// They must be included via: #include "symbolic3/traversal.h"

// ============================================================================
// CANONICAL SIMPLIFICATION (Recommended for All Use Cases)
// ============================================================================
//
// This is the primary simplification function implementing a robust
// multi-stage, fixed-point pipeline that mimics how mathematicians simplify:
//
// PIPELINE STRUCTURE:
// 1. **Innermost Traversal**: Start at leaves and work upward
// 2. **Main Rewrite Loop**: Apply fixpoint iteration at each node
// 3. **Outer Fixpoint**: Repeat until entire tree is stable
//
// This architecture ensures:
//   - Nested arithmetic: x * (y + (z * 0))  →  x * y
//   - Deep expressions: ((x + 0) * 1) + 0   →  x
//   - Mixed operations: exp(log(x + 0))     →  x
//   - Term collection: (x+x)+x              →  3*x
//   - Associativity changes are fully propagated
//
// WHY THIS WORKS:
//   - Innermost ensures leaves are simplified before parents
//   - Inner fixpoint (simplify_fixpoint) exhaustively applies rules at each
//     node until stable, handling cases like (x+x)+x → 2*x+x → 3*x
//   - Outer fixpoint handles rules that restructure the tree (like
//     distribution a*(b+c) → a*b + a*c), ensuring newly created
//     subexpressions are also simplified
//
// Usage:
//   auto result = simplify(expr, default_context());
//
// Implementation note:
//   This is a lambda wrapper around FixPoint{innermost(simplify_fixpoint)}.
//   The nested fixpoint structure is critical for correctness.
// Helper strategy for two-stage simplification
// Stage 1: Apply algebraic rules (factoring, etc.) to whole expression
// Stage 2: Fold nested constants created by factoring
struct TwoStageSimplify {
  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Apply algebraic rules first (enables factoring at parent level)
    auto with_alg_rules = algebraic_simplify.apply(expr, ctx);

    // Then fold any nested constants using innermost traversal
    // (constant_fold wrapped in Try so it doesn't break on non-constants)
    return innermost(try_strategy(constant_fold)).apply(with_alg_rules, ctx);
  }
};

constexpr inline TwoStageSimplify two_stage_simplify{};

constexpr auto full_simplify = [](auto expr, auto ctx) {
  // Must include traversal.h to use this
  //
  // Two-stage pipeline with FixPoint:
  // 1. Apply algebraic_simplify to the whole expression (enables factoring)
  // 2. Apply innermost(constant_fold) to fold nested constants
  // 3. Repeat until stable
  //
  // WHY THIS WORKS:
  // - algebraic_simplify includes factoring rules that need to match the
  //   parent-level pattern (e.g., x·a + x → x·(a+1))
  // - Wrapping algebraic rules in innermost() would apply them to children
  //   first, preventing parent patterns from matching
  // - So we apply algebraic rules to the whole expression first
  // - Then use innermost to fold any nested constants created by factoring
  //
  // CRITICAL: constant_fold wrapped in Try so it doesn't break traversal when
  // applied to non-constant expressions
  return FixPoint{two_stage_simplify}.apply(expr, ctx);
};

// ============================================================================
// PRIMARY SIMPLIFICATION INTERFACE
// ============================================================================
//
// `simplify` is now an alias for `full_simplify` following Recommendation 1
// from SYMBOLIC3_RECOMMENDATIONS.md.
//
// This provides:
//   - Multi-stage pipeline (innermost → fixpoint → outer fixpoint)
//   - Handles all nesting levels correctly
//   - Robust against complex expressions
//   - Predictable, deterministic results
//
// MIGRATION NOTES:
//   - Old code using `simplify` will now get the improved behavior
//   - The old bounded-iteration version is available as `simplify_bounded`
//   - Most code should not need changes, just better results
//
// Usage:
//   auto result = simplify(expr, default_context());
constexpr auto simplify = full_simplify;

// ============================================================================
// Algebraic Simplify with Traversal (Fast)
// ============================================================================
//
// Lighter-weight version that does ONE pass of simplification per node
// instead of fixpoint iteration. Faster but may miss some simplifications.
//
// Good for:
//   - Performance-critical paths
//   - Expressions that are already mostly simplified
//   - Quick cleanup passes
//
// Usage:
//   auto result = algebraic_simplify_recursive(expr, default_context());
constexpr auto algebraic_simplify_recursive = [](auto expr, auto ctx) {
  return innermost(algebraic_simplify).apply(expr, ctx);
};

// ============================================================================
// Bottom-Up Simplification
// ============================================================================
//
// Applies simplification in post-order (children before parents).
// Similar to innermost but with slightly different traversal order.
//
// Use when you want explicit control over traversal order.
//
// Usage:
//   auto result = bottomup_simplify(expr, default_context());
constexpr auto bottomup_simplify = [](auto expr, auto ctx) {
  return bottomup(algebraic_simplify).apply(expr, ctx);
};

// ============================================================================
// Top-Down Simplification
// ============================================================================
//
// Applies simplification in pre-order (parents before children).
// Useful when parent simplifications enable child simplifications.
//
// Example: log(exp(x + 0))
//   - Top-down: log(exp(...)) → (x + 0) → x    [Two passes]
//   - Bottom-up: (x + 0) → x, log(exp(x)) → x [Also works]
//
// Usage:
//   auto result = topdown_simplify(expr, default_context());
constexpr auto topdown_simplify = [](auto expr, auto ctx) {
  return topdown(algebraic_simplify).apply(expr, ctx);
};

// ============================================================================
// Trigonometric-Aware Simplification (Experimental)
// ============================================================================
//
// Combines algebraic simplification with trigonometric identities.
// Useful for expressions involving sin, cos, tan.
//
// Handles:
//   - Pythagorean identity: sin²(x) + cos²(x) → 1
//   - Angle simplification: sin(-x) → -sin(x)
//   - Special values: sin(0) → 0, cos(0) → 1
//
// Usage:
//   auto result = trig_aware_simplify(expr, default_context());
//
// Note: This is the same as full_simplify currently since
// transcendental_simplify is already included in algebraic_simplify.
// Future versions may add more sophisticated trig rules.
constexpr auto trig_aware_simplify = [](auto expr, auto ctx) {
  return innermost(transcendental_simplify | algebraic_simplify)
      .apply(expr, ctx);
};

// ============================================================================
// Usage Guide
// ============================================================================
//
// QUICK START (Most Use Cases):
//
//   #include "symbolic3/symbolic3.h"
//
//   auto expr = x * (y + (z * 0));
//   auto result = simplify(expr, default_context());  // x * y
//
// This gives you the full multi-stage simplification pipeline.
//
// ============================================================================
// When to Use Each Pipeline:
// ============================================================================
//
// ✅ simplify(expr, ctx)
//    - DEFAULT CHOICE for all use cases
//    - Handles nested expressions correctly
//    - Applies fixpoint iteration at all levels
//    - Predictable, robust results
//
// ⚡ algebraic_simplify_recursive(expr, ctx)
//    - PERFORMANCE-CRITICAL paths only
//    - Single pass per node (may miss some simplifications)
//    - Use when expressions are already mostly simplified
//
// 🔧 bottomup_simplify(expr, ctx) / topdown_simplify(expr, ctx)
//    - EXPLICIT TRAVERSAL ORDER control
//    - Use when you need specific semantics
//    - bottomup: children before parents
//    - topdown: parents before children
//
// 📐 trig_aware_simplify(expr, ctx)
//    - TRIGONOMETRIC EXPRESSIONS
//    - Currently similar to simplify, may specialize in future
//
// 🔍 algebraic_simplify.apply(expr, ctx)
//    - LOW-LEVEL tool for custom pipelines
//    - Single node, no traversal
//    - Use as building block for custom strategies
//
// ============================================================================
// Custom Pipelines (Advanced):
// ============================================================================
//
// For specialized needs, combine basic rules with traversal strategies:
//
// Traversal strategy factories (template functions from traversal.h):
//   - innermost(rules) - apply at leaves first, propagate upward (recommended)
//   - bottomup(rules)  - post-order traversal with rule application
//   - topdown(rules)   - pre-order traversal with rule application
//   - outermost(rules) - apply at root first, recurse if changed
//
// Example custom pipeline:
//   #include "symbolic3/traversal.h"
//
//   // Only power and addition rules, exhaustive
//   auto custom = FixPoint{innermost(PowerRules | AdditionRules)};
//   auto result = custom.apply(expr, ctx);
//
// Example comparison:
//   auto expr = x * (y + 0);
//   auto ctx = default_context();
//
//   // ✅ Recommended (exhaustive, correct):
//   auto result = simplify(expr, ctx);  // x * y
//
//   // ⚡ Fast (single pass per node):
//   auto fast = algebraic_simplify_recursive(expr, ctx);  // x * y
//
//   // 🔍 Low-level (top-level only, leaves nested issues):
//   auto partial = algebraic_simplify.apply(expr, ctx);  // x * (y + 0)
//   [incomplete]
//
// ============================================================================
// Migration from Old Code:
// ============================================================================
//
// If your code uses the old `simplify` (bounded iteration):
//
//   OLD: auto result = simplify.apply(expr, ctx);  // 10 iterations max
//   NEW: auto result = simplify(expr, ctx);         // fixpoint iteration
//
// The new `simplify` is a callable lambda, not a Strategy object, so:
//   - Use simplify(expr, ctx) instead of simplify.apply(expr, ctx)
//   - Or continue using .apply() with simplify_bounded if needed
//
// For backward compatibility:
//   auto result = simplify_bounded.apply(expr, ctx);  // old behavior

}  // namespace tempura::symbolic3
