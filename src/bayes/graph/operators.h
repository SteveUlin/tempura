#pragma once

#include <concepts>
#include <tuple>

#include "bayes/graph/core.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// Arithmetic operators for RandomVars and DeterministicVars.
//
// When you perform arithmetic on graph nodes, you get a DeterministicVar:
//   auto y_hat = alpha + beta * x;  // DeterministicVar
//
// The DeterministicVar captures the symbolic expression and parent dependencies.

namespace tempura::bayes::graph {

using namespace tempura::symbolic3;

// =============================================================================
// Helper: Create DeterministicVar from expression and parents
// =============================================================================

template <typename Expr, typename... Parents>
constexpr auto makeDeterministicVar(Expr expr, std::tuple<Parents...> parents) {
  constexpr auto id = [] {};
  using Id = decltype(id);
  return DeterministicVar<Id, Expr, Parents...>{expr, parents};
}

// =============================================================================
// RandomVar + RandomVar -> DeterministicVar
// =============================================================================

template <typename Id1, typename D1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator+(const RandomVar<Id1, D1, P1...>& a,
                         const RandomVar<Id2, D2, P2...>& b) {
  auto expr = a.sym() + b.sym();
  return makeDeterministicVar(
      expr, std::tuple{a, b});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator-(const RandomVar<Id1, D1, P1...>& a,
                         const RandomVar<Id2, D2, P2...>& b) {
  auto expr = a.sym() - b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator*(const RandomVar<Id1, D1, P1...>& a,
                         const RandomVar<Id2, D2, P2...>& b) {
  auto expr = a.sym() * b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator/(const RandomVar<Id1, D1, P1...>& a,
                         const RandomVar<Id2, D2, P2...>& b) {
  auto expr = a.sym() / b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

// =============================================================================
// RandomVar + scalar -> DeterministicVar
// =============================================================================

template <typename Id, typename D, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(const RandomVar<Id, D, Ps...>& rv, T scalar) {
  auto expr = rv.sym() + Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{rv});
}

template <typename T, typename Id, typename D, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(T scalar, const RandomVar<Id, D, Ps...>& rv) {
  return rv + scalar;
}

template <typename Id, typename D, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(const RandomVar<Id, D, Ps...>& rv, T scalar) {
  auto expr = rv.sym() - Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{rv});
}

template <typename T, typename Id, typename D, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(T scalar, const RandomVar<Id, D, Ps...>& rv) {
  auto expr = Literal{static_cast<double>(scalar)} - rv.sym();
  return makeDeterministicVar(expr, std::tuple{rv});
}

template <typename Id, typename D, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(const RandomVar<Id, D, Ps...>& rv, T scalar) {
  auto expr = rv.sym() * Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{rv});
}

template <typename T, typename Id, typename D, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(T scalar, const RandomVar<Id, D, Ps...>& rv) {
  return rv * scalar;
}

template <typename Id, typename D, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(const RandomVar<Id, D, Ps...>& rv, T scalar) {
  auto expr = rv.sym() / Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{rv});
}

template <typename T, typename Id, typename D, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(T scalar, const RandomVar<Id, D, Ps...>& rv) {
  auto expr = Literal{static_cast<double>(scalar)} / rv.sym();
  return makeDeterministicVar(expr, std::tuple{rv});
}

// =============================================================================
// DeterministicVar + RandomVar -> DeterministicVar
// =============================================================================

template <typename Id1, typename E1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator+(const DeterministicVar<Id1, E1, P1...>& dv,
                         const RandomVar<Id2, D2, P2...>& rv) {
  auto expr = dv.sym() + rv.sym();
  return makeDeterministicVar(expr, std::tuple{dv, rv});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator+(const RandomVar<Id1, D1, P1...>& rv,
                         const DeterministicVar<Id2, E2, P2...>& dv) {
  return dv + rv;
}

template <typename Id1, typename E1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator-(const DeterministicVar<Id1, E1, P1...>& dv,
                         const RandomVar<Id2, D2, P2...>& rv) {
  auto expr = dv.sym() - rv.sym();
  return makeDeterministicVar(expr, std::tuple{dv, rv});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator-(const RandomVar<Id1, D1, P1...>& rv,
                         const DeterministicVar<Id2, E2, P2...>& dv) {
  auto expr = rv.sym() - dv.sym();
  return makeDeterministicVar(expr, std::tuple{rv, dv});
}

template <typename Id1, typename E1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator*(const DeterministicVar<Id1, E1, P1...>& dv,
                         const RandomVar<Id2, D2, P2...>& rv) {
  auto expr = dv.sym() * rv.sym();
  return makeDeterministicVar(expr, std::tuple{dv, rv});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator*(const RandomVar<Id1, D1, P1...>& rv,
                         const DeterministicVar<Id2, E2, P2...>& dv) {
  return dv * rv;
}

template <typename Id1, typename E1, typename... P1, typename Id2, typename D2,
          typename... P2>
constexpr auto operator/(const DeterministicVar<Id1, E1, P1...>& dv,
                         const RandomVar<Id2, D2, P2...>& rv) {
  auto expr = dv.sym() / rv.sym();
  return makeDeterministicVar(expr, std::tuple{dv, rv});
}

template <typename Id1, typename D1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator/(const RandomVar<Id1, D1, P1...>& rv,
                         const DeterministicVar<Id2, E2, P2...>& dv) {
  auto expr = rv.sym() / dv.sym();
  return makeDeterministicVar(expr, std::tuple{rv, dv});
}

// =============================================================================
// DeterministicVar + scalar -> DeterministicVar
// =============================================================================

template <typename Id, typename E, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(const DeterministicVar<Id, E, Ps...>& dv, T scalar) {
  auto expr = dv.sym() + Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{dv});
}

template <typename T, typename Id, typename E, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator+(T scalar, const DeterministicVar<Id, E, Ps...>& dv) {
  return dv + scalar;
}

template <typename Id, typename E, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(const DeterministicVar<Id, E, Ps...>& dv, T scalar) {
  auto expr = dv.sym() - Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{dv});
}

template <typename T, typename Id, typename E, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator-(T scalar, const DeterministicVar<Id, E, Ps...>& dv) {
  auto expr = Literal{static_cast<double>(scalar)} - dv.sym();
  return makeDeterministicVar(expr, std::tuple{dv});
}

template <typename Id, typename E, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(const DeterministicVar<Id, E, Ps...>& dv, T scalar) {
  auto expr = dv.sym() * Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{dv});
}

template <typename T, typename Id, typename E, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator*(T scalar, const DeterministicVar<Id, E, Ps...>& dv) {
  return dv * scalar;
}

template <typename Id, typename E, typename... Ps, typename T>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(const DeterministicVar<Id, E, Ps...>& dv, T scalar) {
  auto expr = dv.sym() / Literal{static_cast<double>(scalar)};
  return makeDeterministicVar(expr, std::tuple{dv});
}

template <typename T, typename Id, typename E, typename... Ps>
  requires std::is_arithmetic_v<T>
constexpr auto operator/(T scalar, const DeterministicVar<Id, E, Ps...>& dv) {
  auto expr = Literal{static_cast<double>(scalar)} / dv.sym();
  return makeDeterministicVar(expr, std::tuple{dv});
}

// =============================================================================
// DeterministicVar + DeterministicVar -> DeterministicVar
// =============================================================================

template <typename Id1, typename E1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator+(const DeterministicVar<Id1, E1, P1...>& a,
                         const DeterministicVar<Id2, E2, P2...>& b) {
  auto expr = a.sym() + b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

template <typename Id1, typename E1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator-(const DeterministicVar<Id1, E1, P1...>& a,
                         const DeterministicVar<Id2, E2, P2...>& b) {
  auto expr = a.sym() - b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

template <typename Id1, typename E1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator*(const DeterministicVar<Id1, E1, P1...>& a,
                         const DeterministicVar<Id2, E2, P2...>& b) {
  auto expr = a.sym() * b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

template <typename Id1, typename E1, typename... P1, typename Id2, typename E2,
          typename... P2>
constexpr auto operator/(const DeterministicVar<Id1, E1, P1...>& a,
                         const DeterministicVar<Id2, E2, P2...>& b) {
  auto expr = a.sym() / b.sym();
  return makeDeterministicVar(expr, std::tuple{a, b});
}

// =============================================================================
// Unary Minus
// =============================================================================

template <typename Id, typename D, typename... Ps>
constexpr auto operator-(const RandomVar<Id, D, Ps...>& rv) {
  auto expr = Literal{0.0} - rv.sym();
  return makeDeterministicVar(expr, std::tuple{rv});
}

template <typename Id, typename E, typename... Ps>
constexpr auto operator-(const DeterministicVar<Id, E, Ps...>& dv) {
  auto expr = Literal{0.0} - dv.sym();
  return makeDeterministicVar(expr, std::tuple{dv});
}

}  // namespace tempura::bayes::graph
