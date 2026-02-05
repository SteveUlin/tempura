#pragma once

#include <experimental/meta>
#include <vector>

#include "symbolic4/core.h"
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/info_simplify.h"

// ============================================================================
// info_diff.h — Info-domain symbolic differentiation
// ============================================================================
//
// The type-domain differentiator (diff.h) creates ~200+ intermediate Expression
// types per differentiation. This module does the same work on std::meta::info
// values — zero intermediate types until the final result is spliced.
//
// Key function:
//   diffInfo(expr, var)  → info (differentiated + simplified)
//
// Dispatch on template_of(expr):
//   Expression  → operator rules (sum, product, chain, quotient, power, etc.)
//   Atom        → effect dispatch (Free → Id compare, Sample → Id compare)
//   Constant    → 0
//   Fraction    → 0
//   ReduceOver  → reduce-op dispatch (Sum linearity, Prod log-derivative, LSE softmax)
//   Gather      → chain rule through index (d/dx[gather(a, idx)] = gather(d/dx a, idx))
//
// The operator dispatch mirrors diff.h's rule set exactly, with chain rule
// embedded via recursive calls to diffInfoImpl.
//
// ============================================================================

namespace tempura::symbolic4 {

namespace info_diff_detail {

// Cached operator reflections — avoids repeated ^^::tempura::Op lookups.
// Using-declarations in symbolic4 namespace can't be reflected directly
// (clang-p2996 limitation), so we reflect from ::tempura.
struct Ops {
  static consteval auto add() { return ^^::tempura::AddOp; }
  static consteval auto sub() { return ^^::tempura::SubOp; }
  static consteval auto mul() { return ^^::tempura::MulOp; }
  static consteval auto div_() { return ^^::tempura::DivOp; }
  static consteval auto pow_() { return ^^::tempura::PowOp; }
  static consteval auto neg() { return ^^::tempura::NegOp; }
  static consteval auto sin_() { return ^^::tempura::SinOp; }
  static consteval auto cos_() { return ^^::tempura::CosOp; }
  static consteval auto tan_() { return ^^::tempura::TanOp; }
  static consteval auto asin_() { return ^^::tempura::AsinOp; }
  static consteval auto acos_() { return ^^::tempura::AcosOp; }
  static consteval auto atan_() { return ^^::tempura::AtanOp; }
  static consteval auto sinh_() { return ^^::tempura::SinhOp; }
  static consteval auto cosh_() { return ^^::tempura::CoshOp; }
  static consteval auto tanh_() { return ^^::tempura::TanhOp; }
  static consteval auto exp_() { return ^^::tempura::ExpOp; }
  static consteval auto log_() { return ^^::tempura::LogOp; }
  static consteval auto sqrt_() { return ^^::tempura::SqrtOp; }
  static consteval auto log1p_() { return ^^::tempura::Log1pOp; }
  static consteval auto expm1_() { return ^^::tempura::Expm1Op; }
  static consteval auto log10_() { return ^^::tempura::Log10Op; }
  static consteval auto log2_() { return ^^::tempura::Log2Op; }
  static consteval auto exp2_() { return ^^::tempura::Exp2Op; }
  static consteval auto cbrt_() { return ^^::tempura::CbrtOp; }
  static consteval auto abs_() { return ^^::tempura::AbsOp; }
  static consteval auto lgamma_() { return ^^::tempura::LgammaOp; }
  static consteval auto digamma_() { return ^^::tempura::DigammaOp; }
  static consteval auto erf_() { return ^^::tempura::ErfOp; }
  static consteval auto erfc_() { return ^^::tempura::ErfcOp; }
  static consteval auto hypot_() { return ^^::tempura::HypotOp; }
  static consteval auto floor_() { return ^^::tempura::FloorOp; }
  static consteval auto ceil_() { return ^^::tempura::CeilOp; }
  static consteval auto pi_() { return ^^::tempura::PiOp; }
  static consteval auto e_() { return ^^::tempura::EOp; }
};

// Check if an info represents an Atom with a specific effect template.
consteval auto hasEffectTemplate(std::meta::info atom_info, std::meta::info effect_tmpl) -> bool {
  auto args = std::meta::template_arguments_of(atom_info);
  auto effect = args[1];  // Atom<Id, Effect> → Effect is arg[1]
  if (!std::meta::has_template_arguments(effect)) return false;
  return std::meta::template_of(effect) == effect_tmpl;
}

// Extract the Id (arg[0]) from an Atom info.
consteval auto atomId(std::meta::info atom_info) -> std::meta::info {
  return std::meta::template_arguments_of(atom_info)[0];
}

// Normalizing builders — compose raw construction with simplifyNode so the
// expression tree stays small during differentiation. Opt-in alternative to
// the raw binary()/unary() constructors in info_detail.
consteval auto nBin(std::meta::info op, std::meta::info lhs, std::meta::info rhs)
    -> std::meta::info {
  return simplifyNode(info_detail::binary(op, lhs, rhs));
}

consteval auto nUn(std::meta::info op, std::meta::info arg) -> std::meta::info {
  return simplifyNode(info_detail::unary(op, arg));
}

}  // namespace info_diff_detail

// ============================================================================
// diffInfoImpl — Core recursive differentiation on info values
// ============================================================================

consteval auto diffInfoImpl(std::meta::info expr, std::meta::info var) -> std::meta::info {
  using namespace info_diff_detail;

  auto zero = ^^Constant<0.0>;
  auto one = ^^Constant<1.0>;
  auto two = ^^Constant<2.0>;

  // ----- Constant / Fraction: derivative is zero -----
  if (!std::meta::has_template_arguments(expr)) {
    // Bare operator or non-template type — shouldn't appear as top-level expr
    return zero;
  }

  auto tmpl = std::meta::template_of(expr);

  if (tmpl == ^^Constant) return zero;
  if (tmpl == ^^Fraction) return zero;

  // ----- Atom<Id, Effect> -----
  if (tmpl == ^^Atom) {
    auto id = atomId(expr);
    auto var_id = atomId(var);

    // Free effect: d/dx[x] = 1, d/dx[y] = 0
    if (hasEffectTemplate(expr, ^^Free)) {
      // Free is not a template — it's a plain struct. Check equality differently.
      // Actually Free is just a struct, so effect == ^^Free for Free atoms.
      // But the Atom<Id, Free> has effect = Free (not a template specialization).
    }

    // Match by Id regardless of effect (handles Free, Sample, IndexedSample)
    if (id == var_id) return one;
    return zero;
  }

  // ----- IndexedSymbol<Id, Dims...> -----
  if (tmpl == ^^IndexedSymbol) {
    auto args = std::meta::template_arguments_of(expr);
    auto id = args[0];
    auto var_args = std::meta::template_arguments_of(var);
    auto var_id = var_args[0];
    if (id == var_id) return one;
    return zero;
  }

  // ----- ReduceOver<ReduceOp, DimTag, Body> -----
  if (tmpl == ^^ReduceOver) {
    auto args = std::meta::template_arguments_of(expr);
    auto reduce_op = args[0];
    auto dim_tag = args[1];
    auto body = args[2];

    if (reduce_op == ^^SumReduce) {
      // Linear: d/dx Σ f = Σ d/dx f
      auto dbody = diffInfoImpl(body, var);
      return std::meta::substitute(^^ReduceOver, {reduce_op, dim_tag, dbody});

    } else if (reduce_op == ^^ProdReduce) {
      // Log-derivative trick: d/dx Π fᵢ = (Π fᵢ) · Σ (d/dx fᵢ / fᵢ)
      auto dbody = diffInfoImpl(body, var);
      auto ratio = nBin(Ops::div_(), dbody, body);
      auto sum_ratio = std::meta::substitute(^^ReduceOver,
          {^^SumReduce, dim_tag, ratio});
      return nBin(Ops::mul(), expr, sum_ratio);

    } else if (reduce_op == ^^LogSumExpReduce) {
      // d/dx LSE(f) = Σ softmax(f) · d/dx f
      // softmax(f) = exp(f - LSE(f))
      auto dbody = diffInfoImpl(body, var);
      auto weights = nUn(Ops::exp_(), nBin(Ops::sub(), body, expr));
      auto weighted = nBin(Ops::mul(), weights, dbody);
      return std::meta::substitute(^^ReduceOver,
          {^^SumReduce, dim_tag, weighted});

    } else {
      // MaxReduce: not differentiable, return unchanged
      return expr;
    }
  }

  // ----- Gather<Param, Index> -----
  if (tmpl == ^^Gather) {
    auto args = std::meta::template_arguments_of(expr);
    auto param = args[0];
    auto index = args[1];
    // Chain rule: d/dx[gather(a, idx)] = gather(d/dx[a], idx)
    auto dparam = diffInfoImpl(param, var);
    return std::meta::substitute(^^Gather, {dparam, index});
  }

  // ----- Expression<Op, Args...> -----
  if (tmpl == ^^Expression) {
    auto args = std::meta::template_arguments_of(expr);
    auto op = args[0];
    auto arity = args.size() - 1;  // args[0] is Op, rest are children

    // --- Nullary constants ---
    if (arity == 0) {
      // pi, e → 0
      return zero;
    }

    // --- Unary operations ---
    if (arity == 1) {
      auto f = args[1];
      auto df = diffInfoImpl(f, var);

      // Negation: d/dx[-f] = -f'
      if (op == Ops::neg())
        return nUn(Ops::neg(), df);

      // Trig
      if (op == Ops::sin_())
        return nBin(Ops::mul(), nUn(Ops::cos_(), f), df);
      if (op == Ops::cos_())
        return nBin(Ops::mul(), nUn(Ops::neg(), nUn(Ops::sin_(), f)), df);
      if (op == Ops::tan_())
        return nBin(Ops::div_(), df, nBin(Ops::pow_(), nUn(Ops::cos_(), f), two));

      // Inverse trig
      if (op == Ops::asin_())
        return nBin(Ops::div_(), df,
            nUn(Ops::sqrt_(), nBin(Ops::sub(), one, nBin(Ops::mul(), f, f))));
      if (op == Ops::acos_())
        return nBin(Ops::div_(), nUn(Ops::neg(), df),
            nUn(Ops::sqrt_(), nBin(Ops::sub(), one, nBin(Ops::mul(), f, f))));
      if (op == Ops::atan_())
        return nBin(Ops::div_(), df,
            nBin(Ops::add(), one, nBin(Ops::mul(), f, f)));

      // Hyperbolic
      if (op == Ops::sinh_())
        return nBin(Ops::mul(), nUn(Ops::cosh_(), f), df);
      if (op == Ops::cosh_())
        return nBin(Ops::mul(), nUn(Ops::sinh_(), f), df);
      if (op == Ops::tanh_())
        return nBin(Ops::div_(), df,
            nBin(Ops::pow_(), nUn(Ops::cosh_(), f), two));

      // Exp/Log
      if (op == Ops::exp_())
        return nBin(Ops::mul(), nUn(Ops::exp_(), f), df);
      if (op == Ops::log_())
        return nBin(Ops::div_(), df, f);
      if (op == Ops::sqrt_())
        return nBin(Ops::div_(), df,
            nBin(Ops::mul(), two, nUn(Ops::sqrt_(), f)));
      if (op == Ops::log1p_())
        return nBin(Ops::div_(), df, nBin(Ops::add(), one, f));
      if (op == Ops::expm1_())
        return nBin(Ops::mul(), nUn(Ops::exp_(), f), df);
      if (op == Ops::log10_())
        return nBin(Ops::div_(), df,
            nBin(Ops::mul(), f, nUn(Ops::log_(), ^^Constant<10.0>)));
      if (op == Ops::log2_())
        return nBin(Ops::div_(), df,
            nBin(Ops::mul(), f, nUn(Ops::log_(), ^^Constant<2.0>)));
      if (op == Ops::exp2_())
        return nBin(Ops::mul(),
            nBin(Ops::mul(), nUn(Ops::exp2_(), f), nUn(Ops::log_(), two)),
            df);
      if (op == Ops::cbrt_()) {
        // d/dx[cbrt(f)] = f' / (3 * cbrt(f)²)
        auto cbrt_f = nUn(Ops::cbrt_(), f);
        return nBin(Ops::div_(), df,
            nBin(Ops::mul(), ^^Constant<3.0>,
                nBin(Ops::mul(), cbrt_f, cbrt_f)));
      }

      // Special functions
      if (op == Ops::lgamma_())
        return nBin(Ops::mul(), nUn(Ops::digamma_(), f), df);
      if (op == Ops::erf_()) {
        // d/dx[erf(f)] = (2/√π) · exp(-f²) · f'
        auto coeff = nBin(Ops::div_(), two,
            nUn(Ops::sqrt_(), ^^Expression<PiOp>));
        auto gauss = nUn(Ops::exp_(),
            nUn(Ops::neg(), nBin(Ops::mul(), f, f)));
        return nBin(Ops::mul(), nBin(Ops::mul(), coeff, gauss), df);
      }
      if (op == Ops::erfc_()) {
        // d/dx[erfc(f)] = -(2/√π) · exp(-f²) · f'
        auto coeff = nBin(Ops::div_(), two,
            nUn(Ops::sqrt_(), ^^Expression<PiOp>));
        auto gauss = nUn(Ops::exp_(),
            nUn(Ops::neg(), nBin(Ops::mul(), f, f)));
        return nUn(Ops::neg(),
            nBin(Ops::mul(), nBin(Ops::mul(), coeff, gauss), df));
      }

      // Misc
      if (op == Ops::abs_()) {
        // d/dx[|f|] = (f / |f|) · f'
        return nBin(Ops::mul(),
            nBin(Ops::div_(), f, nUn(Ops::abs_(), f)), df);
      }
      if (op == Ops::floor_()) return zero;
      if (op == Ops::ceil_()) return zero;

      // Digamma: d/dx[ψ(f)] = ψ₁(f) · f' — but we don't have polygamma.
      // For now return the expression unchanged (consistent with diff.h which
      // also doesn't have a rule for standalone digamma differentiation).
      return expr;
    }

    // --- Binary operations ---
    if (arity == 2) {
      auto f = args[1];
      auto g = args[2];
      auto df = diffInfoImpl(f, var);
      auto dg = diffInfoImpl(g, var);

      // f + g → f' + g'
      if (op == Ops::add())
        return nBin(Ops::add(), df, dg);

      // f - g → f' - g'
      if (op == Ops::sub())
        return nBin(Ops::sub(), df, dg);

      // f * g → f'g + fg'
      if (op == Ops::mul())
        return nBin(Ops::add(),
            nBin(Ops::mul(), df, g),
            nBin(Ops::mul(), f, dg));

      // f / g → (f'g - fg') / g²
      if (op == Ops::div_())
        return nBin(Ops::div_(),
            nBin(Ops::sub(),
                nBin(Ops::mul(), df, g),
                nBin(Ops::mul(), f, dg)),
            nBin(Ops::mul(), g, g));

      // f^g → f^g · (g' · ln(f) + g · f' / f)
      if (op == Ops::pow_())
        return nBin(Ops::mul(), expr,
            nBin(Ops::add(),
                nBin(Ops::mul(), dg, nUn(Ops::log_(), f)),
                nBin(Ops::mul(), g, nBin(Ops::div_(), df, f))));

      // hypot(f, g) → (f·f' + g·g') / hypot(f, g)
      if (op == Ops::hypot_())
        return nBin(Ops::div_(),
            nBin(Ops::add(),
                nBin(Ops::mul(), f, df),
                nBin(Ops::mul(), g, dg)),
            expr);

      // Unhandled binary op — return unchanged
      return expr;
    }

    // Higher arity — shouldn't occur in practice
    return expr;
  }

  // Fallback: unknown template — return zero
  return zero;
}

// ============================================================================
// diffInfo — Public API: differentiate + simplify in the info domain
// ============================================================================
//
// Takes reflected expression and variable types, returns simplified derivative.
//
// Usage:
//   constexpr auto result = diffInfo(^^decltype(expr), ^^Symbol<X>);
//   using Result = [:result:];
//
consteval auto diffInfo(std::meta::info expr, std::meta::info var) -> std::meta::info {
  auto raw = diffInfoImpl(expr, var);
  return infoSimplify(raw);
}

}  // namespace tempura::symbolic4
