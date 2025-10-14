#pragma once

#include <cmath>
#include <cstdio>
#include <format>
#include <string>

#include "meta/function_objects.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"

// String conversion and debugging utilities for symbolic3 expressions
// Migrated from symbolic2/to_string.h with symbolic3 adaptations

namespace tempura::symbolic3 {

// =============================================================================
// COMPILE-TIME STRING CONVERSION (StaticString-based)
// =============================================================================

// Integer constants - recursive implementation
template <int N>
constexpr auto toStringImpl(Constant<N>) {
  if constexpr (N == 0) {
    return StaticString("");
  } else {
    auto val = N % 10;
    return toStringImpl(Constant<N / 10>{}) +
           StaticString(static_cast<char>('0' + val));
  }
}

template <int N>
constexpr auto toString(Constant<N>) {
  if constexpr (N == 0) {
    return StaticString("0");
  } else if constexpr (N < 0) {
    return StaticString("-") + toStringImpl(Constant<-N>{});
  } else {
    return toStringImpl(Constant<N>{});
  }
}

// Double constants - fractional part conversion
template <double Orig, double VALUE>
constexpr auto toStringFraction(Constant<VALUE>) {
  if constexpr (VALUE / Orig < 0.000001) {
    return StaticString("");
  } else {
    constexpr auto val = VALUE * 10;
    constexpr auto digit = floor(val);
    return StaticString(static_cast<char>('0' + static_cast<int>(digit))) +
           toStringFraction<Orig>(Constant<val - digit>{});
  }
}

template <double VALUE>
constexpr auto toString(Constant<VALUE>) {
  if constexpr (VALUE == 0.0) {
    return StaticString("0.0");
  } else if constexpr (VALUE < 0.0) {
    return StaticString("-") + toString(Constant<-VALUE>{});
  } else {
    return toString(Constant<static_cast<int>(VALUE)>{}) + StaticString(".") +
           toStringFraction<VALUE>(Constant<VALUE - static_cast<int>(VALUE)>{});
  }
}

// Generic constant fallback
template <auto VALUE>
constexpr auto toString(Constant<VALUE>) {
  return StaticString("<Constant>");
}

// Symbol conversion - show type ID for debugging
template <typename SymbolTag>
constexpr auto toString(Symbol<SymbolTag>) {
  return StaticString("x") +
         toString(Constant<static_cast<int>(kMeta<Symbol<SymbolTag>>)>{});
}

// Fraction conversion - show as "N/D"
template <long long N, long long D>
constexpr auto toString(Fraction<N, D>) {
  if constexpr (D == 1) {
    return toString(Constant<N>{});
  } else {
    return toString(Constant<N>{}) + StaticString("/") +
           toString(Constant<D>{});
  }
}

// =============================================================================
// EXPRESSION PRINTING - Uses operator metadata directly from operators
// =============================================================================
// All operators now have kSymbol and kDisplayMode built-in!

// Expression conversion - supports both infix and prefix notation
// Helper for infix printing
namespace detail {
template <typename Op, Symbolic First, Symbolic... Rest>
constexpr auto toStringInfixImpl() {
  constexpr auto sym = Op::kSymbol;
  if constexpr (sizeof...(Rest) == 0) {
    return toString(First{});
  } else {
    return toString(First{}) +
           ((StaticString(" ") + sym + StaticString(" ") + toString(Rest{})) +
            ...);
  }
}
}  // namespace detail

template <typename Op, Symbolic... Args>
constexpr auto toString(Expression<Op, Args...>) {
  if constexpr (Op::kDisplayMode == DisplayMode::kPrefix) {
    // Prefix: op(arg1, arg2, ...)
    return Op::kSymbol + StaticString("(") +
           ((StaticString(" ") + toString(Args{})) + ... + StaticString(")"));
  } else {
    // Infix: (arg1 op arg2 op ...)
    if constexpr (sizeof...(Args) == 1) {
      // Unary operator (like negation)
      return Op::kSymbol + StaticString("(") + (toString(Args{}) + ...) +
             StaticString(")");
    } else {
      // Binary/n-ary operator
      return StaticString("(") + detail::toStringInfixImpl<Op, Args...>() +
             StaticString(")");
    }
  }
}

// =============================================================================
// RUNTIME STRING CONVERSION (std::string-based for debugging)
// =============================================================================

// Runtime integer conversion
inline std::string toStringRuntime(int val) { return std::format("{}", val); }

// Runtime double conversion
inline std::string toStringRuntime(double val) {
  return std::format("{}", val);
}

// Runtime constant conversion
template <auto VALUE>
inline std::string toStringRuntime(Constant<VALUE>) {
  if constexpr (std::integral<decltype(VALUE)>) {
    return std::format("{}", VALUE);
  } else if constexpr (std::floating_point<decltype(VALUE)>) {
    return std::format("{:.6g}", VALUE);
  } else {
    return "<Constant>";
  }
}

// Runtime symbol conversion
template <typename SymbolTag>
inline std::string toStringRuntime(Symbol<SymbolTag>) {
  return std::format("x{}", static_cast<int>(kMeta<Symbol<SymbolTag>>));
}

// Runtime fraction conversion
template <long long N, long long D>
inline std::string toStringRuntime(Fraction<N, D>) {
  if constexpr (D == 1) {
    return std::format("{}", N);
  } else {
    return std::format("{}/{}", N, D);
  }
}

// Runtime expression conversion
template <typename Op, Symbolic... Args>
inline std::string toStringRuntime(Expression<Op, Args...>) {
  // Use operator metadata directly
  std::string opSymbol = std::string(Op::kSymbol.c_str());

  if constexpr (Op::kDisplayMode == DisplayMode::kPrefix) {
    // Prefix: op(arg1, arg2, ...)
    std::string result = opSymbol + "(";
    bool first = true;
    (
        [&]() {
          if (!first) result += ", ";
          result += toStringRuntime(Args{});
          first = false;
        }(),
        ...);
    result += ")";
    return result;
  } else {
    // Infix: (arg1 op arg2 op ...)
    if constexpr (sizeof...(Args) == 1) {
      // Unary operator
      return opSymbol + "(" + (toStringRuntime(Args{}) + ... + "") + ")";
    } else {
      // Binary/n-ary operator
      std::string result = "(";
      bool first = true;
      (
          [&]() {
            if (!first) result += " " + opSymbol + " ";
            result += toStringRuntime(Args{});
            first = false;
          }(),
          ...);
      result += ")";
      return result;
    }
  }
}

// =============================================================================
// DEBUGGING UTILITIES
// =============================================================================

// Debug print to stdout - uses runtime conversion
template <Symbolic S>
inline void debug_print(S expr, const char* label = nullptr) {
  if (label) {
    std::printf("%s: %s\n", label, toStringRuntime(expr).c_str());
  } else {
    std::printf("%s\n", toStringRuntime(expr).c_str());
  }
}

// Type name extraction for debugging (shows full type)
template <Symbolic S>
inline std::string debug_type_name(S) {
  return __PRETTY_FUNCTION__;
}

// Compact type info - just the expression structure
template <auto val>
inline std::string debug_type_info(Constant<val>) {
  return std::format("Constant<{}>", val);
}

template <typename SymbolTag>
inline std::string debug_type_info(Symbol<SymbolTag>) {
  return std::format("Symbol<{}>", static_cast<int>(kMeta<Symbol<SymbolTag>>));
}

template <typename Op, Symbolic... Args>
inline std::string debug_type_info(Expression<Op, Args...>) {
  std::string opName = [&]() -> std::string {
    if constexpr (std::same_as<Op, AddOp>)
      return "Add";
    else if constexpr (std::same_as<Op, SubOp>)
      return "Sub";
    else if constexpr (std::same_as<Op, MulOp>)
      return "Mul";
    else if constexpr (std::same_as<Op, DivOp>)
      return "Div";
    else if constexpr (std::same_as<Op, PowOp>)
      return "Pow";
    else if constexpr (std::same_as<Op, NegOp>)
      return "Neg";
    else if constexpr (std::same_as<Op, SinOp>)
      return "Sin";
    else if constexpr (std::same_as<Op, CosOp>)
      return "Cos";
    else if constexpr (std::same_as<Op, TanOp>)
      return "Tan";
    else if constexpr (std::same_as<Op, ExpOp>)
      return "Exp";
    else if constexpr (std::same_as<Op, LogOp>)
      return "Log";
    else if constexpr (std::same_as<Op, SqrtOp>)
      return "Sqrt";
    else
      return "Op";
  }();

  std::string result = std::format("Expression<{}", opName);
  ([&]() { result += ", " + debug_type_info(Args{}); }(), ...);
  result += ">";
  return result;
}

// Tree visualization - shows expression structure
template <Symbolic S>
inline void debug_print_tree(S expr, int indent = 0,
                             const char* label = nullptr) {
  std::string prefix(static_cast<std::size_t>(indent * 2), ' ');

  if (label) {
    std::printf("%s%s: ", prefix.c_str(), label);
  } else {
    std::printf("%s", prefix.c_str());
  }

  if constexpr (is_constant<S>) {
    std::printf("Constant: %s\n", toStringRuntime(expr).c_str());
  } else if constexpr (is_symbol<S>) {
    std::printf("Symbol: %s\n", toStringRuntime(expr).c_str());
  } else if constexpr (is_expression<S>) {
    using Op = get_op_t<S>;
    using Args = get_args_t<S>;

    std::printf("Expression: %s\n", toStringRuntime(expr).c_str());

    // Print operator
    std::string opName = [&]() -> std::string {
      if constexpr (std::same_as<Op, AddOp>)
        return "Add (+)";
      else if constexpr (std::same_as<Op, SubOp>)
        return "Sub (-)";
      else if constexpr (std::same_as<Op, MulOp>)
        return "Mul (*)";
      else if constexpr (std::same_as<Op, DivOp>)
        return "Div (/)";
      else if constexpr (std::same_as<Op, PowOp>)
        return "Pow (^)";
      else if constexpr (std::same_as<Op, NegOp>)
        return "Neg (-)";
      else if constexpr (std::same_as<Op, SinOp>)
        return "Sin";
      else if constexpr (std::same_as<Op, CosOp>)
        return "Cos";
      else if constexpr (std::same_as<Op, TanOp>)
        return "Tan";
      else if constexpr (std::same_as<Op, ExpOp>)
        return "Exp";
      else if constexpr (std::same_as<Op, LogOp>)
        return "Log";
      else if constexpr (std::same_as<Op, SqrtOp>)
        return "Sqrt";
      else
        return "Unknown";
    }();
    std::printf("%s  Op: %s\n", prefix.c_str(), opName.c_str());

    // Print arguments
    std::printf("%s  Args:\n", prefix.c_str());
    int argNum = 0;
    []<typename OpT, Symbolic... As>(Expression<OpT, As...> e, int& num,
                                     int ind) {
      (
          [&]() {
            debug_print_tree(As{}, ind + 2, std::format("[{}]", num++).c_str());
          }(),
          ...);
    }(expr, argNum, indent);
  }
}

// Simplified tree view - more compact
template <Symbolic S>
inline void debug_print_compact(S expr, const char* label = nullptr) {
  if (label) {
    std::printf("%s: %s :: %s\n", label, toStringRuntime(expr).c_str(),
                debug_type_info(expr).c_str());
  } else {
    std::printf("%s :: %s\n", toStringRuntime(expr).c_str(),
                debug_type_info(expr).c_str());
  }
}

}  // namespace tempura::symbolic3
