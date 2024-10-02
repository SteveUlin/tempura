#pragma once

#include "symbolic/symbolic.h"

namespace tempura::symbolic {

class Plus {
 public:
  static inline constexpr std::string_view kSymbol = "+"sv;
  static inline constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  static constexpr auto operator()(const auto& lhs, const auto& rhs) {
    return lhs + rhs;
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

  static constexpr auto operator()(const auto& lhs, const auto& rhs) {
    return lhs - rhs;
  }
};
static_assert(Operator<Minus>);

constexpr auto operator-(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Minus, decltype(lhs), decltype(rhs)>{};
}

constexpr auto operator-(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Minus, decltype(lhs), decltype(rhs)>{};
}

struct Negate {
  inline static constexpr std::string_view kSymbol = "-"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  static constexpr auto operator()(auto x) { return -x; }
};
static_assert(Operator<Negate>);

constexpr auto operator-(Symbolic auto x) {
  return SymbolicExpression<Negate, decltype(x)>{};
}

constexpr auto operator-(Matcher auto x) {
  return MatcherExpression<Negate, decltype(x)>{};
}

class Multiplies {
 public:
  inline static constexpr std::string_view kSymbol = "*"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kInfix;

  static constexpr auto operator()(const auto& lhs, const auto& rhs) {
    return lhs * rhs;
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

  static constexpr auto operator()(const auto& lhs, const auto& rhs) {
    return lhs / rhs;
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

  static constexpr auto operator()(auto base, auto power) {
    using std::pow;
    return pow(base, power);
  }
};
static_assert(Operator<Power>);

constexpr auto pow(Symbolic auto lhs, Symbolic auto rhs) {
  return SymbolicExpression<Power, decltype(lhs), decltype(rhs)>{};
}

constexpr auto pow(Matcher auto lhs, Matcher auto rhs) {
  return MatcherExpression<Power, decltype(lhs), decltype(rhs)>{};
}

struct Sqrt {
  inline static constexpr std::string_view kSymbol = "sqrt"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  static constexpr auto operator()(auto x) {
    using std::sqrt;
    return sqrt(x);
  }
};

constexpr auto sqrt(Symbolic auto x) {
  return SymbolicExpression<Sqrt, decltype(x)>{};
}

constexpr auto sqrt(Matcher auto x) {
  return MatcherExpression<Sqrt, decltype(x)>{};
}

struct Exp {
  inline static constexpr std::string_view kSymbol = "exp"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  static constexpr auto operator()(auto x) {
    using std::exp;
    return exp(x);
  }
};

constexpr auto exp(Symbolic auto x) {
  return SymbolicExpression<Exp, decltype(x)>{};
}

constexpr auto exp(Matcher auto x) {
  return MatcherExpression<Exp, decltype(x)>{};
}

struct Log {
  inline static constexpr std::string_view kSymbol = "log"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  static constexpr auto operator()(auto x) {
    using std::log;
    return log(x);
  }
};

constexpr auto log(Symbolic auto x) {
  return SymbolicExpression<Log, decltype(x)>{};
}

constexpr auto log(Matcher auto x) {
  return MatcherExpression<Log, decltype(x)>{};
}

struct Sin {
  inline static constexpr std::string_view kSymbol = "sin"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  static constexpr auto operator()(auto x) {
    using std::sin;
    return sin(x);
  }
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

  static constexpr auto operator()(auto x) {
    using std::cos;
    return cos(x);
  }
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

  static constexpr auto operator()(auto x) {
    using std::tan;
    return tan(x);
  }
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

  static constexpr auto operator()() { return std::numbers::e; }
};
static_assert(Operator<E>);

constexpr auto e = SymbolicExpression<E>{};

struct Pi {
  inline static constexpr std::string_view kSymbol = "π"sv;
  inline static constexpr DisplayMode kDisplayMode = DisplayMode::kPrefix;

  static constexpr auto operator()() { return std::numbers::pi; };
};
static_assert(Operator<Pi>);

constexpr auto π = SymbolicExpression<Pi>{};

}  // namespace tempura::symbolic
