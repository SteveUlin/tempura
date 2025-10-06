#pragma once

#include <tuple>

#include "meta/type_id.h"

// Core symbolic types and concepts for symbolic3 combinator system
// Based on symbolic2 but designed for the combinator architecture

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

  constexpr auto operator=(auto value) const;  // Enable x = 5 binding syntax
};

// Numeric constant embedded in type system for compile-time computation
template <auto val>
struct Constant : SymbolicTag {
  static constexpr auto value = val;
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

template <typename T>
struct get_args;

template <typename Op, Symbolic... Args>
struct get_args<Expression<Op, Args...>> {
  using type = std::tuple<Args...>;
};

template <typename T>
using get_args_t = typename get_args<T>::type;

}  // namespace tempura::symbolic3
