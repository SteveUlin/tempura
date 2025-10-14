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
// TWO-STAGE SIMPLIFICATION ARCHITECTURE
// ============================================================================
//
// This file implements algebraic simplification using a two-stage pipeline:
//
// 1. **Descent Phase** (pre-order): Quick patterns and unwrapping
//    - Short-circuit patterns: 0*x → 0 (before recursing into x)
//    - Unwrapping: -(-x) → x
//
// 2. **Ascent Phase** (post-order): Collection and canonicalization
//    - Constant folding: 2+3 → 5
//    - Term collection: x+x → 2*x, x*a+x*b → x*(a+b)
//    - Canonical ordering: y+x → x+y (when x<y)
//    - Power combining: x*x^2 → x^3
//
// The pipeline uses fixpoint iteration to repeat until stable.
//
// KEY DESIGN DECISIONS:
// - Rules use predicates for directional application (prevents oscillation)
// - Term-structure-aware ordering groups like terms
// - Distribution is disabled to avoid conflict with factoring
// - Fractions maintain exact arithmetic (5/2 → Fraction<5,2>, not 2.5)

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

namespace AdditionRuleCategories {
constexpr auto Identity = Rewrite{0_c + x_, x_} | Rewrite{x_ + 0_c, x_};
constexpr auto LikeTerms = Rewrite{x_ + x_, x_ * 2_c};

constexpr auto Ordering = Rewrite{
    y_ + x_, x_ + y_, [](auto ctx) {
      return compareAdditionTerms(get(ctx, x_), get(ctx, y_)) == Ordering::Less;
    }};

constexpr auto Factoring = Rewrite{x_ * a_ + x_, x_*(a_ + 1_c)} |
                           Rewrite{x_ + x_ * a_, x_ * (1_c + a_)} |
                           Rewrite{x_ * a_ + x_ * b_, x_*(a_ + b_)} |
                           Rewrite{a_ * x_ + x_, x_*(a_ + 1_c)} |
                           Rewrite{x_ + a_ * x_, x_ * (1_c + a_)} |
                           Rewrite{a_ * x_ + b_ * x_, x_*(a_ + b_)};

constexpr auto Associativity =
    Rewrite{a_ + (b_ + c_), (a_ + b_) + c_,
            [](auto ctx) {
              return compareAdditionTerms(get(ctx, b_), get(ctx, a_)) !=
                     Ordering::Less;
            }} |
    Rewrite{(a_ + c_) + b_, a_ + (c_ + b_),
            [](auto ctx) {
              return compareAdditionTerms(get(ctx, b_), get(ctx, c_)) ==
                     Ordering::Less;
            }} |
    Rewrite{a_ + (c_ + b_), a_ + (b_ + c_), [](auto ctx) {
              return compareAdditionTerms(get(ctx, b_), get(ctx, c_)) ==
                     Ordering::Less;
            }};
}  // namespace AdditionRuleCategories

constexpr auto AdditionRules =
    AdditionRuleCategories::Identity | AdditionRuleCategories::LikeTerms |
    AdditionRuleCategories::Ordering | AdditionRuleCategories::Factoring |
    AdditionRuleCategories::Associativity;

// ============================================================================
// Multiplication Simplification Rules
// ============================================================================

namespace MultiplicationRuleCategories {
constexpr auto Identity = Rewrite{0_c * x_, 0_c} | Rewrite{x_ * 0_c, 0_c} |
                          Rewrite{1_c * x_, x_} | Rewrite{x_ * 1_c, x_};

constexpr auto Ordering =
    Rewrite{y_ * x_, x_* y_, [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, x_), get(ctx, y_)) ==
                     Ordering::Less;
            }};

constexpr auto PowerCombining =
    Rewrite{x_ * pow(x_, a_), pow(x_, a_ + 1_c)} |
    Rewrite{pow(x_, a_) * x_, pow(x_, a_ + 1_c)} |
    Rewrite{pow(x_, a_) * pow(x_, b_), pow(x_, a_ + b_)};

constexpr auto Associativity =
    Rewrite{a_ * (b_ * c_), (a_ * b_) * c_,
            [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, b_), get(ctx, a_)) !=
                     Ordering::Less;
            }} |
    Rewrite{(a_ * c_) * b_, a_*(c_* b_),
            [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, b_), get(ctx, c_)) ==
                     Ordering::Less;
            }} |
    Rewrite{a_ * (c_ * b_), a_*(b_* c_), [](auto ctx) {
              return compareMultiplicationTerms(get(ctx, b_), get(ctx, c_)) ==
                     Ordering::Less;
            }};
}  // namespace MultiplicationRuleCategories

constexpr auto MultiplicationRules =
    MultiplicationRuleCategories::Identity |
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

constexpr auto transcendental_simplify =
    ExpRules | LogRules | SinRules | CosRules | TanRules | SinhRules |
    CoshRules | TanhRules | SqrtRules | PythagoreanRules |
    HyperbolicIdentityRules;

// Combined algebraic rules (for building custom pipelines)
constexpr auto algebraic_simplify =
    promote_division_to_fraction | constant_fold | PowerRules | AdditionRules |
    MultiplicationRules | FractionRules | transcendental_simplify;
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

constexpr auto descent_with_quick = quick_patterns | descent_rules;
constexpr auto descent_phase = topdown(descent_with_quick);
constexpr auto ascent_phase = bottomup(ascent_rules);
constexpr auto two_phase_core = descent_phase >> ascent_phase;
constexpr auto two_phase_with_fixpoint = FixPoint{two_phase_core};

// ============================================================================
// PRIMARY SIMPLIFICATION INTERFACE
// ============================================================================
//
// Usage: auto result = simplify(expr, default_context());
//
// Architecture:
//   1. Descent phase: Quick patterns (0*x→0) and unwrapping before recursing
//   2. Ascent phase: Collection, canonicalization after children simplified
//   3. Fixpoint iteration: Repeat until stable
//
// Handles nested expressions, term collection, and canonical forms.
constexpr auto simplify = [](auto expr, auto ctx) {
  return two_phase_with_fixpoint.apply(expr, ctx);
};

}  // namespace tempura::symbolic3
