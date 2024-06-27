#pragma once

#include "src/compile_time_string.h"

#include <cmath>
#include <functional>
#include <numeric>
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

template <typename T>
inline constexpr bool TypeValueBinderTypeTrait = false;

template <typename T, typename U>
inline constexpr bool TypeValueBinderTypeTrait<TypeValueBinder<T, U>> = true;

}  // namespace internal

// All symbols can also be used as matchers
template <typename T>
concept Matcher = internal::MatcherTypeTrait<T> || internal::SymbolicTypeTrait<T>;

// Define Symbolic as a restriction of the Matcher concept so operator overloads
// prefer to resolve to Symbolic versions
template <typename T>
concept Symbolic = Matcher<T> && internal::SymbolicTypeTrait<T>;

template <typename T>
concept BinderT = internal::TypeValueBinderTypeTrait<T>;

template <BinderT... Binders>
class Substitution : Binders... {
public:
  constexpr Substitution(Binders&&... binders) : Binders(std::forward<Binders>(binders))... {}

  using Binders::operator[]...;
};

template <BinderT... Binders>
Substitution(Binders...) -> Substitution<Binders...>;

// --- Constants ---

// Constants are unchanging compile time values, notably tempura represents coefficients as constant
// values. However, tempura represents named constants like π and e as functions with zero
// arguments. This distinction enables special handling for pretty printing.
template <auto VALUE>
struct Constant {
  static constexpr auto value() {
    return VALUE;
  }

  template<BinderT... Binders>
  constexpr auto operator()(const Substitution<Binders...>&) {
    return value();
  }
};

template <auto LHS, auto RHS>
constexpr auto operator==(const Constant<LHS>&, const Constant<RHS>&) {
  return LHS == RHS;
}

template <auto LHS, auto RHS>
constexpr auto operator!=(const Constant<LHS>&, const Constant<RHS>&) {
  return LHS != RHS;
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

  template<BinderT... Binders>
  constexpr auto operator()(const Substitution<Binders...>& substitution) const {
    return substitution[*this];
  }
};

// C++ treats each lambda as a distinct type. By adding a lambda into the template
// parameters, each Symbol will also be a distinct type. Further when clang pretty
// prints the Symbol type it will include the source file line and offset. This
// is convenient for sorting types.
#define SYMBOL() Symbol<[]{}>{}

template <auto kLhs, auto kRhs>
constexpr auto operator==(const Symbol<kLhs>&, const Symbol<kRhs>&) {
  return kLhs == kRhs;
}

template <auto kLhs, auto kRhs>
constexpr auto operator!=(const Symbol<kLhs>&, const Symbol<kRhs>&) {
  return kLhs != kRhs;
}

template <auto kId>
inline constexpr bool internal::SymbolicTypeTrait<Symbol<kId>> = true;

// --- Symbolic Expressions ---

template <typename Op, Symbolic... Args>
struct SymbolicExpression {
  template<BinderT... Binders>
  constexpr auto operator()(const Substitution<Binders...>& substitution) {
    return Op{}(Args{}(substitution)...);
  }
};

// --- Matcher Expressions ---

template <typename Op, Matcher... Matchers>
struct MatcherExpression {};

// --- Display ---

// Symbols cannot be displayed and must be substituted for string constants
// before converting an expression to a string.

template <auto VALUE>
constexpr auto toCTS(const Constant<VALUE>&) {
  return CompileTimeString(VALUE);
}

enum class DisplayMode {
  kInfix,
  kPrefix,
};

template <typename>
struct OpDisplayPolicy {
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr CompileTimeString symbol = "UNKNOWN"_cts;
};

}  // namespace tempura::symbolic
