#pragma once

#include <concepts>
#include <tuple>

#include "bayes2/core.h"
#include "symbolic3/operators.h"

// Arithmetic operators for GraphNodes (RandomVar and DeterministicVar).
//
// Arithmetic on graph nodes produces DeterministicVar with:
//   1. A symbolic expression combining the operands
//   2. Parent tuple capturing dependencies for DAG traversal
//   3. Unique type identity (from decltype([]{})) for deduplication
//
// Example:
//   auto y_hat = alpha + beta * x;  // DeterministicVar

namespace tempura::bayes2 {

using namespace tempura::symbolic3;

// =============================================================================
// Helper: Create DeterministicVar from expression and parents
// =============================================================================
//
// Each call generates a unique lambda type via decltype([]{}) as the Id,
// ensuring every arithmetic result has a distinct type for DAG tracking.

template <typename Expr, typename... Parents>
constexpr auto makeDeterministicVar(Expr expr, std::tuple<Parents...> parents) {
  constexpr auto id = [] {};
  using Id = decltype(id);
  return DeterministicVar<Id, Expr, Parents...>{expr, parents};
}

// =============================================================================
// Binary Operators: GraphNode OP GraphNode
// =============================================================================
//
// Both operands are graph nodes. Result depends on both.

template <GraphNode L, GraphNode R>
constexpr auto operator+(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() + rhs.sym(), std::tuple{lhs, rhs});
}

template <GraphNode L, GraphNode R>
constexpr auto operator-(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() - rhs.sym(), std::tuple{lhs, rhs});
}

template <GraphNode L, GraphNode R>
constexpr auto operator*(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() * rhs.sym(), std::tuple{lhs, rhs});
}

template <GraphNode L, GraphNode R>
constexpr auto operator/(const L& lhs, const R& rhs) {
  return makeDeterministicVar(lhs.sym() / rhs.sym(), std::tuple{lhs, rhs});
}

// =============================================================================
// Binary Operators: GraphNode OP scalar
// =============================================================================
//
// Scalar (arithmetic type) is wrapped as Literal. Result depends only on the
// GraphNode. Uses std::is_arithmetic_v to handle both floating-point and
// integral types uniformly, converting to double for the symbolic expression.

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() + Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} + node.sym(),
                              std::tuple{node});
}

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() - Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} - node.sym(),
                              std::tuple{node});
}

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() * Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} * node.sym(),
                              std::tuple{node});
}

template <GraphNode G, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(const G& node, T scalar) {
  return makeDeterministicVar(node.sym() / Literal{static_cast<double>(scalar)},
                              std::tuple{node});
}

template <typename T, GraphNode G>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(T scalar, const G& node) {
  return makeDeterministicVar(Literal{static_cast<double>(scalar)} / node.sym(),
                              std::tuple{node});
}

// =============================================================================
// Unary Operators
// =============================================================================

template <GraphNode G>
constexpr auto operator-(const G& node) {
  // Express negation as 0-x to reuse SubOp; ensures consistent expression tree
  return makeDeterministicVar(Literal{0.0} - node.sym(), std::tuple{node});
}

// ⚠️ Unary + creates a DeterministicVar wrapper even though it's semantically
// identity. This ensures uniform expression tree structure for symbolic
// differentiation - without it, +x and x would have different types, breaking
// generic code that expects all arithmetic results to be DeterministicVar.
template <GraphNode G>
constexpr auto operator+(const G& node) {
  return makeDeterministicVar(node.sym(), std::tuple{node});
}

}  // namespace tempura::bayes2
