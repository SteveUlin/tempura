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

// Binds a value to a type for compile-time symbol substitution.
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

// Type trait for matcher types.
template <typename T>
inline constexpr bool MatcherTypeTrait = false;

// Type trait for symbolic types.
template <typename T>
inline constexpr bool SymbolicTypeTrait = false;

}  // namespace internal

// A Matcher is a pattern that can be used in expression matching.
template <typename T>
concept Matcher =
    internal::MatcherTypeTrait<T> || internal::SymbolicTypeTrait<T>;

// A Symbolic is a concrete symbolic expression that can be evaluated.
template <typename T>
concept Symbolic = Matcher<T> && internal::SymbolicTypeTrait<T>;

// A collection of symbol-value bindings for expression evaluation.
template <typename... Binders>
struct Substitution : Binders... {
  template <typename... Ts, typename... Us>
  constexpr Substitution(auto&&... binders)
      : Binders(std::forward<decltype(binders)>(binders))... {}

  using Binders::operator[]...;
};

template <typename... Ts, typename... Us>
Substitution(internal::TypeValueBinder<Ts, Us>...)
    -> Substitution<internal::TypeValueBinder<Ts, Us>...>;

// Symbolic constant with compile-time value.
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

// Parse integer from character pack at compile time.
template <char... chars>
constexpr auto toInt() {
  return [] constexpr {
    int value = 0;
    ((value = value * 10 + (chars - '0')), ...);
    return value;
  }();
}

// Parse floating-point from character pack at compile time.
template <char... chars>
constexpr auto toDouble() {
  return [] constexpr {
    double value = 0.;
    double fraction = 1.;
    bool is_fraction = false;
    (
        [&] {
          if constexpr (chars == '.') {
            is_fraction = true;
          } else {
            fraction = is_fraction ? fraction / 10 : fraction;
            value = value * 10 + (chars - '0');
          }
        }(),
        ...);
    return value * fraction;
  }();
}

template <char... chars>
constexpr auto count(char c) {
  return ((c == chars) + ...);
}

// User-defined literal for creating compile-time constants: 123_c or 3.14_c
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

// Helper for compile-time string storage.
template <size_t N>
struct StringLiteral {
  constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }
  char value[N];
};

// A symbolic variable with a unique compile-time identifier.
template <StringLiteral label, auto id>
struct Symbol {
  constexpr static auto kLabel = label;
  constexpr static auto kId = id;

  // Create a binding for this symbol.
  template <typename T>
  constexpr auto operator=(T&& t) const {
    return internal::TypeValueBinder<Symbol<label, id>, T>{std::forward<T>(t)};
  };

  // Evaluate the symbol given a substitution.
  template <typename... Ts, typename... Us>
  constexpr auto operator()(
      const Substitution<internal::TypeValueBinder<Ts, Us>...>& substitution)
      const {
    return substitution[*this];
  }
};

// Macro to define a single symbol with unique ID.
#define TEMPURA_SYMBOL(VAR) tempura::symbolic::Symbol<#VAR, __COUNTER__> VAR{};

// Preprocessor expansion helpers for variadic macro support.
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

// Macro to define multiple symbols at once.
#define TEMPURA_SYMBOLS(...) TEMPURA_FOR_EACH(TEMPURA_SYMBOL, __VA_ARGS__)

template <StringLiteral label, auto id>
inline constexpr bool internal::SymbolicTypeTrait<Symbol<label, id>> = true;

// Display mode for operator pretty-printing.
enum class DisplayMode {
  kInfix,   // e.g., x + y
  kPrefix,  // e.g., sin(x)
};

// An Operator is callable and provides display metadata.
template <typename T>
concept Operator = requires {
  []<typename... Args>(T t) { t(Args{}...); };
  requires std::is_same_v<decltype(T::kDisplayMode), const DisplayMode>;
  requires std::is_same_v<decltype(T::kSymbol), const std::string_view>;
};

consteval auto operator==(Operator auto lhs, Operator auto rhs) -> bool {
  return std::is_same_v<decltype(lhs), decltype(rhs)>;
}

// A symbolic expression combining an operator with operands.
template <Operator Op, Symbolic... Args>
struct SymbolicExpression {
  constexpr SymbolicExpression() = default;
  constexpr SymbolicExpression(const SymbolicExpression&) = default;
  constexpr auto operator=(const SymbolicExpression&)
      -> SymbolicExpression& = default;

  constexpr static auto op() { return Op{}; }
  constexpr static auto terms() { return TypeList<Args...>{}; }

  // For unary expressions.
  constexpr static auto operand()
    requires(sizeof...(Args) == 1)
  {
    return terms().head();
  }

  // For binary expressions.
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

  // Evaluate with symbol substitution.
  template <typename... Ts, typename... Us>
  static constexpr auto operator()(
      const Substitution<internal::TypeValueBinder<Ts, Us>...>& substitution) {
    return op()(Args{}(substitution)...);
  }

  template <typename... Ts, typename... Us>
  static constexpr auto operator()(
      internal::TypeValueBinder<Ts, Us>&... binders) {
    return SymbolicExpression::operator()(Substitution{binders...});
  }
};

template <Operator Op, Symbolic... Args>
inline constexpr bool
    internal::SymbolicTypeTrait<SymbolicExpression<Op, Args...>> = true;

// Type-based equality for constant values.
template <auto LHS, auto RHS>
constexpr auto operator==(Constant<LHS> /*unused*/, Constant<RHS> /*unused*/)
    -> bool {
  return LHS == RHS;
}

// Type-based equality for symbolic expressions.
constexpr auto operator==(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return std::is_same_v<decltype(lhs), decltype(rhs)>;
}

constexpr auto operator!=(Symbolic auto lhs, Symbolic auto rhs) -> bool {
  return !(lhs == rhs);
}

// Ordering based on type names for canonical forms.
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
  constexpr auto operator=(const MatcherExpression&)
      -> MatcherExpression& = default;

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
