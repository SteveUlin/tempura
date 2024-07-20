#pragma once

#include "src/type_list.h"

#include <cmath>
#include <string_view>
#include <type_traits>
#include <utility>

namespace tempura::symbolic {

using namespace std::literals;

namespace internal {

template <typename T, typename U>
class TypeValueBinder {
public:
  using ValueType = std::remove_cvref<U>::type;

  constexpr explicit TypeValueBinder(U&& value) : value_(std::forward<U>(value)) {}

  constexpr auto value() const -> const ValueType& {
    return value_;
  }

  constexpr auto operator[](T) const -> const ValueType& {
    return value_;
  }

private:
  U value_;
};

template <typename T>
inline constexpr bool MatcherTypeTrait = false;

template <typename T>
inline constexpr bool SymbolicTypeTrait = false;

}  // namespace internal

// All symbols can also be used as matchers
template <typename T>
concept Matcher = internal::MatcherTypeTrait<T> || internal::SymbolicTypeTrait<T>;

// Define Symbolic as a restriction of the Matcher concept so operator overloads
// prefer to resolve to Symbolic versions
template <typename T>
concept Symbolic = Matcher<T> && internal::SymbolicTypeTrait<T>;

template <typename... Binders>
class Substitution : Binders... {
public:
  template <typename... Ts, typename... Us>
  constexpr Substitution(internal::TypeValueBinder<Ts, Us>&&... binders) :
    Binders(std::forward<internal::TypeValueBinder<Ts, Us>>(binders))... {}

  using Binders::operator[]...;
};

template <typename... Ts, typename... Us>
Substitution(internal::TypeValueBinder<Ts, Us>...)
    -> Substitution<internal::TypeValueBinder<Ts, Us>...>;

// --- Constants ---

// Constants are unchanging compile time values, notably tempura represents coefficients as constant
// values. However, tempura represents named constants like π and e as functions with zero
// arguments. This distinction enables special handling for pretty printing.
template <auto VALUE>
struct Constant {
  static constexpr auto value() {
    return VALUE;
  }

  template<typename... Ts, typename... Us>
  constexpr auto operator()(const Substitution<internal::TypeValueBinder<Ts, Us>...>&) {
    return value();
  }
};

template <auto LHS, auto RHS>
constexpr auto operator==(Constant<LHS>, Constant<RHS>) -> bool {
  return LHS == RHS;
}

constexpr auto operator==(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return std::is_same_v<decltype(lhs), decltype(rhs)>;
}

constexpr auto operator!=(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return !(lhs == rhs);
}

template <auto VALUE>
inline constexpr bool internal::SymbolicTypeTrait<Constant<VALUE>> = true;

// --- Symbols ---

// A symbol is a generic symbolic with a unique identifier. Users evaluate
// expressions by binding values to symbols.
template <auto ID>
struct Symbol {
  static constexpr auto id() {
    return ID;
  }

  template <typename T>
  constexpr auto operator=(T&& t) const {
    return internal::TypeValueBinder<Symbol<ID>, T>{std::forward<T>(t)};
  };

  template <typename... Ts, typename... Us>
  constexpr auto operator()(
      const Substitution<internal::TypeValueBinder<Ts, Us>...>& substitution) const {
    return substitution[*this];
  }
};

// C++ treats each lambda as a distinct type. By adding a lambda into the template
// parameters, each Symbol will also be a distinct type. Further when clang pretty
// prints the Symbol type it will include the source file line and offset. This
// is convenient for sorting types.
#define SYMBOL() Symbol<__COUNTER__>{}

template <auto kId>
inline constexpr bool internal::SymbolicTypeTrait<Symbol<kId>> = true;

// --- Symbolic Expressions ---

enum class DisplayMode {
  kInfix,
  kPrefix,
};

template <typename T>
concept Operator = requires {
  []<typename... Args>(T t) {
    t(Args{}...);
  };

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
  constexpr auto operator=(const SymbolicExpression&) -> SymbolicExpression& {};

  constexpr SymbolicExpression(Op, Args...) {};
  constexpr SymbolicExpression(Op, TypeList<Args...>) {};

  template<typename... Ts, typename... Us>
  constexpr auto operator()(const Substitution<internal::TypeValueBinder<Ts, Us>...>& substitution) {
    return Op{}(Args{}(substitution)...);
  }

  consteval static auto terms() {
    return TypeList<Args...>{};
  }

  consteval static auto op() {
    return Op{};
  }
};

template <Operator Op, Symbolic... Args>
inline constexpr bool internal::SymbolicTypeTrait<SymbolicExpression<Op, Args...>> = true;

// --- Matcher Expressions ---

template <Operator Op, Matcher... Matchers>
struct MatcherExpression {
  constexpr MatcherExpression() = default;
  constexpr MatcherExpression(const MatcherExpression&) = default;
  constexpr auto operator=(const MatcherExpression&) -> MatcherExpression& {};

  constexpr MatcherExpression(Op, Matchers...) {};
};

template <Operator Op, Matcher... Args>
inline constexpr bool internal::MatcherTypeTrait<MatcherExpression<Op, Args...>> = true;

}  // namespace tempura::symbolic
