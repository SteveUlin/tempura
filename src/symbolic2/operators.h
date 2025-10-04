#pragma once

#include "core.h"
#include "meta/function_objects.h"

// Operator overloads for symbolic expressions

namespace tempura {

// --- Arithmetic Operators ---

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator+(LHS, RHS) {
  return Expression<AddOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator-(LHS, RHS) {
  return Expression<SubOp, LHS, RHS>{};
}

template <Symbolic Arg>
constexpr auto operator-(Arg) {
  return Expression<NegOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator*(LHS, RHS) {
  return Expression<MulOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator/(LHS, RHS) {
  return Expression<DivOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator%(LHS, RHS) {
  return Expression<ModOp, LHS, RHS>{};
}

// --- Comparison Operators ---

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator==(LHS, RHS) {
  return Expression<EqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator!=(LHS, RHS) {
  return Expression<NeqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator<(LHS, RHS) {
  return Expression<LtOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator<=(LHS, RHS) {
  return Expression<LeqOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator>(LHS, RHS) {
  return Expression<GtOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator>=(LHS, RHS) {
  return Expression<GeqOp, LHS, RHS>{};
}

// --- Logical Operators ---

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator&&(LHS, RHS) {
  return Expression<AndOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator||(LHS, RHS) {
  return Expression<OrOp, LHS, RHS>{};
}

template <Symbolic Arg>
constexpr auto operator!(Arg) {
  return Expression<NotOp, Arg>{};
}

// --- Bitwise Operators ---

template <Symbolic Arg>
constexpr auto operator~(Arg) {
  return Expression<BitNotOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator&(LHS, RHS) {
  return Expression<BitAndOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator|(LHS, RHS) {
  return Expression<BitOrOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator^(LHS, RHS) {
  return Expression<BitXorOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator<<(LHS, RHS) {
  return Expression<BitShiftLeftOp, LHS, RHS>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto operator>>(LHS, RHS) {
  return Expression<BitShiftRightOp, LHS, RHS>{};
}

// --- Trigonometric Functions ---

template <Symbolic Arg>
constexpr auto sin(Arg) {
  return Expression<SinOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto cos(Arg) {
  return Expression<CosOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto tan(Arg) {
  return Expression<TanOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto asin(Arg) {
  return Expression<AsinOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto acos(Arg) {
  return Expression<AcosOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto atan(Arg) {
  return Expression<AtanOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto atan2(LHS, RHS) {
  return Expression<Atan2Op, LHS, RHS>{};
}

// --- Hyperbolic Functions ---

template <Symbolic Arg>
constexpr auto sinh(Arg) {
  return Expression<SinhOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto cosh(Arg) {
  return Expression<CoshOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto tanh(Arg) {
  return Expression<TanhOp, Arg>{};
}

// --- Exponential and Logarithmic Functions ---

template <Symbolic Arg>
constexpr auto exp(Arg) {
  return Expression<ExpOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto log(Arg) {
  return Expression<LogOp, Arg>{};
}

template <Symbolic Arg>
constexpr auto sqrt(Arg) {
  return Expression<SqrtOp, Arg>{};
}

template <Symbolic LHS, Symbolic RHS>
constexpr auto pow(LHS, RHS) {
  return Expression<PowOp, LHS, RHS>{};
}

// --- Zero Arg Expressions (Constants) ---

static constexpr auto π = Expression<PiOp>{};
static constexpr auto e = Expression<EOp>{};

}  // namespace tempura
