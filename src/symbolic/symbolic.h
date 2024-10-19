#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>
#include <type_traits>
#include <utility>

#include "symbolic/type_list.h"
#include "symbolic/type_name.h"

namespace tempura::symbolic {

using namespace std::literals;

namespace internal {

// Associate some value with a type, usually a tag type for static lookup.
template <typename T, typename U>
class TypeValueBinder {
 public:
  using ValueType = std::remove_cvref<U>::type;

  constexpr explicit TypeValueBinder(U&& value)
      : value_(std::forward<U>(value)) {}

  constexpr auto value() const -> const ValueType& { return value_; }

  constexpr auto operator[](T /*unused*/) const -> const ValueType& {
    return value_;
  }

 private:
  U value_;
};

// Is this type a matcher?
template <typename T>
inline constexpr bool MatcherTypeTrait = false;

// Is this type a symbol?
template <typename T>
inline constexpr bool SymbolicTypeTrait = false;

}  // namespace internal

// All symbols can also be used as matchers
template <typename T>
concept Matcher =
    internal::MatcherTypeTrait<T> || internal::SymbolicTypeTrait<T>;

// Define Symbolic as a restriction of the Matcher concept so operator overloads
// prefer to resolve to Symbolic versions
template <typename T>
concept Symbolic = Matcher<T> && internal::SymbolicTypeTrait<T>;

// A collection of bound values that should eventually be provided to a
// symbolic expression.
template <typename... Binders>
struct Substitution : Binders... {
  template <typename... Ts, typename... Us>
  constexpr Substitution(internal::TypeValueBinder<Ts, Us>&&... binders)
      : Binders(std::forward<internal::TypeValueBinder<Ts, Us>>(binders))... {}

  using Binders::operator[]...;
};

template <typename... Ts, typename... Us>
Substitution(internal::TypeValueBinder<Ts, Us>...)
    -> Substitution<internal::TypeValueBinder<Ts, Us>...>;

// --- Constants ---

// Constants are unchanging compile time values, notably tempura represents
// coefficients as constant values. However, tempura represents named constants
// like π and e as functions with zero arguments. This distinction enables
// special handling for pretty printing.
template <auto VALUE>
struct Constant {
  constexpr Constant() = default;
  static constexpr auto value() { return VALUE; }

  template <typename... Ts, typename... Us>
  constexpr auto operator()(
      const Substitution<internal::TypeValueBinder<Ts, Us>...>& /*unused*/) {
    return value();
  }
};

template <auto VALUE>
inline constexpr bool internal::SymbolicTypeTrait<Constant<VALUE>> = true;

template <char... chars>
constexpr auto toInt() {
  return [] constexpr {
    int value = 0;
    ((value = value * 10 + (chars - '0')), ...);
    return value;
  }();
}

template <char... chars>
constexpr auto toDouble() {
  return [] constexpr {
    double value = 0.;
    double fraction = 1.;
    bool is_fraction = false;
    ([&] {
      if constexpr (chars == '.') {
        is_fraction = true;
      } else {
        fraction = is_fraction ? fraction / 10 : fraction;
        value = value * 10 + (chars - '0');
      }
    }(), ...);
    return value * fraction;
  }();
}

template <char... chars>
constexpr auto count(char c) {
  return ((c == chars) + ...);
}

template <char... chars>
constexpr auto operator""_c() {
  constexpr int dot_count = count<chars...>('.');
  if constexpr (dot_count == 0) {
    return Constant<toInt<chars...>()>{};
  } else if constexpr (dot_count == 1) {
    return Constant<toDouble<chars...>()>{};
  } else {
    static_assert(dot_count < 2, "Invalid floating point constant");
  }
}

// --- Symbols ---

template <size_t N>
struct StringLiteral {
  constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }
  char value[N];
};

// A symbol is a generic symbolic with a unique identifier. Users evaluate
// expressions by binding values to symbols.
template <StringLiteral label, auto id>
struct Symbol {
  constexpr static auto kLabel = label;
  constexpr static auto kId = id;

  template <typename T>
  constexpr auto operator=(T&& t) const {
    return internal::TypeValueBinder<Symbol<label, id>, T>{std::forward<T>(t)};
  };

  template <typename... Ts, typename... Us>
  constexpr auto operator()(
      const Substitution<internal::TypeValueBinder<Ts, Us>...>& substitution)
      const {
    return substitution[*this];
  }
};

#define TEMPURA_SYMBOL(VAR) tempura::symbolic::Symbol<#VAR, __COUNTER__> VAR{};

// Trick the C++ preprocessor into expanding the arguments of a variadic macro
// https://www.scs.stanford.edu/~dm/blog/va-opt.html
#define TEMPURA_PARENS ()

#define TEMPURA_EXPAND(...) \
  TEMPURA_EXPAND4(          \
      TEMPURA_EXPAND4(TEMPURA_EXPAND4(TEMPURA_EXPAND4(__VA_ARGS__))))
#define TEMPURA_EXPAND4(...) \
  TEMPURA_EXPAND3(           \
      TEMPURA_EXPAND3(TEMPURA_EXPAND3(TEMPURA_EXPAND3(__VA_ARGS__))))
#define TEMPURA_EXPAND3(...) \
  TEMPURA_EXPAND2(           \
      TEMPURA_EXPAND2(TEMPURA_EXPAND2(TEMPURA_EXPAND2(__VA_ARGS__))))
#define TEMPURA_EXPAND2(...) \
  TEMPURA_EXPAND1(           \
      TEMPURA_EXPAND1(TEMPURA_EXPAND1(TEMPURA_EXPAND1(__VA_ARGS__))))
#define TEMPURA_EXPAND1(...) __VA_ARGS__

#define TEMPURA_FOR_EACH(macro, ...) \
  __VA_OPT__(TEMPURA_EXPAND(TEMPURA_FOR_EACH_HELPER(macro, __VA_ARGS__)))

#define TEMPURA_FOR_EACH_HELPER(macro, a1, ...) \
  macro(a1);                                    \
  __VA_OPT__(TEMPURA_FOR_EACH_AGAIN TEMPURA_PARENS(macro, __VA_ARGS__))

#define TEMPURA_FOR_EACH_AGAIN() TEMPURA_FOR_EACH_HELPER

#define TEMPURA_SYMBOLS(...) TEMPURA_FOR_EACH(TEMPURA_SYMBOL, __VA_ARGS__)

template <StringLiteral label, auto id>
inline constexpr bool internal::SymbolicTypeTrait<Symbol<label, id>> = true;

// --- Symbolic Expressions ---

enum class DisplayMode {
  kInfix,
  kPrefix,
};

// A symbolic operator is a function object that can be applied to symbolic
// expressions.
template <typename T>
concept Operator = requires {
  // You can call and operator like a function
  []<typename... Args>(T t) { t(Args{}...); };

  // Information for Pretty Printing
  requires std::is_same_v<decltype(T::kDisplayMode), const DisplayMode>;
  requires std::is_same_v<decltype(T::kSymbol), const std::string_view>;
};

consteval auto operator==(Operator auto lhs, Operator auto rhs) -> bool {
  return std::is_same_v<decltype(lhs), decltype(rhs)>;
}

template <Operator Op, Symbolic... Args>
struct SymbolicExpression {
  constexpr SymbolicExpression() = default;
  constexpr SymbolicExpression(const SymbolicExpression&) = default;
  constexpr auto operator=(const SymbolicExpression&) -> SymbolicExpression& =
                                                             default;

  constexpr static auto op() { return Op{}; }
  constexpr static auto terms() { return TypeList<Args...>{}; }

  constexpr static auto operand()
    requires (sizeof...(Args) == 1)
  {
    return terms().head();
  }

  constexpr static auto left()
    requires(sizeof...(Args) == 2)
  {
    return terms().head();
  }
  constexpr static auto right()
    requires(sizeof...(Args) == 2)
  {
    return terms().tail().head();
  }

  constexpr SymbolicExpression(Op /*unused*/, Args... /*unused*/) {};
  constexpr SymbolicExpression(Op /*unused*/, TypeList<Args...> /*unused*/) {};

  // Evaluate the expression given a substitution of values for symbols.
  template <typename... Ts, typename... Us>
  static constexpr auto operator()(
      const Substitution<internal::TypeValueBinder<Ts, Us>...>& substitution) {
    return op()(Args{}(substitution)...);
  }
};

template <Operator Op, Symbolic... Args>
inline constexpr bool
    internal::SymbolicTypeTrait<SymbolicExpression<Op, Args...>> = true;

// -- Comparison Operators --

// TODO: Consider not using operator for these comparisons so the operators
//       could be used in expressions instead.

template <auto LHS, auto RHS>
constexpr auto operator==(Constant<LHS> /*unused*/,
                          Constant<RHS> /*unused*/) -> bool {
  return LHS == RHS;
}

constexpr auto operator==(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return std::is_same_v<decltype(lhs), decltype(rhs)>;
}

constexpr auto operator!=(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return !(lhs == rhs);
}

constexpr auto operator<=>(Symbolic auto lhs, Symbolic auto rhs) {
  return typeName(lhs) <=> typeName(rhs);
}

// --- Matcher Expressions ---

// A matcher expression acts much like a SymbolicExpression but can only
// be used for comparisons.
template <Operator Op, Matcher... Matchers>
struct MatcherExpression {
  constexpr MatcherExpression() = default;
  constexpr MatcherExpression(const MatcherExpression&) = default;
  constexpr auto operator=(const MatcherExpression&) -> MatcherExpression& =
                                                            default;

  constexpr static auto op() { return Op{}; }
  constexpr static auto terms() { return TypeList<Matchers...>{}; }

  constexpr MatcherExpression(Op /*unused*/, Matchers... /*unused*/) {};
};

template <Operator Op, Matcher... Args>
consteval auto makeExpr(Op op, Args... args) {
  if constexpr ((Symbolic<Args> && ...)) {
    return SymbolicExpression{op, args...};
  } else {
    return MatcherExpression{op, args...};
  }
}

template <Operator Op, Matcher... Args>
inline constexpr bool
    internal::MatcherTypeTrait<MatcherExpression<Op, Args...>> = true;

}  // namespace tempura::symbolic
