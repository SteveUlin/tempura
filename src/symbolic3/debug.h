#pragma once

#include <type_traits>

#include "symbolic3/core.h"
#include "symbolic3/to_string.h"

// Compile-time debugging utilities for symbolic3 expressions
// These tools help inspect expression types during compilation using
// compiler error messages and static_assert

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
    using Args = get_args_t<S>;

    // Check for obvious non-simplified patterns
    if constexpr (std::same_as<Op, AddOp>) {
      // Check for x + 0 or 0 + x
      return !std::apply(
          []<typename... As>(As...) {
            return (... || std::is_same_v<As, Constant<0>>);
          },
          Args{});
    } else if constexpr (std::same_as<Op, MulOp>) {
      // Check for x * 1 or 1 * x or x * 0
      return !std::apply(
          []<typename... As>(As...) {
            return (... || (std::is_same_v<As, Constant<0>> ||
                            std::is_same_v<As, Constant<1>>));
          },
          Args{});
    }
    return true;
  }
  return true;
}

// Get the "depth" of an expression tree
template <Symbolic S>
constexpr int expression_depth(S) {
  if constexpr (is_constant<S> || is_symbol<S>) {
    return 0;
  } else if constexpr (is_expression<S>) {
    using Args = get_args_t<S>;
    return 1 + std::apply(
                   []<typename... As>(As...) {
                     return std::max({expression_depth(As{})...});
                   },
                   Args{});
  }
  return 0;
}

// Count the number of operations in an expression
template <Symbolic S>
constexpr int operation_count(S) {
  if constexpr (is_constant<S> || is_symbol<S>) {
    return 0;
  } else if constexpr (is_expression<S>) {
    using Args = get_args_t<S>;
    return 1 + std::apply([]<typename... As>(
                              As...) { return (... + operation_count(As{})); },
                          Args{});
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

// Check if expression contains a sub-expression
template <Symbolic S, Symbolic Sub>
constexpr bool contains_subexpression(S, Sub) {
  if constexpr (std::is_same_v<S, Sub>) {
    return true;
  } else if constexpr (is_expression<S>) {
    using Args = get_args_t<S>;
    return std::apply(
        []<typename... As>(As...) {
          return (... || contains_subexpression(As{}, Sub{}));
        },
        Args{});
  }
  return false;
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
