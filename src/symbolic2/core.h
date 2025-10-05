#pragma once

#include "meta/type_id.h"

// Core symbolic types and concepts

namespace tempura {

// --- Core Symbolic Concept & Tag ---

struct SymbolicTag {};

template <typename T>
concept Symbolic = DerivedFrom<T, SymbolicTag>;

// --- Symbol ---

// Unique symbolic variable using stateless lambda for type identity
//
// Each Symbol{} creates a distinct type via the default lambda parameter,
// enabling compile-time symbolic computation without runtime overhead.
template <typename unique = decltype([] {})>
struct Symbol : SymbolicTag {
  // Type ID determines ordering - earlier symbols have lower IDs
  static constexpr auto id = kMeta<Symbol<unique>>;

  constexpr Symbol() {
    // Force type ID generation to preserve declaration order
    (void)id;
  };

  // Enable binding: (x = 5) creates TypeValueBinder for evaluation
  constexpr auto operator=(auto value) const;
};

// --- Constant ---

// Compile-time numeric constant embedded in the type system
template <auto val>
struct Constant : SymbolicTag {
  static constexpr auto value = val;
};

// --- Expression ---

// Symbolic expression tree: operator + arguments encoded in type
template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {
  constexpr Expression() = default;
  constexpr Expression(Op, Args...) {}
};

// --- Pattern Matching Wildcards ---

struct AnyArg : SymbolicTag {};       // Matches any symbolic expression
struct AnyExpr : SymbolicTag {};      // Matches any Expression<Op, Args...>
struct AnyConstant : SymbolicTag {};  // Matches any Constant<value>
struct AnySymbol : SymbolicTag {};    // Matches any Symbol<unique>
struct Never : SymbolicTag {};        // Never matches (accessor return type)

}  // namespace tempura
