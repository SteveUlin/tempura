#pragma once

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/evaluate.h"
#include "symbolic3/fraction.h"
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

// Helper to promote division of constants to fractions or constants
// Uses if constexpr and compile-time arithmetic to decide promotion
template <auto n, auto d>
constexpr auto promote_div_const() {
  if constexpr (d == 1) {
    // Division by 1: return numerator as constant
    return Constant<n>{};
  } else if constexpr (n % d == 0) {
    // Exact division: return quotient as constant
    return Constant<n / d>{};
  } else {
    // Non-integer division: return as reduced fraction
    // Compute reduced numerator and denominator at compile-time
    constexpr auto g = gcd(n, d);
    constexpr auto sign = ((n < 0) != (d < 0)) ? -1 : 1;
    constexpr auto reduced_num = sign * abs_val(n) / g;
    constexpr auto reduced_den = abs_val(d) / g;
    return Fraction<reduced_num, reduced_den>{};
  }
}

// Strategy: Use SFINAE-based overloading to handle each case with consistent
// return types
struct PromoteDivisionToFraction {
  // Match division of two constants
  template <auto n, auto d, typename Context>
    requires(d != 0)
  constexpr auto apply(Expression<DivOp, Constant<n>, Constant<d>>,
                       Context) const {
    return promote_div_const<n, d>();
  }

  // No match - return unchanged (for non-constant divisions like x / y)
  template <Symbolic S, typename Context>
  constexpr auto apply(S expr, Context) const {
    return expr;
  }
};

constexpr inline PromoteDivisionToFraction promote_division_to_fraction{};

// ============================================================================
// Fraction Simplification Rules
// ============================================================================

// Fractions are already in their simplest form (GCD-reduced at construction),
// but we need rules for combining fractions with other expressions

namespace FractionRuleCategories {

// ────────────────────────────────────────────────────────────────────────────
// Fraction Identity Rules
// ────────────────────────────────────────────────────────────────────────────

// Fraction<n, 1> → Constant<n> (denominator 1 means integer)
// This is handled by pattern matching on specific Fraction types
template <long long n>
constexpr auto frac_to_int = Rewrite{Fraction<n, 1>{}, Constant<n>{}};

// Fraction<0, d> → Constant<0> (zero numerator is always zero)
template <long long d>
  requires(d != 0)
constexpr auto zero_frac = Rewrite{Fraction<0, d>{}, Constant<0>{}};

// ────────────────────────────────────────────────────────────────────────────
// Fraction Arithmetic Integration
// ────────────────────────────────────────────────────────────────────────────

// When fractions appear in symbolic expressions, they participate in
// simplification like any other constant. The actual arithmetic is handled
// by the operators in fraction.h, and ConstantFold will evaluate them.

// x * Fraction<1, 1> → x  (multiply by 1)
constexpr auto mult_by_one_frac = Rewrite{x_ * Fraction<1, 1>{}, x_};

// Fraction<1, 1> * x → x  (symmetric)
constexpr auto one_frac_mult = Rewrite{Fraction<1, 1>{} * x_, x_};

// x * Fraction<0, d> → Constant<0>  (multiply by zero)
// This will be caught by zero_frac first, so not strictly necessary

}  // namespace FractionRuleCategories

// Combined fraction rules - mostly handled by constant folding
// The main work is done by PromoteDivisionToFraction and the fraction
// arithmetic operators
constexpr auto FractionRules = FractionRuleCategories::mult_by_one_frac |
                               FractionRuleCategories::one_frac_mult;

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

// Additional patterns for canonical multiplication order (constant-first):
// a·x + x → x·(a+1)  (constant-first version of factor_simple)
constexpr auto factor_simple_cf = Rewrite{a_ * x_ + x_, x_*(a_ + 1_c)};

// x + a·x → x·(1+a)  (constant-first version of factor_simple_rev)
constexpr auto factor_simple_rev_cf = Rewrite{x_ + a_ * x_, x_ * (1_c + a_)};

// a·x + b·x → x·(a+b)  (constant-first version of factor_both)
constexpr auto factor_both_cf = Rewrite{a_ * x_ + b_ * x_, x_*(a_ + b_)};

constexpr auto Factoring = factor_simple | factor_simple_rev | factor_both |
                           factor_simple_cf | factor_simple_rev_cf |
                           factor_both_cf;

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

// Basic algebraic simplification: fold constants FIRST, then apply structural
// rules
//
// CRITICAL: constant_fold must come BEFORE AdditionRules/MultiplicationRules
// to prevent canonical ordering from creating oscillations.
//
// Example problem if constant_fold comes AFTER:
//   x*(2+1) → try AdditionRules on (2+1) → swap to (1+2) by ordering
//   x*(1+2) → try AdditionRules on (1+2) → swap to (2+1) by ordering
//   Infinite oscillation! FixPoint can't detect it because types differ.
//
// With constant_fold FIRST:
//   x*(2+1) → constant_fold → x*3 ✅
//
// This doesn't break the (1+2)+3 example from the old comment because:
//   (1+2)+3 → fold (1+2) → 3+3 → fold 3+3 → 6 ✅
// CRITICAL: promote_division_to_fraction must come BEFORE constant_fold
// Otherwise constant_fold will evaluate Constant<5>{}/Constant<2>{} to
// Constant<2> using integer division, losing precision
constexpr auto algebraic_simplify =
    promote_division_to_fraction | constant_fold | PowerRules | AdditionRules |
    MultiplicationRules | FractionRules | transcendental_simplify;

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
// ============================================================================
// TWO-STAGE SIMPLIFICATION (New Implementation)
// ============================================================================
//
// This is the new, recommended simplification strategy that addresses the
// inefficiencies of the single-phase bottom-up approach.
//
// Key improvements:
//   1. Short-circuit patterns checked BEFORE recursing (0 * x → 0)
//   2. Two-phase traversal: descent (expand) then ascent (collect)
//   3. Resolves distribution/factoring conflict
//   4. More efficient (fewer fixpoint iterations)
//
// Architecture:
//   1. Quick patterns: annihilators, identities (short-circuit)
//   2. Descent phase: rules applied going down (pre-order)
//   3. Recurse into children
//   4. Ascent phase: rules applied coming up (post-order)
//   5. Fixpoint iteration until stable

// ────────────────────────────────────────────────────────────────────────────
// Phase 1: Quick Patterns (Short-Circuit)
// ────────────────────────────────────────────────────────────────────────────
//
// These patterns are checked at parent level BEFORE recursing into children.
// This enables major optimizations like 0 * (complex_expr) → 0

constexpr auto quick_annihilators = Rewrite{0_c * x_, 0_c} |  // 0 * x → 0
                                    Rewrite{x_ * 0_c, 0_c};   // x * 0 → 0

constexpr auto quick_identities =
    Rewrite{1_c * x_, x_} |      // 1 * x → x
    Rewrite{x_ * 1_c, x_} |      // x * 1 → x
    Rewrite{0_c + x_, x_} |      // 0 + x → x
    Rewrite{x_ + 0_c, x_} |      // x + 0 → x
    Rewrite{x_ * 1_c, x_} |      // x^1 → x (using multiplication for now)
    Rewrite{exp(log(x_)), x_} |  // exp(log(x)) → x
    Rewrite{log(exp(x_)), x_};   // log(exp(x)) → x

constexpr auto quick_patterns = quick_annihilators | quick_identities;

// ────────────────────────────────────────────────────────────────────────────
// Phase 2: Descent Rules (Going Down, Pre-Order)
// ────────────────────────────────────────────────────────────────────────────
//
// These rules are applied BEFORE recursing into children.
// Good for: expansion, unwrapping, transformations that don't need simplified
// children

constexpr auto descent_unwrapping =
    Rewrite{-(-x_), x_};  // Double negation: -(-x) → x

// Note: Distribution is intentionally OMITTED from descent to avoid
// oscillation with factoring in ascent. Distribution can be enabled later
// with conditional logic (only when beneficial).

constexpr auto descent_rules = descent_unwrapping;

// ────────────────────────────────────────────────────────────────────────────
// Phase 3: Ascent Rules (Coming Up, Post-Order)
// ────────────────────────────────────────────────────────────────────────────
//
// These rules are applied AFTER children are simplified.
// Good for: collection, factoring, folding, canonicalization

constexpr auto ascent_constant_folding =
    promote_division_to_fraction | constant_fold;

constexpr auto ascent_collection =
    AdditionRuleCategories::LikeTerms |  // x + x → 2*x
    AdditionRuleCategories::Factoring;   // x*a + x*b → x*(a+b)

constexpr auto ascent_power_combining =
    MultiplicationRuleCategories::PowerCombining;  // x * x^a → x^(a+1)

constexpr auto ascent_canonicalization =
    AdditionRuleCategories::Ordering |        // y + x → x + y (if x < y)
    MultiplicationRuleCategories::Ordering |  // y * x → x * y (if x < y)
    AdditionRuleCategories::Associativity |   // Strategic reassociation
    MultiplicationRuleCategories::Associativity;

constexpr auto ascent_rules = ascent_constant_folding |
                              PowerRules |  // x^0 → 1, x^1 → x, etc.
                              ascent_collection | ascent_power_combining |
                              ascent_canonicalization | transcendental_simplify;

// ────────────────────────────────────────────────────────────────────────────
// Two-Stage Pipeline Assembly
// ────────────────────────────────────────────────────────────────────────────

// Build the two-stage strategy using existing combinators
// This avoids return type inconsistency by using the strategy composition
// system

// Phase 1: Descent phase (pre-order)
// At each node going down: try quick patterns first, then descent rules
// topdown() automatically applies this strategy at every node during traversal
constexpr auto descent_with_quick = quick_patterns | descent_rules;
constexpr auto descent_phase = topdown(descent_with_quick);

// Phase 2: Ascent phase (post-order)
// At each node coming up: apply ascent rules after children are simplified
// bottomup() automatically applies this strategy at every node during traversal
constexpr auto ascent_phase = bottomup(ascent_rules);

// Combine: descent (with quick checks at each node), then ascent
// No try_strategy wrapper needed - traversals handle the per-node application
constexpr auto two_phase_core = descent_phase >> ascent_phase;

// Add fixpoint iteration to repeat until stable
// Using FixPoint with CTAD (class template argument deduction)
constexpr auto two_phase_with_fixpoint = FixPoint{two_phase_core};

// Public interface: two_stage_simplify
constexpr auto two_stage_simplify = [](auto expr, auto ctx) {
  return two_phase_with_fixpoint.apply(expr, ctx);
};

// ============================================================================
// TRADITIONAL SINGLE-PHASE SIMPLIFICATION (For Comparison)
// ============================================================================
//
// The simplification pipeline must handle several challenges:
//
// 1. **Nested Expressions**: Rules must be applied recursively to
// subexpressions
//    Example: x * (y + (z * 0)) requires simplifying (z * 0) first
//
// 2. **Parent-Level Patterns**: Some rules match parent-level structures
//    Example: x*2 + x requires seeing the whole Add expression
//
// 3. **Multiple Passes**: Some simplifications enable others
//    Example: (x + x) + x → (2*x) + x → 3*x
//
// STRATEGY: Use bottomup (post-order) traversal with fixpoint iteration
//
// - bottomup: Apply rules to children first, then parent (handles nesting)
// - FixPoint: Repeat until no more changes (handles multi-pass requirements)
// - try_strategy: Wrap algebraic_simplify so it doesn't fail on leaves
//
// This ensures:
//   - Leaf nodes are simplified first (constants, identities)
//   - Parent nodes see already-simplified children
//   - Parent-level patterns can match
//   - Process repeats until stable

constexpr auto full_simplify = [](auto expr, auto ctx) {
  // Bottom-up traversal with fixpoint iteration
  //
  // bottomup applies algebraic_simplify at each node, starting from leaves.
  // This ensures nested expressions are fully simplified before parent rules
  // run.
  //
  // FixPoint repeats the process until the expression stops changing,
  // handling cases that require multiple passes like term collection.
  //
  // try_strategy wraps algebraic_simplify to return the original expression
  // when rules don't match (instead of Never), which is necessary for
  // traversal.
  return FixPoint{bottomup(try_strategy(algebraic_simplify))}.apply(expr, ctx);
};

// ============================================================================
// PRIMARY SIMPLIFICATION INTERFACE
// ============================================================================
//
// `simplify` is now an alias for `two_stage_simplify`, the new recommended
// simplification pipeline that provides:
//
// This provides:
//   - Short-circuit patterns (0*x → 0 without recursing)
//   - Two-phase traversal: descent (expand) then ascent (collect)
//   - Resolves distribution/factoring conflict
//   - More efficient than single-phase bottom-up
//   - Fixpoint iteration for exhaustive simplification
//
// MIGRATION NOTES:
//   - Previous `simplify` (aliased to full_simplify) is still available
//   - The two-stage approach is more efficient and handles edge cases better
//   - Old code using `simplify` will now get the improved two-stage behavior
//   - The old bounded-iteration version is available as `simplify_bounded`
//
// Usage:
//   auto result = simplify(expr, default_context());
constexpr auto simplify = two_stage_simplify;

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
//
// CRITICAL: Must wrap algebraic_simplify in try_strategy() because:
//   - algebraic_simplify returns Never when no rules match (e.g., on leaf
//   nodes)
//   - Traversal strategies call apply_to_children which builds Expression<Op,
//   ...>
//   - If a child is Never, this creates invalid types
//   - try_strategy converts Never back to the original expression
constexpr auto algebraic_simplify_recursive = [](auto expr, auto ctx) {
  return innermost(try_strategy(algebraic_simplify)).apply(expr, ctx);
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
//
// CRITICAL: Must wrap algebraic_simplify in try_strategy() for same reasons
// as algebraic_simplify_recursive above.
constexpr auto bottomup_simplify = [](auto expr, auto ctx) {
  return bottomup(try_strategy(algebraic_simplify)).apply(expr, ctx);
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
//
// CRITICAL: Must wrap algebraic_simplify in try_strategy() for same reasons
// as algebraic_simplify_recursive above.
constexpr auto topdown_simplify = [](auto expr, auto ctx) {
  return topdown(try_strategy(algebraic_simplify)).apply(expr, ctx);
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
