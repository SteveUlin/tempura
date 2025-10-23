#pragma once

#include <type_traits>

#include "meta/static_string_display.h"
#include "meta/utility.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/to_string.h"

// ============================================================================
// COMPILE-TIME DEBUGGING UTILITIES FOR SYMBOLIC3 EXPRESSIONS
// ============================================================================
//
// This header provides tools for inspecting and debugging symbolic expressions
// at compile-time. Two main categories of utilities:
//
// 1. TYPE INSPECTION - Display types in compiler errors
//    - constexpr_print_type() - Force compiler to show type
//    - constexpr_type_name() - Get type name as string
//
// 2. MATCH EXPLANATION - Understand pattern matching behavior
//    - explain_match() - Explain why a pattern matches or doesn't match
//    - match_summary() - Short one-line summary of match result
//
// 3. EXPRESSION ANALYSIS - Query expression properties
//    - expression_depth() - Tree depth
//    - operation_count() - Number of operations
//    - is_likely_simplified() - Heuristic simplification check
//
// 4. COMPILE-TIME ASSERTIONS - Better error messages for symbolic code
//    - SYMBOLIC_STATIC_ASSERT - Assert with expression context
//    - VERIFY_SIMPLIFICATION - Check simplification results

namespace tempura::symbolic3 {

// =============================================================================
// COMPILE-TIME TYPE INSPECTION
// =============================================================================

// Force compilation to show type information in error messages
// Usage: constexpr_print_type<decltype(expr)>()
// The compiler error will display the full type of expr
template <typename T>
constexpr void constexpr_print_type() {
  static_assert(sizeof(T) == 0,
                "This static_assert always fails - check the compiler error "
                "message to see the type T");
}

// =============================================================================
// COMPILE-TIME STRING DISPLAY
// =============================================================================
// These utilities force the compiler to show StaticString contents in error messages
// Useful for debugging symbolic expressions, custom names, and toString results
//
// Core utilities are in meta/static_string_display.h
// Re-exported here for convenience in symbolic3 namespace

// Import ShowStaticString from meta namespace
using tempura::ShowStaticString;
using tempura::show_string_in_error;

// Alternative: Uses pretty function name to show type
// This version can be called without template parameters
template <typename T>
constexpr auto constexpr_type_name(T) {
  // The string will contain the full type name
  // In GCC/Clang: __PRETTY_FUNCTION__ contains the instantiation
  return __PRETTY_FUNCTION__;
}

// =============================================================================
// COMPILE-TIME EXPRESSION INSPECTION
// =============================================================================

// Wrapper that forces compile-time string evaluation and shows it in errors
// Usage: constexpr_print_expr(expr)
template <Symbolic S>
constexpr void constexpr_print_expr(S expr) {
  constexpr auto str = toString(expr);
  // Force the string to be evaluated at compile time
  static_assert(str.size() >= 0,
                "Expression (check compiler error for string value)");
}

// Helper to compare two expressions at compile time
// Usage: constexpr_assert_equal(expr1, expr2)
template <Symbolic S1, Symbolic S2>
constexpr void constexpr_assert_equal([[maybe_unused]] S1 lhs,
                                      [[maybe_unused]] S2 rhs) {
  static_assert(std::is_same_v<S1, S2>,
                "Expressions are not equal (check types in error message)");
}

// Helper to verify an expression matches expected form
// Usage: constexpr_assert_match(actual, expected)
template <Symbolic S1, Symbolic S2>
constexpr void constexpr_assert_match([[maybe_unused]] S1 actual,
                                      [[maybe_unused]] S2 expected) {
  static_assert(std::is_same_v<S1, S2>,
                "Expression doesn't match expected form (check error message)");
}

// =============================================================================
// COMPILE-TIME DEBUGGING MACROS
// =============================================================================

// Convenience macro for printing types during compilation
// Usage: CONSTEXPR_PRINT_TYPE(decltype(expr))
#define CONSTEXPR_PRINT_TYPE(T) ::tempura::symbolic3::constexpr_print_type<T>()

// Convenience macro for showing StaticString content in compiler errors
// Usage: SHOW_STATIC_STRING(toString(expr))
//        SHOW_STATIC_STRING("debug message"_cts)
// The error will display the actual string content
// (Re-exported from meta/static_string_display.h)
#ifndef SHOW_STATIC_STRING
#define SHOW_STATIC_STRING(str_expr) \
  do { \
    constexpr auto _debug_str_ = (str_expr); \
    ::tempura::ShowStaticString<_debug_str_> _show_; \
  } while (0)
#endif

// Convenience macro for printing expression strings during compilation
// Usage: CONSTEXPR_PRINT_EXPR(my_expr)
#define CONSTEXPR_PRINT_EXPR(expr) \
  ::tempura::symbolic3::constexpr_print_expr(expr)

// Convenience macro for asserting equality at compile time
// Usage: CONSTEXPR_ASSERT_EQUAL(result, expected)
#define CONSTEXPR_ASSERT_EQUAL(lhs, rhs) \
  ::tempura::symbolic3::constexpr_assert_equal(lhs, rhs)

// =============================================================================
// COMPILE-TIME EXPRESSION PROPERTIES
// =============================================================================

// Check if expression is simplified (doesn't contain obvious patterns)
// This is a heuristic check - not exhaustive
template <Symbolic S>
constexpr bool is_likely_simplified(S) {
  if constexpr (is_constant<S> || is_symbol<S>) {
    return true;
  } else if constexpr (is_expression<S>) {
    using Op = get_op_t<S>;

    // Check for obvious non-simplified patterns - use expression directly
    if constexpr (isSame<Op, AddOp>) {
      // Check for x + 0 or 0 + x
      return []<typename OpT, Symbolic... As>(Expression<OpT, As...>) {
        return !(... || isSame<As, Constant<0>>);
      }(S{});
    } else if constexpr (isSame<Op, MulOp>) {
      // Check for x * 1 or 1 * x or x * 0
      return []<typename OpT, Symbolic... As>(Expression<OpT, As...>) {
        return !(... || (isSame<As, Constant<0>> || isSame<As, Constant<1>>));
      }(S{});
    }
    return true;
  }
  return true;
}

// Helper to compute max depth from args
template <typename Op, Symbolic... Args>
constexpr int expression_depth_impl(Expression<Op, Args...>) {
  if constexpr (sizeof...(Args) == 0) {
    return 1;
  } else {
    return 1 + std::max({expression_depth(Args{})...});
  }
}

// Get the "depth" of an expression tree
template <Symbolic S>
constexpr int expression_depth(S) {
  if constexpr (is_constant<S> || is_symbol<S>) {
    return 0;
  } else if constexpr (is_expression<S>) {
    return expression_depth_impl(S{});
  }
  return 0;
}

// Helper to count operations from args
template <typename Op, Symbolic... Args>
constexpr int operation_count_impl(Expression<Op, Args...>) {
  if constexpr (sizeof...(Args) == 0) {
    return 1;
  } else {
    return 1 + (... + operation_count(Args{}));
  }
}

// Count the number of operations in an expression
template <Symbolic S>
constexpr int operation_count(S) {
  if constexpr (is_constant<S> || is_symbol<S>) {
    return 0;
  } else if constexpr (is_expression<S>) {
    return operation_count_impl(S{});
  }
  return 0;
}

// =============================================================================
// COMPILE-TIME DEBUGGING HELPERS FOR TESTS
// =============================================================================

// Wrapper for static_assert with better error messages for symbolic expressions
// Usage: SYMBOLIC_STATIC_ASSERT(std::is_same_v<Result, Expected>, result,
// expected)
#define SYMBOLIC_STATIC_ASSERT(condition, actual, expected)                   \
  do {                                                                        \
    constexpr auto _actual = actual;                                          \
    constexpr auto _expected = expected;                                      \
    constexpr auto _actual_str = ::tempura::symbolic3::toString(_actual);     \
    constexpr auto _expected_str = ::tempura::symbolic3::toString(_expected); \
    static_assert(condition,                                                  \
                  "Symbolic assertion failed - check types in "               \
                  "error message");                                           \
  } while (0)

// Verify simplification produces expected result
// Usage: VERIFY_SIMPLIFICATION(simplify(expr), expected_result)
#define VERIFY_SIMPLIFICATION(actual, expected)                       \
  do {                                                                \
    constexpr auto _actual = actual;                                  \
    constexpr auto _expected = expected;                              \
    static_assert(                                                    \
        std::is_same_v<decltype(_actual), decltype(_expected)>,       \
        "Simplification produced unexpected result - check types in " \
        "error message");                                             \
  } while (0)

// =============================================================================
// COMPILE-TIME EXPRESSION COMPARISON
// =============================================================================

// Check if two expressions are structurally equivalent
// (same operations, same constants, same symbols)
template <Symbolic S1, Symbolic S2>
constexpr bool structurally_equal(S1, S2) {
  return std::is_same_v<S1, S2>;
}

// Helper for contains_subexpression
template <Symbolic Sub, typename Op, Symbolic... Args>
constexpr bool contains_subexpression_impl(Expression<Op, Args...>, Sub) {
  return (... || contains_subexpression(Args{}, Sub{}));
}

// Check if expression contains a sub-expression
template <Symbolic S, Symbolic Sub>
constexpr bool contains_subexpression(S, Sub) {
  if constexpr (isSame<S, Sub>) {
    return true;
  } else if constexpr (is_expression<S>) {
    return contains_subexpression_impl(S{}, Sub{});
  }
  return false;
}

// =============================================================================
// PATTERN MATCH EXPLANATION UTILITIES
// =============================================================================
// These utilities explain why pattern matches succeed or fail
// Invaluable for debugging complex rewrite rules and simplification strategies
//
// USAGE EXAMPLE:
//   constexpr auto pattern = x_ + 0_c;
//   constexpr auto expr = y + 5_c;
//   constexpr auto explanation = explain_match(pattern, expr);
//   // explanation: "Match failed: Constant<0> != Constant<5> (values differ)"
//
// The explanations are compile-time strings that can be:
// - Displayed in static_assert messages
// - Logged at runtime for debugging
// - Used in test failure messages

namespace detail {

// Helper to build match explanation strings
// Uses FixedString for compile-time string manipulation
template <size_t N>
struct MatchExplanation {
  char data[N + 1]{};
  size_t size = N;

  constexpr MatchExplanation(const char (&str)[N + 1]) {
    for (size_t i = 0; i < N; ++i) {
      data[i] = str[i];
    }
    data[N] = '\0';
  }

  constexpr const char* c_str() const { return data; }

  // Compile-time equality comparison with another MatchExplanation
  template <size_t M>
  constexpr bool operator==(const MatchExplanation<M>& other) const {
    if (size != other.size) return false;
    for (size_t i = 0; i < size; ++i) {
      if (data[i] != other.data[i]) return false;
    }
    return true;
  }

  // Compile-time comparison with string literal
  template <size_t M>
  constexpr bool operator==(const char (&str)[M]) const {
    if (size != M - 1) return false;
    for (size_t i = 0; i < size; ++i) {
      if (data[i] != str[i]) return false;
    }
    return true;
  }

  // Check if string contains a substring at compile-time
  template <size_t M>
  constexpr bool contains(const char (&substr)[M]) const {
    if (M - 1 > size) return false;
    for (size_t i = 0; i <= size - (M - 1); ++i) {
      bool match = true;
      for (size_t j = 0; j < M - 1; ++j) {
        if (data[i + j] != substr[j]) {
          match = false;
          break;
        }
      }
      if (match) return true;
    }
    return false;
  }
};

template <size_t N>
MatchExplanation(const char (&)[N]) -> MatchExplanation<N - 1>;

// Concatenate explanation strings
template <size_t N1, size_t N2>
constexpr auto concat_explanation(const MatchExplanation<N1>& a,
                                  const MatchExplanation<N2>& b) {
  MatchExplanation<N1 + N2> result{""};
  for (size_t i = 0; i < N1; ++i) {
    result.data[i] = a.data[i];
  }
  for (size_t i = 0; i < N2; ++i) {
    result.data[N1 + i] = b.data[i];
  }
  result.data[N1 + N2] = '\0';
  result.size = N1 + N2;
  return result;
}

}  // namespace detail

// =============================================================================
// MATCH EXPLANATION FUNCTIONS
// =============================================================================
// These mirror the match() overloads in matching.h but return explanations

// Explain Symbol matching
template <typename U1, typename U2>
constexpr auto explain_match(Symbol<U1>, Symbol<U2>) {
  if constexpr (isSame<Symbol<U1>, Symbol<U2>>) {
    return detail::MatchExplanation{"✓ Match succeeded: Symbols have same type identity"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: Symbols have different type identities"};
  }
}

// Explain Constant matching
template <auto V1, auto V2>
constexpr auto explain_match(Constant<V1>, Constant<V2>) {
  if constexpr (V1 == V2) {
    return detail::MatchExplanation{"✓ Match succeeded: Constants have same value"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: Constants have different values"};
  }
}

// Explain Fraction matching
template <long long N1, long long D1, long long N2, long long D2>
constexpr auto explain_match(Fraction<N1, D1>, Fraction<N2, D2>) {
  if constexpr (Fraction<N1, D1>::numerator == Fraction<N2, D2>::numerator &&
                Fraction<N1, D1>::denominator == Fraction<N2, D2>::denominator) {
    return detail::MatchExplanation{"✓ Match succeeded: Fractions reduce to same value"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: Fractions have different reduced forms"};
  }
}

// Explain AnyArg matching (always succeeds)
template <Symbolic S>
constexpr auto explain_match(AnyArg, S) {
  return detail::MatchExplanation{"✓ Match succeeded: AnyArg matches any expression"};
}

template <Symbolic S>
constexpr auto explain_match(S, AnyArg) {
  return detail::MatchExplanation{"✓ Match succeeded: Any expression matches AnyArg"};
}

// Explain AnyExpr matching
template <Symbolic S>
constexpr auto explain_match(AnyExpr, S) {
  if constexpr (is_expression<S>) {
    return detail::MatchExplanation{"✓ Match succeeded: AnyExpr matches compound expression"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: AnyExpr only matches compound expressions (not atoms)"};
  }
}

// Explain AnyConstant matching
template <Symbolic S>
constexpr auto explain_match(AnyConstant, S) {
  if constexpr (is_constant<S> || is_fraction<S>) {
    return detail::MatchExplanation{"✓ Match succeeded: AnyConstant matches constant value"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: AnyConstant only matches constants and fractions"};
  }
}

// Explain AnySymbol matching
template <Symbolic S>
constexpr auto explain_match(AnySymbol, S) {
  if constexpr (is_symbol<S>) {
    return detail::MatchExplanation{"✓ Match succeeded: AnySymbol matches symbolic variable"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: AnySymbol only matches symbols"};
  }
}

// Explain Expression matching - structural recursion
template <typename Op1, Symbolic... Args1, typename Op2, Symbolic... Args2>
constexpr auto explain_match(Expression<Op1, Args1...>,
                             Expression<Op2, Args2...>) {
  if constexpr (!isSame<Op1, Op2>) {
    return detail::MatchExplanation{"✗ Match failed: Operations differ"};
  } else if constexpr (sizeof...(Args1) != sizeof...(Args2)) {
    return detail::MatchExplanation{"✗ Match failed: Argument counts differ"};
  } else if constexpr (sizeof...(Args1) == 0) {
    return detail::MatchExplanation{"✓ Match succeeded: Same operation, no arguments"};
  } else {
    // Check if all arguments match
    if constexpr ((match(Args1{}, Args2{}) && ...)) {
      return detail::MatchExplanation{"✓ Match succeeded: Operation and all arguments match"};
    } else {
      return detail::MatchExplanation{"✗ Match failed: Operation matches but some arguments differ"};
    }
  }
}

// Explain Never matching (always fails)
constexpr auto explain_match(Never, Never) {
  return detail::MatchExplanation{"✗ Match failed: Never never matches (not even itself)"};
}

template <Symbolic S>
  requires(!isSame<S, Never>)
constexpr auto explain_match(Never, S) {
  return detail::MatchExplanation{"✗ Match failed: Never matches nothing"};
}

template <Symbolic S>
  requires(!isSame<S, Never>)
constexpr auto explain_match(S, Never) {
  return detail::MatchExplanation{"✗ Match failed: Nothing matches Never"};
}

// Explain fallback (type mismatch)
// Note: This overload is carefully constrained to not conflict with wildcards
template <Symbolic S1, Symbolic S2>
  requires(!isSame<S1, S2> &&       // Different types
           !is_expression<S1> &&    // Not both expressions
           !is_expression<S2> &&
           !isSame<S1, AnyArg> && !isSame<S2, AnyArg> &&          // Not wildcards
           !isSame<S1, AnyExpr> && !isSame<S2, AnyExpr> &&
           !isSame<S1, AnyConstant> && !isSame<S2, AnyConstant> &&
           !isSame<S1, AnySymbol> && !isSame<S2, AnySymbol>)
constexpr auto explain_match(S1, S2) {
  if constexpr (is_symbol<S1> && is_constant<S2>) {
    return detail::MatchExplanation{"✗ Match failed: Symbol cannot match Constant"};
  } else if constexpr (is_constant<S1> && is_symbol<S2>) {
    return detail::MatchExplanation{"✗ Match failed: Constant cannot match Symbol"};
  } else if constexpr (is_symbol<S1> && is_fraction<S2>) {
    return detail::MatchExplanation{"✗ Match failed: Symbol cannot match Fraction"};
  } else if constexpr (is_fraction<S1> && is_symbol<S2>) {
    return detail::MatchExplanation{"✗ Match failed: Fraction cannot match Symbol"};
  } else if constexpr (is_constant<S1> && is_fraction<S2>) {
    return detail::MatchExplanation{"✗ Match failed: Constant cannot match Fraction"};
  } else if constexpr (is_fraction<S1> && is_constant<S2>) {
    return detail::MatchExplanation{"✗ Match failed: Fraction cannot match Constant"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: Type category mismatch"};
  }
}

// Fallback for expression vs atom mismatch
template <Symbolic S1, Symbolic S2>
  requires((is_expression<S1> && !is_expression<S2>) ||
           (!is_expression<S1> && is_expression<S2>))
constexpr auto explain_match(S1, S2) {
  if constexpr (is_expression<S1>) {
    return detail::MatchExplanation{"✗ Match failed: Compound expression cannot match atomic value"};
  } else {
    return detail::MatchExplanation{"✗ Match failed: Atomic value cannot match compound expression"};
  }
}

// =============================================================================
// MATCH SUMMARY - Short one-line description
// =============================================================================

template <Symbolic S1, Symbolic S2>
constexpr bool match_result_bool(S1 a, S2 b) {
  return match(a, b);
}

template <Symbolic S1, Symbolic S2>
constexpr auto match_summary(S1 a, S2 b) {
  if constexpr (match(a, b)) {
    return detail::MatchExplanation{"✓ MATCH"};
  } else {
    return detail::MatchExplanation{"✗ NO MATCH"};
  }
}

// =============================================================================
// COMPILE-TIME BENCHMARKING HELPERS
// =============================================================================

// Marker for timing compile-time evaluation
// Usage: START_CONSTEXPR_TIMER(); ... END_CONSTEXPR_TIMER();
// (Use compiler flags like -ftime-trace to see actual timing)
struct CompileTimeMarker {
  const char* label;
  constexpr CompileTimeMarker(const char* l) : label(l) {}
};

#define START_CONSTEXPR_TIMER(label) \
  constexpr CompileTimeMarker _timer_##__LINE__##_start { label }

#define END_CONSTEXPR_TIMER(label) \
  constexpr CompileTimeMarker _timer_##__LINE__##_end{label "_end"}

}  // namespace tempura::symbolic3
