#pragma once

#include "meta/tags.h"  // For TypeList
#include "meta/type_id.h"
#include "meta/type_list.h"  // For Get_t
#include "meta/utility.h"

// Core symbolic types and concepts for symbolic3 combinator system
// Combinator-based symbolic computation core types
// Uses stateless lambdas for unique type identity (no NTTP hacks)
// Zero runtime overhead - all computation is compile-time

namespace tempura::symbolic3 {

struct SymbolicTag {};

template <typename T>
concept Symbolic = DerivedFrom<T, SymbolicTag>;

// Symbolic variable with unique type identity via stateless lambda
// Each Symbol{} declaration generates a distinct type for compile-time tracking
template <typename unique = decltype([] {})>
struct Symbol : SymbolicTag {
  static constexpr auto id = kMeta<Symbol<unique>>;

  constexpr Symbol() { (void)id; }  // Force ID generation for stable ordering

  // Enable binding syntax for evaluation: x = value
  // Returns TypeValueBinder for use in BinderPack{x = 5, y = 3}
  // The key insight: operator= returns auto, allowing the binder to capture
  // both the Symbol type (compile-time) and the value (runtime-compatible) This
  // enables heterogeneous binding: BinderPack{x = 5, y = 3.14, z = "text"}
  // Actual implementation is in evaluate.h to avoid circular dependency
  constexpr auto operator=(auto value) const;
};

// Numeric constant embedded in type system for compile-time computation
template <auto val>
struct Constant : SymbolicTag {
  static constexpr auto value = val;
};

// Constexpr GCD using Euclidean algorithm (no STL dependencies)
constexpr long long gcd_impl(long long a, long long b) {
  return b == 0 ? a : gcd_impl(b, a % b);
}

constexpr long long abs_val(long long x) { return x < 0 ? -x : x; }

constexpr long long gcd(long long a, long long b) {
  return gcd_impl(abs_val(a), abs_val(b));
}

// Compile-time rational number (exact fraction) - GCD-reduced automatically
// Enables exact arithmetic without floating-point approximation
template <long long N, long long D = 1>
struct Fraction : SymbolicTag {
  static_assert(D != 0, "Denominator cannot be zero");

  // Reduce fraction to lowest terms at compile-time
  static constexpr auto g = gcd(N, D);
  static constexpr auto sign = ((N < 0) != (D < 0)) ? -1 : 1;  // XOR for sign

  static constexpr auto numerator = sign * abs_val(N) / g;
  static constexpr auto denominator = abs_val(D) / g;

  // Convert to double for evaluation (opt-in only)
  static constexpr double to_double() {
    return static_cast<double>(numerator) / static_cast<double>(denominator);
  }
};

// Expression node: operation + arguments encoded entirely in type system
template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {
  constexpr Expression() = default;
  constexpr Expression(Op, Args...) {}
};

// Pattern matching wildcards
struct AnyArg : SymbolicTag {};       // Universal wildcard
struct AnyExpr : SymbolicTag {};      // Compound expressions only
struct AnyConstant : SymbolicTag {};  // Numeric constants only
struct AnySymbol : SymbolicTag {};    // Symbols only
struct Never : SymbolicTag {};        // Non-matching sentinel type

// Type predicates for expression classification
template <typename T>
constexpr bool is_symbol = false;

template <typename unique>
constexpr bool is_symbol<Symbol<unique>> = true;

template <typename T>
constexpr bool is_constant = false;

template <auto val>
constexpr bool is_constant<Constant<val>> = true;

template <typename T>
constexpr bool is_fraction = false;

template <long long N, long long D>
constexpr bool is_fraction<Fraction<N, D>> = true;

template <typename T>
constexpr bool is_expression = false;

template <typename Op, Symbolic... Args>
constexpr bool is_expression<Expression<Op, Args...>> = true;

// Handle cv-qualified versions
template <typename T>
constexpr bool is_expression<const T> = is_expression<T>;

template <typename T>
constexpr bool is_expression<volatile T> = is_expression<T>;

template <typename T>
constexpr bool is_expression<const volatile T> = is_expression<T>;

// Type extraction utilities for expressions
template <typename T>
struct get_op;

template <typename Op, Symbolic... Args>
struct get_op<Expression<Op, Args...>> {
  using type = Op;
};

template <typename T>
using get_op_t = typename get_op<T>::type;

// Get arguments as TypeList
template <typename T>
struct get_args;

template <typename Op, Symbolic... Args>
struct get_args<Expression<Op, Args...>> {
  using type = TypeList<Args...>;
};

template <typename T>
using get_args_t = typename get_args<T>::type;

// Helper to extract Nth argument type from an expression
// Uses meta/type_list.h Get_t helper
template <SizeT N, typename T>
using get_arg_t = Get_t<N, typename get_args<T>::type>;

}  // namespace tempura::symbolic3
