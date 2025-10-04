#pragma once

#include "core.h"

// Expression accessor functions

namespace tempura {

// --- Expression Accessors ---

constexpr auto operand(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic Arg>
constexpr auto operand(Expression<Op, Arg>) {
  return Arg{};
}

constexpr auto left(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic LHS, Symbolic RHS>
constexpr auto left(Expression<Op, LHS, RHS>) {
  return LHS{};
}

constexpr auto right(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic LHS, Symbolic RHS>
constexpr auto right(Expression<Op, LHS, RHS>) {
  return RHS{};
}

constexpr auto getOp(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic... Args>
constexpr auto getOp(Expression<Op, Args...>) {
  return Op{};
}

}  // namespace tempura
