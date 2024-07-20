#pragma once

#include "src/symbolic/symbolic.h"

namespace tempura::symbolic {

class Plus {
public:
  static inline constexpr std::string_view kSymbol = "+"sv;
  static inline constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename... Args>
  static constexpr auto operator()(Args... args) {
    return (args + ...);
  }
};
static_assert(Operator<Plus>);

constexpr auto operator+(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Plus, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator+(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Plus, decltype(lhs), decltype(rhs)>{};
}

class Minus {
public:
  inline static constexpr std::string_view kSymbol = "-"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename... Args>
  static constexpr auto operator()(Args... args) {
    return (args - ...);
  }
};
static_assert(Operator<Minus>);

constexpr auto operator-(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Minus, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator-(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Minus, decltype(lhs), decltype(rhs)>{};
}

class Multiplies {
public:
  inline static constexpr std::string_view kSymbol = "*"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename... Args>
  static constexpr auto operator()(Args... args) {
    return (args * ...);
  }
};
static_assert(Operator<Multiplies>);

constexpr auto operator*(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Multiplies, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator*(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Multiplies, decltype(lhs), decltype(rhs)>{};
}

class Divides {
public:
  inline static constexpr std::string_view kSymbol = "/"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename... Args>
  static constexpr auto operator()(Args... args) {
    return (args / ...);
  }
};
static_assert(Operator<Divides>);

constexpr auto operator/(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Divides, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator/(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Divides, decltype(lhs), decltype(rhs)>{};
}

class Power {
public:
  inline static constexpr std::string_view kSymbol = "^"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  template <typename Base, typename Pow>
  static constexpr auto operator()(Base base, Pow pow) {
    return std::pow(base, pow);
  }
};
static_assert(Operator<Power>);

constexpr auto operator^(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Power, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator^(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Power, decltype(lhs), decltype(rhs)>{};
}

struct Sin {
  inline static constexpr std::string_view kSymbol = "sin"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto x) { return std::sin(x); }
};
static_assert(Operator<Sin>);

constexpr auto sin(Symbolic auto x) {
  return SymbolicExpression<Sin, decltype(x)>{};
}

constexpr auto sin(Matcher auto x) {
  return MatcherExpression<Sin, decltype(x)>{};
}

struct Cos {
  inline static constexpr std::string_view kSymbol = "cos"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto x) { return std::cos(x); }
};
static_assert(Operator<Cos>);

constexpr auto cos(Symbolic auto x) {
  return SymbolicExpression<Cos, decltype(x)>{};
}

constexpr auto cos(Matcher auto x) {
  return MatcherExpression<Cos, decltype(x)>{};
}

struct Tan {
  inline static constexpr std::string_view kSymbol = "tan"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()(auto x) { return std::tan(x); }
};
static_assert(Operator<Tan>);

constexpr auto tan(Symbolic auto x) {
  return SymbolicExpression<Tan, decltype(x)>{};
}

constexpr auto tan(Matcher auto x) {
  return MatcherExpression<Tan, decltype(x)>{};
}

struct E {
  inline static constexpr std::string_view kSymbol = "e"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()() { return std::numbers::e; }
};
static_assert(Operator<E>);

constexpr auto e = SymbolicExpression<E>{};

struct Pi {
  inline static constexpr std::string_view kSymbol = "π"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  constexpr auto operator()() { return std::numbers::pi; };
};
static_assert(Operator<Pi>);

constexpr auto π = SymbolicExpression<Pi>{};

} // namespace tempura::symbolic
