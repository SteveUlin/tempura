#pragma once

#include "meta/type_id.h"

// Core symbolic types and concepts

namespace tempura {

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

}  // namespace tempura
