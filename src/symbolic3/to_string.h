#pragma once

#include <cmath>
#include <cstdio>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

#include "meta/function_objects.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/operator_display.h"

// String conversion and debugging utilities for symbolic3 expressions
// Uses DisplayTraits from operator_display.h for operator presentation

namespace tempura::symbolic3 {

// =============================================================================
// COMPILE-TIME STRING CONVERSION (StaticString-based)
// =============================================================================

// Symbol name mapping for custom variable names at compile-time
// Usage: auto ctx = makeSymbolNames(x, "alpha", y, "beta");
//        auto str = toString(expr, ctx);

namespace detail {
// Find the index where a symbol appears in the context (returns -1 if not found)
template <typename SymbolTag, typename CtxTuple, size_t Idx = 0>
constexpr int findSymbolIndex() {
  if constexpr (Idx >= std::tuple_size_v<CtxTuple>) {
    return -1;  // Not found
  } else if constexpr (Idx + 1 < std::tuple_size_v<CtxTuple>) {
    using KeyType = std::tuple_element_t<Idx, CtxTuple>;
    if constexpr (std::same_as<KeyType, Symbol<SymbolTag>>) {
      return static_cast<int>(Idx);  // Found at this index
    } else {
      // Try next pair
      return findSymbolIndex<SymbolTag, CtxTuple, Idx + 2>();
    }
  } else {
    return -1;  // Not enough elements
  }
}
}  // namespace detail

// Create a symbol name context from alternating symbol/name pairs
template <typename... Args>
constexpr auto makeSymbolNames(Args&&... args) {
  static_assert(sizeof...(Args) % 2 == 0,
                "makeSymbolNames requires an even number of arguments (symbol, name pairs)");
  return std::tuple{std::forward<Args>(args)...};
}

// Empty context for default behavior
constexpr auto emptySymbolNames() {
  return std::tuple{};
}

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

// Symbol conversion with custom name context
template <typename SymbolTag, typename... CtxPairs>
constexpr auto toString(Symbol<SymbolTag> s, const std::tuple<CtxPairs...>& ctx) {
  constexpr int idx = detail::findSymbolIndex<SymbolTag, std::tuple<CtxPairs...>>();
  if constexpr (idx >= 0) {
    // Found - return the name at idx+1
    return std::get<idx + 1>(ctx);
  } else {
    // Not found - use default
    return toString(s);
  }
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

// Context-aware versions for constants and fractions (context unused but needed for uniform interface)
template <int N, typename... CtxPairs>
constexpr auto toString(Constant<N> c, const std::tuple<CtxPairs...>&) {
  return toString(c);
}

template <double VALUE, typename... CtxPairs>
constexpr auto toString(Constant<VALUE> c, const std::tuple<CtxPairs...>&) {
  return toString(c);
}

template <auto VALUE, typename... CtxPairs>
constexpr auto toString(Constant<VALUE> c, const std::tuple<CtxPairs...>&) {
  return toString(c);
}

template <long long N, long long D, typename... CtxPairs>
constexpr auto toString(Fraction<N, D> f, const std::tuple<CtxPairs...>&) {
  return toString(f);
}

// =============================================================================
// EXPRESSION PRINTING - Uses DisplayTraits for operator presentation
// =============================================================================

// Helper to get precedence of any symbolic expression
namespace detail {
template <Symbolic S>
constexpr int get_precedence() {
  if constexpr (is_expression<S>) {
    return DisplayTraits<get_op_t<S>>::precedence;
  } else {
    // Atoms (constants, symbols) have highest precedence
    return Precedence::kAtomic;
  }
}

// Wrap expression in parentheses if needed based on precedence
// Only wrap if child has STRICTLY LOWER precedence
template <int ParentPrec, Symbolic S>
constexpr auto maybeWrap(S) {
  constexpr int childPrec = get_precedence<S>();
  // Only wrap if precedence is strictly lower
  // Equal or higher precedence doesn't need wrapping
  if constexpr (childPrec < ParentPrec) {
    return StaticString("(") + toString(S{}) + StaticString(")");
  } else {
    return toString(S{});
  }
}

// Context-aware version
template <int ParentPrec, Symbolic S, typename... CtxPairs>
constexpr auto maybeWrap(S s, const std::tuple<CtxPairs...>& ctx) {
  constexpr int childPrec = get_precedence<S>();
  if constexpr (childPrec < ParentPrec) {
    return StaticString("(") + toString(s, ctx) + StaticString(")");
  } else {
    return toString(s, ctx);
  }
}

// Helper for infix printing with precedence awareness
template <typename Op, Symbolic First, Symbolic... Rest>
constexpr auto toStringInfixImpl() {
  constexpr auto sym = DisplayTraits<Op>::symbol;
  constexpr int prec = DisplayTraits<Op>::precedence;

  if constexpr (sizeof...(Rest) == 0) {
    return maybeWrap<prec>(First{});
  } else {
    return maybeWrap<prec>(First{}) +
           ((StaticString(" ") + sym + StaticString(" ") + maybeWrap<prec>(Rest{})) +
            ...);
  }
}

// Context-aware version
template <typename Op, typename CtxTuple, Symbolic First, Symbolic... Rest>
constexpr auto toStringInfixImpl(const CtxTuple& ctx) {
  constexpr auto sym = DisplayTraits<Op>::symbol;
  constexpr int prec = DisplayTraits<Op>::precedence;

  if constexpr (sizeof...(Rest) == 0) {
    return maybeWrap<prec>(First{}, ctx);
  } else {
    return maybeWrap<prec>(First{}, ctx) +
           ((StaticString(" ") + sym + StaticString(" ") + maybeWrap<prec>(Rest{}, ctx)) +
            ...);
  }
}
}  // namespace detail

template <typename Op, Symbolic... Args>
constexpr auto toString(Expression<Op, Args...>) {
  if constexpr (DisplayTraits<Op>::mode == DisplayMode::kPrefix) {
    // Prefix: op(arg1, arg2, ...)
    // Prefix operators always use parentheses for clarity
    return DisplayTraits<Op>::symbol + StaticString("(") +
           ((StaticString(" ") + toString(Args{})) + ... + StaticString(")"));
  } else {
    // Infix operators use precedence-aware parenthesization
    if constexpr (sizeof...(Args) == 1) {
      // Unary operator (like negation)
      constexpr int prec = DisplayTraits<Op>::precedence;
      return DisplayTraits<Op>::symbol + detail::maybeWrap<prec>(Args{}...);
    } else {
      // Binary/n-ary operator - no outer parentheses, precedence handles it
      return detail::toStringInfixImpl<Op, Args...>();
    }
  }
}

// Context-aware expression conversion
template <typename Op, Symbolic... Args, typename... CtxPairs>
constexpr auto toString(Expression<Op, Args...> expr, const std::tuple<CtxPairs...>& ctx) {
  if constexpr (DisplayTraits<Op>::mode == DisplayMode::kPrefix) {
    // Prefix: op(arg1, arg2, ...)
    return DisplayTraits<Op>::symbol + StaticString("(") +
           ((StaticString(" ") + toString(Args{}, ctx)) + ... + StaticString(")"));
  } else {
    // Infix operators use precedence-aware parenthesization
    if constexpr (sizeof...(Args) == 1) {
      // Unary operator (like negation)
      constexpr int prec = DisplayTraits<Op>::precedence;
      return DisplayTraits<Op>::symbol + detail::maybeWrap<prec>(Args{}..., ctx);
    } else {
      // Binary/n-ary operator
      return detail::toStringInfixImpl<Op, decltype(ctx), Args...>(ctx);
    }
  }
}

// =============================================================================
// RUNTIME STRING CONVERSION (std::string-based for debugging)
// =============================================================================

// Runtime precedence helpers
namespace detail {
template <Symbolic S>
inline int get_precedence_runtime() {
  if constexpr (is_expression<S>) {
    return DisplayTraits<get_op_t<S>>::precedence;
  } else {
    return Precedence::kAtomic;
  }
}

template <Symbolic S>
inline std::string maybeWrapRuntime(int parentPrec, S) {
  int childPrec = get_precedence_runtime<S>();
  if (childPrec < parentPrec) {
    return "(" + toStringRuntime(S{}) + ")";
  } else {
    return toStringRuntime(S{});
  }
}
}  // namespace detail

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
  // Use DisplayTraits for operator metadata
  std::string opSymbol = std::string(DisplayTraits<Op>::symbol.c_str());
  constexpr int prec = DisplayTraits<Op>::precedence;

  if constexpr (DisplayTraits<Op>::mode == DisplayMode::kPrefix) {
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
    // Infix operators use precedence-aware parenthesization
    if constexpr (sizeof...(Args) == 1) {
      // Unary operator
      return opSymbol + (detail::maybeWrapRuntime(prec, Args{}) + ... + "");
    } else {
      // Binary/n-ary operator - no outer parentheses
      std::string result;
      bool first = true;
      (
          [&]() {
            if (!first) result += " " + opSymbol + " ";
            result += detail::maybeWrapRuntime(prec, Args{});
            first = false;
          }(),
          ...);
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
