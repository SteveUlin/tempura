#pragma once

#include "src/symbolic/symbolic.h"

namespace tempura::symbolic {

constexpr auto operator+(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<std::plus<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator+(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<std::plus<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator-(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<std::minus<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator-(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<std::minus<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator*(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<std::multiplies<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator*(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<std::multiplies<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator/(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<std::divides<>, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator/(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<std::divides<>, decltype(lhs), decltype(rhs)>{};
}

struct Sin {
  constexpr auto operator()(auto x) { return std::sin(x); }
};

constexpr auto sin(Symbolic auto x) {
  return SymbolicExpression<Sin, decltype(x)>{};
}

constexpr auto sin(Matcher auto x) {
  return MatcherExpression<Sin, decltype(x)>{};
}

struct Cos {
  constexpr auto operator()(auto x) { return std::cos(x); }
};

constexpr auto cos(Symbolic auto x) {
  return SymbolicExpression<Cos, decltype(x)>{};
}

constexpr auto cos(Matcher auto x) {
  return MatcherExpression<Cos, decltype(x)>{};
}

struct Tan {
  constexpr auto operator()(auto x) { return std::tan(x); }
};

constexpr auto tan(Symbolic auto x) {
  return SymbolicExpression<Tan, decltype(x)>{};
}

constexpr auto tan(Matcher auto x) {
  return MatcherExpression<Tan, decltype(x)>{};
}

struct E {
  constexpr auto operator()() { return std::numbers::e; }
};

constexpr auto e = SymbolicExpression<E>{};

struct Pi {
  constexpr auto operator()() { return std::numbers::pi; };
};

constexpr auto π = SymbolicExpression<Pi>{};

} // namespace tempura::symbolic
