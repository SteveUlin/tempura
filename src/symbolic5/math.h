#pragma once
#include "symbolic5/expr.h"
#include <cmath>

namespace tempura {

// ─── Math operators ──────────────────────────────────────────────────────────
//
// First domain: standard arithmetic and transcendental functions.
// Each op type serves dual roles — AST node identity (via ^^Op in the tree)
// and runtime evaluator (via Op::apply after splicing).
//
// Adding a new math op:
//   1. Define a struct with static apply(), a `notation` member, and a diff() method
//   2. Place the consteval factory function or operator overload below it
//   That's it — differentiation dispatches via Op::diff() automatically.

// ─── Precedence levels ──────────────────────────────────────────────────────
//
// Higher binds tighter. Operators at the same level associate left-to-right
// unless right_wraps is set (SubOp, DivOp).

inline constexpr int kAdditivePrec       = 1;
inline constexpr int kMultiplicativePrec = 2;
inline constexpr int kUnaryPrec          = 3;

// ─── Additive ────────────────────────────────────────────────────────────────

struct AddOp {
  static constexpr auto notation = InfixNotation{
      .symbol = " + ", .precedence = kAdditivePrec};
  static constexpr auto apply(auto a, auto b) { return a + b; }
  static consteval Expr diff(Expr f, Expr df, Expr g, Expr dg);
};

template <Symbolic L, Symbolic R>
consteval Expr operator+(L lhs, R rhs) {
  return makeBinary(^^AddOp, toInfo(lhs), metaOf(lhs), toInfo(rhs), metaOf(rhs));
}

struct SubOp {
  static constexpr auto notation = InfixNotation{
      .symbol = " - ", .precedence = kAdditivePrec, .right_wraps = true};
  static constexpr auto apply(auto a, auto b) { return a - b; }
  static consteval Expr diff(Expr f, Expr df, Expr g, Expr dg);
};

template <Symbolic L, Symbolic R>
consteval Expr operator-(L lhs, R rhs) {
  return makeBinary(^^SubOp, toInfo(lhs), metaOf(lhs), toInfo(rhs), metaOf(rhs));
}

// ─── Multiplicative ──────────────────────────────────────────────────────────

struct MulOp {
  static constexpr auto notation = InfixNotation{
      .symbol = " * ", .precedence = kMultiplicativePrec};
  static constexpr auto apply(auto a, auto b) { return a * b; }
  static consteval Expr diff(Expr f, Expr df, Expr g, Expr dg);
};

template <Symbolic L, Symbolic R>
consteval Expr operator*(L lhs, R rhs) {
  return makeBinary(^^MulOp, toInfo(lhs), metaOf(lhs), toInfo(rhs), metaOf(rhs));
}

struct DivOp {
  static constexpr auto notation = InfixNotation{
      .symbol = " / ", .precedence = kMultiplicativePrec, .right_wraps = true};
  static constexpr auto apply(auto a, auto b) { return a / b; }
  static consteval Expr diff(Expr f, Expr df, Expr g, Expr dg);
};

template <Symbolic L, Symbolic R>
consteval Expr operator/(L lhs, R rhs) {
  return makeBinary(^^DivOp, toInfo(lhs), metaOf(lhs), toInfo(rhs), metaOf(rhs));
}

// ─── Unary ───────────────────────────────────────────────────────────────────

struct NegOp {
  static constexpr auto notation = PrefixNotation{
      .symbol = "-", .precedence = kUnaryPrec};
  static constexpr auto apply(auto x) { return -x; }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr operator-(Symbolic auto e) {
  return makeUnary(^^NegOp, toInfo(e), metaOf(e));
}

// ─── Transcendentals ─────────────────────────────────────────────────────────
//
// apply() uses the ADL two-step: `using std::f; f(x);` so user-defined types
// that provide their own overloads (e.g. dual numbers, intervals) participate
// without requiring std:: specialization.

struct SinOp {
  static constexpr auto notation = FunctionNotation{.name = "sin"};
  static constexpr auto apply(auto x) { using std::sin; return sin(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr sin(Symbolic auto e) {
  return makeUnary(^^SinOp, toInfo(e), metaOf(e));
}

struct CosOp {
  static constexpr auto notation = FunctionNotation{.name = "cos"};
  static constexpr auto apply(auto x) { using std::cos; return cos(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr cos(Symbolic auto e) {
  return makeUnary(^^CosOp, toInfo(e), metaOf(e));
}

struct ExpOp {
  static constexpr auto notation = FunctionNotation{.name = "exp"};
  static constexpr auto apply(auto x) { using std::exp; return exp(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr exp(Symbolic auto e) {
  return makeUnary(^^ExpOp, toInfo(e), metaOf(e));
}

struct LogOp {
  static constexpr auto notation = FunctionNotation{.name = "log"};
  static constexpr auto apply(auto x) { using std::log; return log(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr log(Symbolic auto e) {
  return makeUnary(^^LogOp, toInfo(e), metaOf(e));
}

struct TanOp {
  static constexpr auto notation = FunctionNotation{.name = "tan"};
  static constexpr auto apply(auto x) { using std::tan; return tan(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr tan(Symbolic auto e) {
  return makeUnary(^^TanOp, toInfo(e), metaOf(e));
}

struct SqrtOp {
  static constexpr auto notation = FunctionNotation{.name = "sqrt"};
  static constexpr auto apply(auto x) { using std::sqrt; return sqrt(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr sqrt(Symbolic auto e) {
  return makeUnary(^^SqrtOp, toInfo(e), metaOf(e));
}

struct AbsOp {
  static constexpr auto notation = FunctionNotation{.name = "abs"};
  static constexpr auto apply(auto x) { using std::abs; return abs(x); }
  static consteval Expr diff(Expr u, Expr du);
};

consteval Expr abs(Symbolic auto e) {
  return makeUnary(^^AbsOp, toInfo(e), metaOf(e));
}

// ─── Named constants ──────────────────────────────────────────────────────
//
// Zero-arg ops that own their display symbol. Evaluate via apply(), render
// via ConstantNotation. Derivative is zero (no children, falls through in diff).

struct PiOp {
  static constexpr auto notation = ConstantNotation{.symbol = "π"};
  static constexpr auto apply() { return M_PI; }
};

consteval Expr pi() { return makeNullary(^^PiOp); }

struct EulerOp {
  static constexpr auto notation = ConstantNotation{.symbol = "e"};
  static constexpr auto apply() { return M_E; }
};

consteval Expr euler() { return makeNullary(^^EulerOp); }

// ─── Power ────────────────────────────────────────────────────────────────

struct PowOp {
  static constexpr auto notation = FunctionNotation{.name = "pow"};
  static constexpr auto apply(auto x, auto y) { using std::pow; return pow(x, y); }
  static consteval Expr diff(Expr f, Expr df, Expr g, Expr dg);
};

consteval Expr pow(Symbolic auto base, Symbolic auto exponent) {
  return makeBinary(^^PowOp, toInfo(base), metaOf(base),
                    toInfo(exponent), metaOf(exponent));
}

// ─── Derivative rules ────────────────────────────────────────────────────────
//
// Defined out-of-line so all operator/function definitions are visible.
// consteval demands the definition, not just a declaration, at the call site.

consteval Expr AddOp::diff(Expr, Expr df, Expr, Expr dg) { return df + dg; }
consteval Expr SubOp::diff(Expr, Expr df, Expr, Expr dg) { return df - dg; }
consteval Expr MulOp::diff(Expr f, Expr df, Expr g, Expr dg) { return f * dg + df * g; }
consteval Expr DivOp::diff(Expr f, Expr df, Expr g, Expr dg) {
  return (df * g - f * dg) / (g * g);
}

consteval Expr NegOp::diff(Expr, Expr du) { return -du; }
consteval Expr SinOp::diff(Expr u, Expr du) { return cos(u) * du; }
consteval Expr CosOp::diff(Expr u, Expr du) { return -sin(u) * du; }
consteval Expr ExpOp::diff(Expr u, Expr du) { return exp(u) * du; }
consteval Expr LogOp::diff(Expr u, Expr du) { return du / u; }

// d/dx[tan(u)] = du / cos²(u)
consteval Expr TanOp::diff(Expr u, Expr du) { return du / (cos(u) * cos(u)); }

// d/dx[√u] = du / (2√u)
consteval Expr SqrtOp::diff(Expr u, Expr du) { return du / (lit<2.0> * sqrt(u)); }

// d/dx[|u|] = (u / |u|) · du  — NaN at u=0, which is honest
consteval Expr AbsOp::diff(Expr u, Expr du) { return (u / abs(u)) * du; }

// d/dx[f^g] = f^g · (g' · log(f) + g · f'/f)
consteval Expr PowOp::diff(Expr f, Expr df, Expr g, Expr dg) {
  return pow(f, g) * (dg * log(f) + g * df / f);
}

}  // namespace tempura
