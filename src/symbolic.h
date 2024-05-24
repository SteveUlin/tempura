#pragma once

#include "type_list.h"

#include <cassert>
#include <concepts>
#include <format>
#include <string>
#include <sstream>
#include <strstream>
#include <tuple>
#include <type_traits>

namespace tempura::symbolic {

using namespace std::string_view_literals;

// Compile-time symbolic algebra library.

// Quite a bit different from but inspired by:
// Vincent Reverdy - CppCon https://www.youtube.com/watch?v=lPfA4SFojaok

namespace internal {

template <typename T>
struct SymbolicTrait {
  static constexpr bool value = false;
};

} // namespace

template <typename T>
concept Symbolic = internal::SymbolicTrait<T>::value;

// Substitution defines how to replace symbols type with a value.
template <typename Binder>
class SubstitutionElement {
 public:
  constexpr SubstitutionElement(Binder binder)
      : binder_(binder) {}

  constexpr auto operator[](Binder::SymbolType) const -> const auto& {
    return binder_.value();
  }

 private:
  Binder binder_;
};

// A substitution is a collection of bindings of symbols to values.
template <typename... Binders>
class Substitution : SubstitutionElement<Binders>... {
public:
  constexpr Substitution(Binders... binders)
      : SubstitutionElement<Binders>(binders)... {}

  using SubstitutionElement<Binders>::operator[]...;
};

// Kinds of symbols.
//
// A symbol allows certain types to "bind" to it. We use type traits to
// determine if a type can bind to a symbol.
template <typename T>
struct Unconstrined : std::true_type {};

template <typename T>
struct Real : std::is_floating_point<T> {};

// Automatically include a lambda in each instantiation of a Symbol.
// This forces each instantiation to be a unique type.
template <template <typename> typename Constraint = Unconstrined,
          auto UNIQUE = [] {}>
class Symbol {
public:
  // Bind the symbol with a provided value
  template <typename T>
  requires Constraint<std::remove_cvref<T>>::value
  class Binder {
  public:
    using StorageType = T;
    using SymbolType = Symbol;
    using ValueType = std::remove_cvref<T>::type;

    constexpr Binder(T value) : value_(std::forward<T>(value)) {}

    constexpr auto value() const -> const ValueType& { return value_; }
    constexpr auto operator()() const -> const ValueType& { return value_; }

    static constexpr auto symbol() -> Symbol {
      return Symbol{};
    }

  private:
    T value_;
  };

  template<typename T>
  constexpr auto bind(T&& value) const -> Binder<T> {
    return Binder<T>(std::forward<T>(value));
  }

  template<typename T>
  constexpr auto operator=(T&& value) const -> Binder<T> {
    return Binder<T>(std::forward<T>(value));
  }

  template <typename... Binders>
  constexpr auto
  operator()(const Substitution<Binders...> &substitution) const -> const auto& {
    return substitution[*this];
  }
};

template <template <typename> typename Constraint, auto ID>
struct internal::SymbolicTrait<Symbol<Constraint, ID>> {
  static constexpr bool value = true;
};

template <auto Value>
struct Constant {
  using Type = decltype(Value);
  constexpr Constant() = default;

  static constexpr auto value = Value;

  template <typename... Binders>
  constexpr auto
  operator()(const Substitution<Binders...>&) const {
    return value;
  }
};

template <auto Value>
struct internal::SymbolicTrait<Constant<Value>> {
  static constexpr bool value = true;
};

template <typename Action, Symbolic... Terms>
struct SymbolicExpression {
  constexpr SymbolicExpression() = default;
  constexpr SymbolicExpression(Action, Terms...) {};

  constexpr auto terms() -> TypeList<Terms...> {
    return {};
  }

  constexpr auto action() -> Action {
    return {};
  }

  template <typename... Binders>
  constexpr auto
  operator()(const Substitution<Binders...> &substitution) const {
    return Action{}(Terms{}(substitution)...);
  }
};

template <typename Operator, Symbolic... Terms>
struct internal::SymbolicTrait<SymbolicExpression<Operator, Terms...>> {
  static constexpr bool value = true;
};

enum class DisplayMode {
  kInfix,
  kPrefix,
};

template <typename>
struct DisplayPolicy {
  DisplayMode mode = DisplayMode::kPrefix;
  std::string_view symbol = "UNKNOWN"sv;
};

struct Plus {
  template <typename... Args>
  constexpr auto
  operator()(Args&&... args) const {
    return (0 + ... + args);
  }
};

template <>
struct DisplayPolicy<Plus> {
  DisplayMode mode = DisplayMode::kInfix;
  std::string_view symbol = "+"sv;
};

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto operator+(Lhs, Rhs) -> SymbolicExpression<Plus, Lhs, Rhs> {
  return {};
}

struct Minus {
  template <typename... Args>
  constexpr auto
  operator()(Args&&... args) const {
    return (args - ...);
  }
};

template <>
struct DisplayPolicy<Minus> {
  DisplayMode mode = DisplayMode::kInfix;
  std::string_view symbol = "-"sv;
};

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto operator-(Lhs, Rhs) -> SymbolicExpression<Minus, Lhs, Rhs> {
  return {};
}

struct Multiply {
  template <typename... Args>
  constexpr auto
  operator()(Args&&... args) const {
    return (1 * ... * args);
  }
};

template <>
struct DisplayPolicy<Multiply> {
  DisplayMode mode = DisplayMode::kInfix;
  std::string_view symbol = "*"sv;
};

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto operator*(Lhs, Rhs) -> SymbolicExpression<Multiply, Lhs, Rhs> {
  return {};
}

template <>
struct DisplayPolicy<std::divides<void>> {
  DisplayMode mode = DisplayMode::kInfix;
  std::string_view symbol = "/"sv;
};

template <Symbolic Lhs, Symbolic Rhs>
constexpr auto operator/(Lhs, Rhs) -> SymbolicExpression<std::divides<void>, Lhs, Rhs> {
  return {};
}

struct SinSymbol {
  template <typename Arg>
  constexpr auto operator()(Arg x) {
    return std::sin(x);
  }
};

template <>
struct DisplayPolicy<SinSymbol> {
  DisplayMode mode = DisplayMode::kPrefix;
  std::string_view symbol = "sin"sv;
};

template <Symbolic Arg>
constexpr auto sin(Arg) -> SymbolicExpression<SinSymbol, Arg> {
  return {};
}

struct CosSymbol {
  template <typename Arg>
  constexpr auto operator()(Arg x) {
    return std::cos(x);
  }
};

template <>
struct DisplayPolicy<CosSymbol> {
  DisplayMode mode = DisplayMode::kPrefix;
  std::string_view symbol = "cos"sv;
};

template <Symbolic Arg>
constexpr auto cos(Arg) -> SymbolicExpression<SinSymbol, Arg> {
  return {};
}

struct Any {};
template <>
struct internal::SymbolicTrait<Any> {
  static constexpr bool value = true;
};

struct AnyNTerms {};
template <>
struct internal::SymbolicTrait<AnyNTerms> {
  static constexpr bool value = true;
};
struct AnyConstant {};
template <>
struct internal::SymbolicTrait<AnyConstant> {
  static constexpr bool value = true;
};
struct AnySymbol {};
template <>
struct internal::SymbolicTrait<AnySymbol> {
  static constexpr bool value = true;
};
struct AnySymbolicExpression {};
template <>
struct internal::SymbolicTrait<AnySymbolicExpression> {
  static constexpr bool value = true;
};

template <Symbolic Lhs, Symbolic Rhs>
consteval auto match(Lhs, Rhs) -> bool {
  return false;
}

consteval auto match(Any, Any) -> bool {
  return true;
}

template <Symbolic Lhs>
consteval auto match(Lhs, Any) -> bool {
  return true;
}

template <Symbolic Rhs>
consteval auto match(Any, Rhs) -> bool {
  return true;
}

template <auto RhsValue>
consteval auto match(AnyConstant, Constant<RhsValue>) -> bool {
  return true;
}

template <auto LhsValue>
consteval auto match(Constant<LhsValue>, AnyConstant) -> bool {
  return true;
}

template <template <typename> typename Constraint, auto ID>
consteval auto match(Symbol<Constraint, ID>, AnySymbol) -> bool {
  return true;
}

template <template <typename> typename Constraint, auto ID>
consteval auto match(AnySymbol, Symbol<Constraint, ID>) -> bool {
  return true;
}

template <typename OperatorLhs, Symbolic... TermsLhs>
consteval auto match(SymbolicExpression<OperatorLhs, TermsLhs...>, AnySymbolicExpression) -> bool {
  return true;
}

template <typename OperatorRhs, Symbolic... TermsRhs>
consteval auto match(AnySymbolicExpression, SymbolicExpression<OperatorRhs, TermsRhs...>) -> bool {
  return true;
}

template <typename OperatorLhs, Symbolic... TermsLhs,
          typename OperatorRhs, Symbolic... TermsRhs>
consteval auto match(SymbolicExpression<OperatorLhs, TermsLhs...>,
                     SymbolicExpression<OperatorRhs, TermsRhs...>) -> bool {
  if constexpr (!std::is_same_v<OperatorLhs, OperatorRhs>) {
    // Operators must match
    return false;
  } else if constexpr (std::is_same_v<TypeList<AnyNTerms>, TypeList<TermsRhs...>> or
                       std::is_same_v<TypeList<AnyNTerms>, TypeList<TermsLhs...>>) {
    return true;
  } else if constexpr (sizeof...(TermsLhs) != sizeof...(TermsRhs)) {
    return false;
  } else {
    return (match(TermsLhs{}, TermsRhs{}) && ...);
  }
}

template <auto LhsValue, auto RhsValue>
consteval auto match(Constant<LhsValue> lhs,
                     Constant<RhsValue> rhs) -> bool {
  return lhs.value == rhs.value;
}

template <template <typename> typename Constraint, auto ID>
consteval auto match(Symbol<Constraint, ID>,
                     Symbol<Constraint, ID>) -> bool {
  return true;
}

template <typename Action, Symbolic... Terms>
consteval auto flattenImpl(Action action, TypeList<Terms...> terms) {
  if constexpr (sizeof...(Terms) == 0) {
    return TypeList<>{};

  } else if constexpr (match(terms.head(),
                             SymbolicExpression<Action, AnyNTerms>{})) {
    return flattenImpl(action, concat(terms.head().terms(), terms.tail()));
  } else {
    return flattenImpl(action, terms.tail()) >>= [&](Symbolic auto... rest) {
        return TypeList<decltype(terms.head()), decltype(rest)...>{};
    };
  }
}
template <typename Action, Symbolic... Terms>
consteval auto flatten(SymbolicExpression<Action, Terms...> expr) {
  return flattenImpl(Action{}, expr.terms()) >>= [&](auto... args) {
    return SymbolicExpression<Action, decltype(args)...>{};
  };
}

constexpr auto shouldWrap(Symbolic auto expr) -> bool {
  if constexpr (!match(expr, AnySymbolicExpression{})) {
    return false;
  } else {
    return getDisplayPolicy(expr.action()).mode == DisplayMode::kInfix;
  }
}

template<typename... Binders>
void show(Symbolic auto expr, const Substitution<Binders...>& substitution) {
  if constexpr (match(expr, AnySymbol{})) {
    std::print("{}", substitution[expr.id()]);

  } else if constexpr (match(expr, AnyConstant{})) {
    if constexpr (std::is_floating_point_v<decltype(expr.value)>) {
      std::print("{:.2f}", expr.value);
    } else {
      std::print("{}", expr.value);
    }

  } else if constexpr (match(expr, AnySymbolicExpression{}) &&
                       getDisplayPolicy(expr.action()).mode ==
                           DisplayMode::kInfix) {
    if (expr.terms().size() == 0) {
      std::print("{}", getDisplayPolicy(expr.action()).symbol);
      return;
    }
    show(expr.terms().head(), substitution);
    expr.terms().tail() >>= [&](auto... args) {
      ((std::print(" {} ", getDisplayPolicy(expr.action()).symbol),
        (std::print(shouldWrap(args) ? "(" : "")),
        show(args, substitution),
        (std::print(shouldWrap(args) ? ")" : ""))),
       ...);
    };

  } else if constexpr (match(expr, AnySymbolicExpression{}) &&
                       getDisplayPolicy(expr.action()).mode ==
                           DisplayMode::kPrefix) {
    std::print("{}(", getDisplayPolicy(expr.action()).symbol);
    show(expr.terms().head(), substitution);
    expr.terms().tail() >>= [&](auto... args) {
      ((std::print(" "), show(args, substitution)), ...);
    };
    std::print(")");

  } else {
    assert(false);
  }
}

} // namespace tempura::symbolic


