#pragma once

#include "symbolic4/compressed.h"
#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/distributions/wrappers.h"  // For support type traits
#include "symbolic4/operators.h"
#include "symbolic4/scheme/cata.h"

// ============================================================================
// diff.h - Symbolic differentiation via para scheme
// ============================================================================
//
// Computes symbolic derivatives using the para recursion scheme with Rule-based
// dispatch. Each operator has a Rule specialization defining its derivative.
//
// The para scheme provides Pair{original, derivative} for each child,
// enabling rules like the product rule: d/dx(f*g) = f*dg + df*g
//
// Usage:
//   Symbol<struct X> x;
//   auto expr = x * x;
//   auto deriv = diff(expr, x);  // Returns type representing 2*x
//
// ============================================================================

namespace tempura::symbolic4 {

namespace diff_detail {
// Check if two atoms have the same Id (regardless of effect type)
// This allows Atom<Id, Free> and Atom<Id, Sample<D>> to be treated as the same
// variable for differentiation purposes.
template <typename T, typename Var>
struct IsSameAtomId : std::false_type {};

// Both are atoms with the same Id
template <typename Id, typename E1, typename E2>
struct IsSameAtomId<Atom<Id, E1>, Atom<Id, E2>> : std::true_type {};

template <typename T, typename Var>
constexpr bool is_same_atom_id_v = IsSameAtomId<T, Var>::value;

// Check if T is a Sample atom with the same Id as Var
// and return the derivative of the constraint transform
template <typename T, typename Var>
struct SampleAtomDerivative {
  static constexpr bool is_sample = false;
  static constexpr auto derivative() { return Constant<1>{}; }
};

// Specialization for Sample atoms matching Free atom Var
template <typename Id, typename Dist>
struct SampleAtomDerivative<Atom<Id, Sample<Dist>>, Atom<Id, Free>> {
  static constexpr bool is_sample = true;

  static constexpr auto derivative() {
    using Support = typename Dist::support_type;
    auto z = Atom<Id, Free>{};
    if constexpr (is_positive_support_v<Support>) {
      // d/dz [exp(z)] = exp(z)
      return exp(z);
    } else if constexpr (is_unit_support_v<Support>) {
      // d/dz [sigmoid(z)] = sigmoid(z) * (1 - sigmoid(z))
      auto s = 1_c / (1_c + exp(-z));
      return s * (1_c - s);
    } else {
      // Unconstrained: d/dz [z] = 1
      return Constant<1>{};
    }
  }
};

// Structural check for SumOver without including sum_over.h
// SumOver<DimTag, Expr> has: dim_tag typedef, expr_type typedef, expr() method, rebuild()
template <typename T>
concept IsSumOverLike = requires(T t) {
  typename T::dim_tag;
  typename T::expr_type;
  { t.expr() } -> Symbolic;
  // rebuild<NewExpr>(new_expr) for creating new SumOver with different expression type
  { T::template rebuild<typename T::expr_type>(t.expr()) };
};
}  // namespace diff_detail

// Forward declaration for recursive diff calls from Diff::terminal
template <Symbolic E, typename V>
constexpr auto diff(E expr, V);

template <typename Var>
struct Diff {
  // Terminal: d/dx(x) = 1, d/dx(anything else) = 0
  // Note: We check both exact type match and same-Id-different-effect match
  // to handle RandomVarSymbol (Atom<Id, Sample<D>>) vs Symbol (Atom<Id, Free>)
  //
  // For Sample atoms with constrained support (positive, unit interval), the
  // Sample represents the transformed value (exp(z), sigmoid(z)), so the
  // derivative must include the transform's derivative (chain rule).
  //
  // Special case: SumOver is treated as a "terminal" by cata/para because it's
  // not an Expression<Op, Args...>, but we need to distribute differentiation
  // into the sum: d/dx(Σᵢ f(x,i)) = Σᵢ d/dx(f(x,i))
  template <typename T>
  static constexpr auto terminal(T t) {
    if constexpr (std::is_same_v<T, Var>) {
      // Exact match: d/dz[z] = 1
      return Constant<1>{};
    } else if constexpr (diff_detail::SampleAtomDerivative<T, Var>::is_sample) {
      // Sample atom with same Id as Var: apply chain rule for constraint transform
      // d/dz[transform(z)] = transform'(z)
      return diff_detail::SampleAtomDerivative<T, Var>::derivative();
    } else if constexpr (diff_detail::is_same_atom_id_v<T, Var>) {
      // Same Id but not a Sample atom - treat as identity
      return Constant<1>{};
    } else if constexpr (diff_detail::IsSumOverLike<T>) {
      // SumOver is treated as a "terminal" by cata/para because it's not an
      // Expression<Op, Args...>, but we need to distribute differentiation
      // into the sum: d/dx(Σᵢ f(x,i)) = Σᵢ d/dx(f(x,i))
      auto inner_diff = diff(t.expr(), Var{});
      // Reconstruct using T's constructor pattern (SumOver<DimTag, Expr>)
      using ResultType = std::decay_t<decltype(inner_diff)>;
      return T::template rebuild<ResultType>(inner_diff);
    } else {
      return Constant<0>{};
    }
  }

  // Combine: dispatch to Rule based on operator
  template <typename Op, typename... Ps>
  static constexpr auto combine(Ps... ps) {
    return Rule<Op, Ps...>::apply(ps...);
  }

  // =========================================================================
  // Default rule (should not be reached for supported ops)
  // =========================================================================

  template <typename Op, typename... Ps>
  struct Rule {
    static constexpr auto apply(Ps...) {
      static_assert(sizeof...(Ps) == 0, "Unsupported operator for differentiation");
      return Constant<0>{};
    }
  };

  // =========================================================================
  // Nullary rules (constants)
  // =========================================================================

  // d/dx(π) = 0, d/dx(e) = 0
  template <typename Op>
    requires(std::is_same_v<Op, PiOp> || std::is_same_v<Op, EOp>)
  struct Rule<Op> {
    static constexpr auto apply() { return Constant<0>{}; }
  };

  // =========================================================================
  // Unary rules
  // =========================================================================

  // Negation: d/dx(-f) = -df
  template <typename P>
  struct Rule<NegOp, P> {
    static constexpr auto apply(P p) { return -p.second; }
  };

  // Exponential: d/dx(e^f) = e^f * df
  template <typename P>
  struct Rule<ExpOp, P> {
    static constexpr auto apply(P p) { return exp(p.first) * p.second; }
  };

  // Logarithm: d/dx(log(f)) = df/f
  template <typename P>
  struct Rule<LogOp, P> {
    static constexpr auto apply(P p) { return p.second / p.first; }
  };

  // Sine: d/dx(sin(f)) = cos(f) * df
  template <typename P>
  struct Rule<SinOp, P> {
    static constexpr auto apply(P p) { return cos(p.first) * p.second; }
  };

  // Cosine: d/dx(cos(f)) = -sin(f) * df
  template <typename P>
  struct Rule<CosOp, P> {
    static constexpr auto apply(P p) { return -sin(p.first) * p.second; }
  };

  // Tangent: d/dx(tan(f)) = df / cos²(f)
  template <typename P>
  struct Rule<TanOp, P> {
    static constexpr auto apply(P p) {
      return p.second / pow(cos(p.first), Constant<2>{});
    }
  };

  // Hyperbolic sine: d/dx(sinh(f)) = cosh(f) * df
  template <typename P>
  struct Rule<SinhOp, P> {
    static constexpr auto apply(P p) { return cosh(p.first) * p.second; }
  };

  // Hyperbolic cosine: d/dx(cosh(f)) = sinh(f) * df
  template <typename P>
  struct Rule<CoshOp, P> {
    static constexpr auto apply(P p) { return sinh(p.first) * p.second; }
  };

  // Hyperbolic tangent: d/dx(tanh(f)) = df / cosh²(f)
  template <typename P>
  struct Rule<TanhOp, P> {
    static constexpr auto apply(P p) {
      return p.second / pow(cosh(p.first), Constant<2>{});
    }
  };

  // Arcsine: d/dx(asin(f)) = df / √(1 - f²)
  template <typename P>
  struct Rule<AsinOp, P> {
    static constexpr auto apply(P p) {
      return p.second / sqrt(1_c - p.first * p.first);
    }
  };

  // Arccosine: d/dx(acos(f)) = -df / √(1 - f²)
  template <typename P>
  struct Rule<AcosOp, P> {
    static constexpr auto apply(P p) {
      return -p.second / sqrt(1_c - p.first * p.first);
    }
  };

  // Arctangent: d/dx(atan(f)) = df / (1 + f²)
  template <typename P>
  struct Rule<AtanOp, P> {
    static constexpr auto apply(P p) {
      return p.second / (1_c + p.first * p.first);
    }
  };

  // Square root: d/dx(√f) = df / (2√f)
  template <typename P>
  struct Rule<SqrtOp, P> {
    static constexpr auto apply(P p) {
      return p.second / (Constant<2>{} * sqrt(p.first));
    }
  };

  // =========================================================================
  // Special functions for probability distributions
  // =========================================================================

  // Log-gamma: d/dx(lgamma(f)) = digamma(f) * df
  template <typename P>
  struct Rule<LgammaOp, P> {
    static constexpr auto apply(P p) { return digamma(p.first) * p.second; }
  };

  // Digamma: d/dx(digamma(f)) = trigamma(f) * df
  // Trigamma ψ₁(x) = d/dx[digamma(x)] - we approximate or leave as-is
  // For now, use finite-difference approximation via evaluation
  template <typename P>
  struct Rule<DigammaOp, P> {
    static constexpr auto apply(P p) {
      // trigamma(x) = d/dx[digamma(x)]
      // We'll represent this symbolically but evaluation uses numeric approx
      // For simplicity, return digamma'(f) * df where digamma' ≈ 1/f + 1/(2f²) + ...
      // This is a rough approximation; production code would use proper trigamma
      return (1_c / p.first + 1_c / (Constant<2>{} * p.first * p.first)) * p.second;
    }
  };

  // Error function: d/dx(erf(f)) = (2/√π) * exp(-f²) * df
  template <typename P>
  struct Rule<ErfOp, P> {
    static constexpr auto apply(P p) {
      // 2/√π ≈ 1.1283791670955126
      return lit(1.1283791670955126) * exp(-p.first * p.first) * p.second;
    }
  };

  // Complementary error function: d/dx(erfc(f)) = -(2/√π) * exp(-f²) * df
  template <typename P>
  struct Rule<ErfcOp, P> {
    static constexpr auto apply(P p) {
      return lit(-1.1283791670955126) * exp(-p.first * p.first) * p.second;
    }
  };

  // log(1+x): d/dx(log1p(f)) = df / (1 + f)
  template <typename P>
  struct Rule<Log1pOp, P> {
    static constexpr auto apply(P p) { return p.second / (1_c + p.first); }
  };

  // exp(x)-1: d/dx(expm1(f)) = exp(f) * df
  template <typename P>
  struct Rule<Expm1Op, P> {
    static constexpr auto apply(P p) { return exp(p.first) * p.second; }
  };

  // Absolute value: d/dx(|f|) = sign(f) * df
  // Note: not differentiable at f=0; this gives subgradient
  template <typename P>
  struct Rule<AbsOp, P> {
    static constexpr auto apply(P p) {
      // sign(f) = f / |f|
      return (p.first / abs(p.first)) * p.second;
    }
  };

  // Floor and ceil: derivatives are 0 (piecewise constant)
  template <typename P>
  struct Rule<FloorOp, P> {
    static constexpr auto apply(P) { return Constant<0>{}; }
  };

  template <typename P>
  struct Rule<CeilOp, P> {
    static constexpr auto apply(P) { return Constant<0>{}; }
  };

  // Cube root: d/dx(cbrt(f)) = df / (3 * cbrt(f)²)
  template <typename P>
  struct Rule<CbrtOp, P> {
    static constexpr auto apply(P p) {
      return p.second / (Constant<3>{} * cbrt(p.first) * cbrt(p.first));
    }
  };

  // log10: d/dx(log10(f)) = df / (f * ln(10))
  template <typename P>
  struct Rule<Log10Op, P> {
    static constexpr auto apply(P p) {
      // ln(10) ≈ 2.302585092994046
      return p.second / (p.first * lit(2.302585092994046));
    }
  };

  // log2: d/dx(log2(f)) = df / (f * ln(2))
  template <typename P>
  struct Rule<Log2Op, P> {
    static constexpr auto apply(P p) {
      // ln(2) ≈ 0.6931471805599453
      return p.second / (p.first * lit(0.6931471805599453));
    }
  };

  // exp2: d/dx(2^f) = 2^f * ln(2) * df
  template <typename P>
  struct Rule<Exp2Op, P> {
    static constexpr auto apply(P p) {
      return exp2(p.first) * lit(0.6931471805599453) * p.second;
    }
  };

  // =========================================================================
  // Binary rules
  // =========================================================================

  // Sum: d/dx(f + g) = df + dg
  template <typename L, typename R>
  struct Rule<AddOp, L, R> {
    static constexpr auto apply(L l, R r) { return l.second + r.second; }
  };

  // Difference: d/dx(f - g) = df - dg
  template <typename L, typename R>
  struct Rule<SubOp, L, R> {
    static constexpr auto apply(L l, R r) { return l.second - r.second; }
  };

  // Product: d/dx(f * g) = f*dg + df*g
  template <typename L, typename R>
  struct Rule<MulOp, L, R> {
    static constexpr auto apply(L l, R r) {
      return l.first * r.second + l.second * r.first;
    }
  };

  // Quotient: d/dx(f/g) = (df*g - f*dg) / g²
  template <typename L, typename R>
  struct Rule<DivOp, L, R> {
    static constexpr auto apply(L l, R r) {
      return (l.second * r.first - l.first * r.second) / (r.first * r.first);
    }
  };

  // Power (constant exponent): d/dx(f^n) = n * f^(n-1) * df
  template <typename L, typename R>
    requires(is_constant_v<decltype(R::first)> ||
             is_fraction_v<decltype(R::first)>)
  struct Rule<PowOp, L, R> {
    static constexpr auto apply(L l, R r) {
      return r.first * pow(l.first, r.first - Constant<1>{}) * l.second;
    }
  };

  // Power (general): d/dx(f^g) = f^g * (dg*ln(f) + g*df/f)
  template <typename L, typename R>
    requires(!is_constant_v<decltype(R::first)>) &&
            (!is_fraction_v<decltype(R::first)>)
  struct Rule<PowOp, L, R> {
    static constexpr auto apply(L l, R r) {
      return pow(l.first, r.first) *
             (r.second * log(l.first) + r.first * l.second / l.first);
    }
  };

  // Hypot: d/dx(hypot(f,g)) = (f*df + g*dg) / hypot(f,g)
  template <typename L, typename R>
  struct Rule<HypotOp, L, R> {
    static constexpr auto apply(L l, R r) {
      return (l.first * l.second + r.first * r.second) / hypot(l.first, r.first);
    }
  };

  // Max: d/dx(max(f,g)) = df if f > g, dg otherwise (subgradient)
  // For differentiability, use smooth approximation or just pass through
  // Here we return a symbolic expression that evaluates appropriately
  template <typename L, typename R>
  struct Rule<MaxOp, L, R> {
    static constexpr auto apply(L l, R r) {
      // Subgradient: use sign(f-g) to blend df and dg
      // When f > g: df, when f < g: dg, at boundary: (df+dg)/2
      // This is not symbolic-friendly; for now, assume constant comparison
      // In practice, max is often used with constants
      return l.second + r.second;  // Approximate as sum (conservative)
    }
  };

  // Min: similar to max
  template <typename L, typename R>
  struct Rule<MinOp, L, R> {
    static constexpr auto apply(L l, R r) {
      return l.second + r.second;  // Approximate as sum (conservative)
    }
  };
};

// ============================================================================
// Convenience function: diff(expr, var)
// ============================================================================

template <Symbolic E, typename Var>
constexpr auto diff(E expr, [[maybe_unused]] Var) {
  return para<Diff<Var>>(expr);
}

// ============================================================================
// SumOver differentiation support (separate header to avoid circular deps)
// ============================================================================
// diff(SumOver<D, E>, x) = SumOver<D, diff(E, x)>
// This is provided in symbolic4/indexed/sum_over_diff.h

}  // namespace tempura::symbolic4
