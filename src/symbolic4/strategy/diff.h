#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/interpreter/simplify.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/recursive.h"

// ============================================================================
// diff.h - Symbolic differentiation via recursive rewrite rules
// ============================================================================
//
// Differentiation rules read like their mathematical definitions:
//
//   rrule(sin(f_), cos(f_) * rec(f_))        // d/dx[sin(f)] = cos(f) · f'
//   rrule(f_ * g_, rec(f_) * g_ + f_ * rec(g_))  // product rule
//
// The rec(f_) means "recursively differentiate whatever f_ matched".
// Use rrule() for rules with rec(), rule() for base cases without rec().
//
// Usage:
//   Symbol<struct X> x;
//   auto result = differentiate(x * sin(x), x);
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Sample atom handling for differentiation
// ============================================================================
//
// Expressions built from RandomVars contain Atom<Id, Sample<Dist>> nodes.
// These represent the *constrained* value of the random variable.
// When Var is Atom<Id, Free> and we see Atom<Id, Sample<D>>:
//   - Same Id: derivative is the constraint transform's derivative
//     (1 for real support, exp(z) for positive, sigmoid'(z) for unit)
//   - Different Id: derivative is 0 (constant w.r.t. Var)
//
// ============================================================================

namespace diff_detail {

// Check if two atoms share the same Id regardless of effect type
template <typename T, typename Var>
struct IsSameAtomId : std::false_type {};

template <typename Id, typename E1, typename E2>
struct IsSameAtomId<Atom<Id, E1>, Atom<Id, E2>> : std::true_type {};

template <typename T, typename Var>
constexpr bool is_same_atom_id_v = IsSameAtomId<T, Var>::value;

// Compute the derivative of a Sample atom w.r.t. the matching Free variable.
// For constrained distributions, this is the derivative of the constraint transform.
template <typename T, typename Var>
struct SampleDerivative {
  static constexpr auto compute() { return Constant<0>{}; }
};

// Atom<Id, Sample<D>> differentiated w.r.t. Atom<Id, Free>:
// The Sample atom represents constrainedExpr(z) where z = Atom<Id, Free>.
// d/dz[constrainedExpr(z)] depends on the distribution's support.
template <typename Id, typename Dist>
struct SampleDerivative<Atom<Id, Sample<Dist>>, Atom<Id, Free>> {
  static constexpr auto compute() {
    using Support = typename Dist::support_type;
    auto z = Atom<Id, Free>{};
    if constexpr (is_positive_support_v<Support>) {
      return exp(z);  // d/dz[exp(z)] = exp(z)
    } else if constexpr (is_unit_support_v<Support>) {
      auto s = 1_c / (1_c + exp(-z));
      return s * (1_c - s);  // d/dz[sigmoid(z)] = sigmoid(z)(1-sigmoid(z))
    } else {
      return Constant<1>{};  // d/dz[z] = 1 (unconstrained)
    }
  }
};

// Check if T is a Sample atom
template <typename T>
struct IsSampleAtom : std::false_type {};

template <typename Id, typename D>
struct IsSampleAtom<Atom<Id, Sample<D>>> : std::true_type {};

template <typename T>
constexpr bool is_sample_atom_v = IsSampleAtom<T>::value;

}  // namespace diff_detail

// ============================================================================
// DiffRecursive - Recursive with Sample atom awareness
// ============================================================================
//
// Wraps the standard Recursive rule set but intercepts Sample atoms before
// the pattern-based rules. This keeps Sample-specific logic out of the
// general strategy framework.
//
// ============================================================================

template <typename Rules, typename Var>
struct DiffRecursive {
  [[no_unique_address]] Recursive<Rules> inner;

  constexpr explicit DiffRecursive(Rules r) : inner{r} {}

  template <Symbolic E>
  constexpr auto apply(E expr) const {
    if constexpr (is_reduce_over_v<E>) {
      using ROp = typename E::reduce_op;
      using DimTag = typename E::dim_tag;

      if constexpr (isSame<ROp, SumReduce>) {
        // Linear: d/dx Sigma_i f_i = Sigma_i d/dx f_i
        auto inner_result = apply(expr.expr());
        return expr.rebuild(inner_result);

      } else if constexpr (isSame<ROp, ProdReduce>) {
        // Log-derivative trick:
        // d/dx Prod_i f_i = (Prod_i f_i) * Sigma_i (d/dx f_i / f_i)
        auto body = expr.expr();
        auto dbody = apply(body);
        auto ratio = dbody / body;
        return expr * ReduceOver<SumReduce, DimTag, decltype(ratio)>{ratio};

      } else if constexpr (isSame<ROp, LogSumExpReduce>) {
        // d/dx LSE_i(f_i) = Sigma_i softmax(f_i) * d/dx f_i
        // where softmax(f_i) = exp(f_i - LSE(f)) = exp(f_i) / Sigma exp(f_i)
        auto body = expr.expr();
        auto dbody = apply(body);
        auto weights = exp(body - expr);  // softmax weights
        auto weighted = weights * dbody;
        return ReduceOver<SumReduce, DimTag, decltype(weighted)>{weighted};

      } else {
        // MaxReduce: not differentiable symbolically, return unchanged
        return expr;
      }
    } else if constexpr (diff_detail::is_sample_atom_v<E>) {
      // Sample atom: compute derivative based on Id match with Var
      if constexpr (diff_detail::is_same_atom_id_v<E, Var>) {
        return diff_detail::SampleDerivative<E, Var>::compute();
      } else {
        return Constant<0>{};  // Different Id: constant w.r.t. Var
      }
    } else {
      // Delegate to standard recursive rules (which handle normal atoms,
      // expressions, constants, etc.)
      auto result = detail::applyRecursiveRule(inner.rules, expr, *this);
      if constexpr (isSame<decltype(result), Never>) {
        return expr;  // No rule matched, return unchanged
      } else {
        return result;
      }
    }
  }
};

// ============================================================================
// makeDiff - Creates a differentiation strategy for a given variable
// ============================================================================

template <typename Var>
constexpr auto makeDiff() {
  // Pattern variables for expressions
  constexpr auto f_ = PatternVar<struct DiffF_>{};
  constexpr auto g_ = PatternVar<struct DiffG_>{};

  auto rules =
      // -----------------------------------------------------------------------
      // Binary operations
      // -----------------------------------------------------------------------
      rrule(f_ + g_, rec(f_) + rec(g_))
    | rrule(f_ - g_, rec(f_) - rec(g_))
    | rrule(f_ * g_, rec(f_) * g_ + f_ * rec(g_))
    | rrule(f_ / g_, (rec(f_) * g_ - f_ * rec(g_)) / (g_ * g_))
    | rrule(pow(f_, g_), pow(f_, g_) * (rec(g_) * log(f_) + g_ * rec(f_) / f_))
    | rrule(hypot(f_, g_), (f_ * rec(f_) + g_ * rec(g_)) / hypot(f_, g_))

      // -----------------------------------------------------------------------
      // Trig functions (chain rule built-in via rec)
      // -----------------------------------------------------------------------
    | rrule(sin(f_), cos(f_) * rec(f_))
    | rrule(cos(f_), -sin(f_) * rec(f_))
    | rrule(tan(f_), rec(f_) / pow(cos(f_), 2_c))

      // -----------------------------------------------------------------------
      // Inverse trig
      // -----------------------------------------------------------------------
    | rrule(asin(f_), rec(f_) / sqrt(1_c - f_ * f_))
    | rrule(acos(f_), -rec(f_) / sqrt(1_c - f_ * f_))
    | rrule(atan(f_), rec(f_) / (1_c + f_ * f_))

      // -----------------------------------------------------------------------
      // Hyperbolic
      // -----------------------------------------------------------------------
    | rrule(sinh(f_), cosh(f_) * rec(f_))
    | rrule(cosh(f_), sinh(f_) * rec(f_))
    | rrule(tanh(f_), rec(f_) / pow(cosh(f_), 2_c))

      // -----------------------------------------------------------------------
      // Exp/Log
      // -----------------------------------------------------------------------
    | rrule(exp(f_), exp(f_) * rec(f_))
    | rrule(log(f_), rec(f_) / f_)
    | rrule(sqrt(f_), rec(f_) / (2_c * sqrt(f_)))
    | rrule(log1p(f_), rec(f_) / (1_c + f_))
    | rrule(expm1(f_), exp(f_) * rec(f_))
    | rrule(log10(f_), rec(f_) / (f_ * log(10_c)))
    | rrule(log2(f_), rec(f_) / (f_ * log(2_c)))
    | rrule(exp2(f_), exp2(f_) * log(2_c) * rec(f_))
    | rrule(cbrt(f_), rec(f_) / (3_c * cbrt(f_) * cbrt(f_)))

      // -----------------------------------------------------------------------
      // Special functions
      // -----------------------------------------------------------------------
    | rrule(lgamma(f_), digamma(f_) * rec(f_))
    | rrule(erf(f_), 2_c / sqrt(pi) * exp(-f_ * f_) * rec(f_))
    | rrule(erfc(f_), -2_c / sqrt(pi) * exp(-f_ * f_) * rec(f_))

      // -----------------------------------------------------------------------
      // Misc unary
      // -----------------------------------------------------------------------
    | rrule(-f_, -rec(f_))
    | rrule(abs(f_), (f_ / abs(f_)) * rec(f_))
    | rule(floor(f_), 0_c)  // Not differentiable, but return 0
    | rule(ceil(f_), 0_c)

      // -----------------------------------------------------------------------
      // Nullary constants
      // -----------------------------------------------------------------------
    | rule(pi, 0_c)
    | rule(e, 0_c)

      // -----------------------------------------------------------------------
      // Terminal rules (order matters: specific before general)
      // -----------------------------------------------------------------------
    | rule(Var{}, 1_c)           // d/dx[x] = 1
    | rule(AnySymbol{}, 0_c)     // d/dx[y] = 0 (other symbols)
    | rule(AnyConstant{}, 0_c)   // d/dx[c] = 0 (includes Fraction)
    | rule(AnyFraction{}, 0_c)   // d/dx[n/d] = 0 (explicit for clarity)
    | rule(AnyLiteral{}, 0_c);   // d/dx[lit] = 0

  return DiffRecursive<decltype(rules), Var>{rules};
}

// ============================================================================
// Main API
// ============================================================================

template <Symbolic E, typename Var>
constexpr auto differentiate(E expr, Var) {
  constexpr auto d = makeDiff<Var>();
  return simplify(d.apply(expr));
}

// ReduceOver support: DiffRecursive::apply() handles ReduceOver nodes by
// recursing into the inner expression and rebuilding.
// No standalone overload needed — d/dx(Σf) = Σ(d/dx f) is automatic.

// Backward-compatible alias: call sites use diff(expr, var) unchanged
template <Symbolic E, typename Var>
constexpr auto diff(E expr, Var v) {
  return differentiate(expr, v);
}

}  // namespace tempura::symbolic4
