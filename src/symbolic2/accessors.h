#pragma once

#include "core.h"

// Expression accessor functions

namespace tempura {

// --- Expression Accessors ---
//
// Type-safe accessors that return Never{} for non-matching patterns,
// enabling compile-time checks via requires clauses

// Extract argument from unary expression (e.g., sin(x) → x)
constexpr auto operand(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic Arg>
constexpr auto operand(Expression<Op, Arg>) {
  return Arg{};
}

// Extract left operand from binary expression (e.g., x + y → x)
constexpr auto left(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic LHS, Symbolic RHS>
constexpr auto left(Expression<Op, LHS, RHS>) {
  return LHS{};
}

// Extract right operand from binary expression (e.g., x + y → y)
constexpr auto right(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic LHS, Symbolic RHS>
constexpr auto right(Expression<Op, LHS, RHS>) {
  return RHS{};
}

// Extract operator type from expression
constexpr auto getOp(Symbolic auto) { return Never{}; }
template <typename Op, Symbolic... Args>
constexpr auto getOp(Expression<Op, Args...>) {
  return Op{};
}

}  // namespace tempura
