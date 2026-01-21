#pragma once

#include <format>
#include <string>
#include <tuple>

#include "meta/function_objects.h"
#include "symbolic4/core.h"
#include "symbolic4/let.h"
#include "symbolic4/operators.h"
#include "symbolic4/scheme/fold.h"

// ============================================================================
// to_string.h - Pretty-printing symbolic expressions
// ============================================================================
//
// This interpreter converts symbolic expressions to human-readable strings.
// It handles operator precedence and parenthesization automatically.
//
// Usage:
//   Symbol<struct X> x;
//   Symbol<struct Y> y;
//   auto expr = x * y + one;
//   std::string str = toString(expr, name(x, "x"), name(y, "y"));
//   // str == "x * y + 1"
//
// The Binder Model:
//
// Unlike evaluation where symbols carry numeric values, toString needs STRING
// names for symbols. Since Symbol types have no runtime name (just a unique
// Id type), we use a "binder" pattern:
//
//   name(x, "x")  // Creates a StringBinding mapping x's Id → "x"
//
// Multiple bindings are packed into a StringBinderPack, similar to how
// evaluation uses BinderPack for numeric values.
//
// Precedence and Parenthesization:
//
// The interpreter tracks operator precedence to minimize parentheses while
// preserving correctness. Lower precedence operations need parens when
// nested inside higher precedence ones:
//
//   (x + y) * z  →  "(x + y) * z"  (addition is lower than multiplication)
//   x * y + z    →  "x * y + z"    (no parens needed)
//
// Non-associative operators (-, /) also get special handling for their
// right operands to preserve mathematical meaning:
//
//   x - (y - z)  →  "x - (y - z)"  (parens needed to preserve grouping)
//
// DisplayTraits:
//
// Each operator has associated display metadata:
//   - symbol: The string representation ("+", "sin", "π")
//   - mode: Prefix ("sin(x)") or infix ("x + y")
//   - precedence: For parenthesization decisions
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Precedence levels (higher = binds more tightly)
// ============================================================================
//
// Standard mathematical precedence:
//   50 (Atomic)         - Symbols, constants: x, 42, π
//   40 (Unary)          - Negation, functions: -x, sin(x)
//   30 (Power)          - Exponentiation: x^2
//   20 (Multiplication) - Multiply, divide: x * y, x / y
//   10 (Addition)       - Add, subtract: x + y, x - y
//
// Lower precedence operations need parentheses inside higher ones.

namespace Precedence {
constexpr int kAddition = 10;
constexpr int kMultiplication = 20;
constexpr int kPower = 30;
constexpr int kUnary = 40;
constexpr int kAtomic = 50;
}  // namespace Precedence

// ============================================================================
// DisplayTraits - presentation metadata for operators
// ============================================================================
//
// Each operator has a specialization defining how it should be displayed:
//   - symbol: The text representation ("+", "sin", "π")
//   - mode: kInfix for binary ops (a + b), kPrefix for functions (sin(x))
//   - precedence: For parenthesization decisions

enum class DisplayMode { kPrefix, kInfix };

template <typename Op>
struct DisplayTraits {
  static constexpr const char* symbol = "?";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = 0;
};

// Binary arithmetic
template <>
struct DisplayTraits<AddOp> {
  static constexpr const char* symbol = "+";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kAddition;
};

template <>
struct DisplayTraits<SubOp> {
  static constexpr const char* symbol = "-";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kAddition;
};

template <>
struct DisplayTraits<MulOp> {
  static constexpr const char* symbol = "*";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kMultiplication;
};

template <>
struct DisplayTraits<DivOp> {
  static constexpr const char* symbol = "/";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kMultiplication;
};

template <>
struct DisplayTraits<PowOp> {
  static constexpr const char* symbol = "^";
  static constexpr DisplayMode mode = DisplayMode::kInfix;
  static constexpr int precedence = Precedence::kPower;
};

// Unary
template <>
struct DisplayTraits<NegOp> {
  static constexpr const char* symbol = "-";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Trigonometric
template <>
struct DisplayTraits<SinOp> {
  static constexpr const char* symbol = "sin";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CosOp> {
  static constexpr const char* symbol = "cos";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<TanOp> {
  static constexpr const char* symbol = "tan";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Hyperbolic
template <>
struct DisplayTraits<SinhOp> {
  static constexpr const char* symbol = "sinh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<CoshOp> {
  static constexpr const char* symbol = "cosh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<TanhOp> {
  static constexpr const char* symbol = "tanh";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Exponential/logarithmic
template <>
struct DisplayTraits<ExpOp> {
  static constexpr const char* symbol = "exp";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<LogOp> {
  static constexpr const char* symbol = "log";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

template <>
struct DisplayTraits<SqrtOp> {
  static constexpr const char* symbol = "sqrt";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kUnary;
};

// Constants
template <>
struct DisplayTraits<PiOp> {
  static constexpr const char* symbol = "π";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kAtomic;
};

template <>
struct DisplayTraits<EOp> {
  static constexpr const char* symbol = "e";
  static constexpr DisplayMode mode = DisplayMode::kPrefix;
  static constexpr int precedence = Precedence::kAtomic;
};

// ============================================================================
// Type-based unique ID generation (works with incomplete types)
// ============================================================================
//
// Used for LetNode display to generate stable numeric IDs from type identities.

namespace detail {
template <typename T>
struct TypeIdHolder {
  static constexpr char id = 0;  // Address used as unique identifier
};

template <typename T>
constexpr auto typeId() -> SizeT {
  return reinterpret_cast<SizeT>(&TypeIdHolder<T>::id);
}
}  // namespace detail

// ============================================================================
// StringResult - carries string + precedence for parenthesization
// ============================================================================
//
// The fold result type. Carries both the string representation AND the
// precedence of the outermost operation, enabling correct parenthesization.

struct StringResult {
  std::string str;
  int precedence = Precedence::kAtomic;

  // Wrap in parentheses if this expression's precedence is lower than parent's.
  // Used for left operands and associative operators.
  auto wrap(int parent_prec) const -> std::string {
    if (precedence < parent_prec) {
      return "(" + str + ")";
    }
    return str;
  }

  // For RIGHT operands of non-associative operators (-, /):
  // wrap if precedence <= parent to preserve mathematical meaning.
  // Example: x - (y - z) needs parens even though same precedence.
  auto wrapRight(int parent_prec) const -> std::string {
    if (precedence <= parent_prec) {
      return "(" + str + ")";
    }
    return str;
  }
};

// ============================================================================
// String binding: name(sym, "str") syntax
// ============================================================================
//
// The binder pattern for symbol names. Each StringBinding maps a symbol's
// Id type to a display string. Multiple bindings are packed into a
// StringBinderPack for lookup during toString traversal.
//
// Usage:
//   name(x, "x")              // x displays as "x"
//   name(theta, "θ")          // theta displays as "θ"
//   name(my_var, "my_var")    // Full names work too

template <typename Id>
struct StringBinding {
  using id_type = Id;
  std::string str;
};

// name() function creates StringBinding from symbol and string
template <typename Id>
auto name(Atom<Id, Lookup>, const char* str) -> StringBinding<Id> {
  return StringBinding<Id>{std::string(str)};
}

template <typename Id>
auto name(Atom<Id, Lookup>, std::string str) -> StringBinding<Id> {
  return StringBinding<Id>{std::move(str)};
}

// StringBinderPack: holds all name bindings, provides lookup<Id>()
// Similar to BinderPack but for string names instead of numeric values.
template <typename... Bindings>
struct StringBinderPack {
  std::tuple<Bindings...> bindings;

  // Lookup the display name for a symbol with the given Id type
  template <typename Id>
  auto lookup() const -> std::string {
    return lookupImpl<Id>(std::make_index_sequence<sizeof...(Bindings)>{});
  }

 private:
  // Search bindings for matching Id, return "?" if not found
  template <typename Id, SizeT... Is>
  auto lookupImpl(std::index_sequence<Is...>) const -> std::string {
    std::string result = "?";
    auto try_match = [&]<SizeT I>() {
      using B = std::tuple_element_t<I, std::tuple<Bindings...>>;
      if constexpr (std::is_same_v<typename B::id_type, Id>) {
        result = std::get<I>(bindings).str;
      }
    };
    (try_match.template operator()<Is>(), ...);
    return result;
  }
};

// ============================================================================
// ToString Interpreter
// ============================================================================
//
// The fold interpreter that converts expressions to strings.
// Uses DisplayTraits to determine how each operator should be formatted.

template <typename Bindings>
struct ToString {
  using result_type = StringResult;
  using context_type = Bindings;

  // Terminals
  template <typename T>
  static auto terminal(T, context_type& ctx) -> StringResult {
    if constexpr (is_constant_v<T>) {
      if constexpr (std::is_integral_v<decltype(T::value)>) {
        return {std::format("{}", T::value), Precedence::kAtomic};
      } else {
        return {std::format("{:.6g}", T::value), Precedence::kAtomic};
      }
    } else if constexpr (is_fraction_v<T>) {
      if constexpr (T::denominator == 1) {
        return {std::format("{}", T::numerator), Precedence::kAtomic};
      } else {
        return {std::format("{}/{}", T::numerator, T::denominator),
                Precedence::kMultiplication};
      }
    } else if constexpr (is_literal_v<T>) {
      return {std::format("{}", T{}.strategy_.value), Precedence::kAtomic};
    } else if constexpr (is_atom_v<T>) {
      // Symbol - lookup name from bindings
      return {ctx.template lookup<typename T::id_type>(), Precedence::kAtomic};
    } else {
      return {"?", Precedence::kAtomic};
    }
  }

  // Combine child results
  template <typename Op, typename... Rs>
  static auto combine(context_type&, Op, Rs... children) -> StringResult {
    constexpr auto mode = DisplayTraits<Op>::mode;
    constexpr auto prec = DisplayTraits<Op>::precedence;
    const char* sym = DisplayTraits<Op>::symbol;

    if constexpr (sizeof...(Rs) == 0) {
      // Nullary (pi, e)
      return {sym, prec};
    } else if constexpr (sizeof...(Rs) == 1) {
      // Unary
      auto [child] = std::tuple{children...};
      if (mode == DisplayMode::kPrefix) {
        // Function-style: sin(x)
        return {std::format("{}({})", sym, child.str), prec};
      } else {
        // Prefix operator: -x
        return {std::format("{}{}", sym, child.wrap(prec)), prec};
      }
    } else if constexpr (sizeof...(Rs) == 2) {
      // Binary - handle associativity properly
      auto [left, right] = std::tuple{children...};
      if (mode == DisplayMode::kInfix) {
        // For non-associative operators (-, /), right operand needs parens at equal precedence
        constexpr bool is_non_associative =
            std::is_same_v<Op, SubOp> || std::is_same_v<Op, DivOp>;
        std::string right_str =
            is_non_associative ? right.wrapRight(prec) : right.wrap(prec);
        return {std::format("{} {} {}", left.wrap(prec), sym, right_str), prec};
      }
      // Prefix with two args: op(a, b)
      return {std::format("{}({}, {})", sym, left.str, right.str), prec};
    } else {
      // More than binary
      if (mode == DisplayMode::kInfix) {
        std::string result;
        bool first = true;
        auto append = [&](const StringResult& r) {
          if (!first) {
            result += std::format(" {} ", sym);
          }
          result += r.wrap(prec);
          first = false;
        };
        (append(children), ...);
        return {result, prec};
      }
      // Prefix with multiple args: op(a, b, ...)
      std::string result = std::format("{}(", sym);
      bool first = true;
      auto append = [&](const StringResult& r) {
        if (!first) result += ", ";
        result += r.str;
        first = false;
      };
      (append(children), ...);
      result += ")";
      return {result, prec};
    }
  }
};

// ============================================================================
// Convenience function: toString(expr, name(x, "x"), name(y, "y"), ...)
// ============================================================================
//
// The main user-facing API. Converts an expression to a string.
//
// Examples:
//   toString(x + y, name(x, "x"), name(y, "y"))           // "x + y"
//   toString(x * x + one, name(x, "α"))                   // "α * α + 1"
//   toString(sin(x) * cos(x), name(x, "θ"))               // "sin(θ) * cos(θ)"
//   toString(x - (y - x), name(x, "x"), name(y, "y"))     // "x - (y - x)"

template <Symbolic E, typename... Bindings>
auto toString(E expr, Bindings... bindings) -> std::string {
  using Pack = StringBinderPack<Bindings...>;
  using ToStringType = ToString<Pack>;
  Pack ctx{std::tuple{bindings...}};
  return fold<ToStringType>(expr, ctx).str;
}

// ============================================================================
// LetNode support - show as "let_NNN = expr"
// ============================================================================
//
// LetNodes display with a generated numeric ID (from their type identity).

template <typename Id, Symbolic E, typename... Bindings>
auto toString(LetNode<Id, E> node, Bindings... bindings) -> std::string {
  SizeT id = detail::typeId<Id>();
  return std::format("let_{} = {}", id % 1000, toString(node.expr(), bindings...));
}

}  // namespace tempura::symbolic4
