#pragma once

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/indexed/gather.h"
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
// DiffRecursive - Recursive with ReduceOver and Sample atom awareness
// ============================================================================
//
// Wraps the standard Recursive rule set but intercepts nodes that the
// pattern-based rules can't handle:
//
//   - ReduceOver: distributes diff through SumReduce, applies log-derivative
//     trick for ProdReduce, softmax weighting for LogSumExpReduce
//   - Sample/IndexedSample atoms: distribution factories embed parent RVs as
//     Atom<Id, Sample<D>> in the expression tree (via toSymbolic()). The
//     pattern rule `rule(Var{}, 1_c)` only matches exact types, so it can't
//     match a Sample atom when Var is a Free symbol. We intercept these and
//     compare by Id: same Id → 1, different → 0.
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
        // Linear: d/dx Σᵢ fᵢ = Σᵢ d/dx fᵢ
        auto inner_result = apply(expr.expr());
        return expr.rebuild(inner_result);

      } else if constexpr (isSame<ROp, ProdReduce>) {
        // Log-derivative trick:
        // d/dx Πᵢ fᵢ = (Πᵢ fᵢ) · Σᵢ (d/dx fᵢ / fᵢ)
        auto body = expr.expr();
        auto dbody = apply(body);
        auto ratio = dbody / body;
        return expr * ReduceOver<SumReduce, DimTag, decltype(ratio)>{ratio};

      } else if constexpr (isSame<ROp, LogSumExpReduce>) {
        // d/dx LSEᵢ(fᵢ) = Σᵢ softmax(fᵢ) · d/dx fᵢ
        // where softmax(fᵢ) = exp(fᵢ - LSE(f))
        auto body = expr.expr();
        auto dbody = apply(body);
        auto weights = exp(body - expr);  // softmax weights
        auto weighted = weights * dbody;
        return ReduceOver<SumReduce, DimTag, decltype(weighted)>{weighted};

      } else {
        // MaxReduce: not differentiable symbolically, return unchanged
        return expr;
      }
    } else if constexpr (is_gather_v<E>) {
      // Chain rule for Gather: d/dx[gather(a, idx)] = gather(d/dx[a], idx)
      // The index is data (constant w.r.t. parameters), so d/dx[idx] = 0.
      // We only need to differentiate the param and keep the same index.
      auto dparam = apply(expr.param);
      return Gather<decltype(dparam), typename E::index_type>{dparam, expr.index};
    } else if constexpr (is_random_var_atom_v<E>) {
      // Atom<Id, Sample<D>> from toSymbolic(rv) — match by Id against Var
      if constexpr (same_atom_id_v<E, Var>) {
        return Constant<1>{};
      } else {
        return Constant<0>{};
      }
    } else if constexpr (is_indexed_random_var_atom_v<E>) {
      // Atom<Id, IndexedSample<D, DimsList>> — match by Id against Var
      if constexpr (std::is_same_v<typename E::id_type, typename Var::id_type>) {
        return Constant<1>{};
      } else {
        return Constant<0>{};
      }
    } else {
      // Delegate to standard recursive rules (which handle Free atoms,
      // IndexedSymbols, expressions, constants, etc.)
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
constexpr auto diff(E expr, Var) {
  constexpr auto d = makeDiff<Var>();
  return simplify(d.apply(expr));
}

// ReduceOver support: DiffRecursive::apply() handles ReduceOver nodes by
// recursing into the inner expression and rebuilding.
// No standalone overload needed — d/dx(Σf) = Σ(d/dx f) is automatic.

// Long-form alias
template <Symbolic E, typename Var>
constexpr auto differentiate(E expr, Var v) {
  return diff(expr, v);
}

}  // namespace tempura::symbolic4
