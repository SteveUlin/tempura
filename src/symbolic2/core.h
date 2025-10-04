#pragma once

#include "meta/type_id.h"

// Core symbolic types and concepts

namespace tempura {

// --- Core Symbolic Concept & Tag ---

struct SymbolicTag {};

template <typename T>
concept Symbolic = DerivedFrom<T, SymbolicTag>;

// --- Symbol ---

// Symbol is a unique symbolic variable.
//
// The lambda in the template ensures that each call to Symbol{}, will produce
// a different unique type.
template <typename unique = decltype([] {})>
struct Symbol : SymbolicTag {
  // kMeta acts as a compile-time counter for types. Symbols with lower ids
  // (defined first) will have higher precedence in comparisons.
  static constexpr auto id = kMeta<Symbol<unique>>;

  constexpr Symbol() {
    // In order to preserve symbol order, force type id generation for all
    // Symbols
    (void)id;
  };

  // Create a type value binder with the assignment operator
  // auto binder x = 5;
  //   -> binder[x] == 5;
  constexpr auto operator=(auto value) const;
};

// --- Constant ---

template <auto val>
struct Constant : SymbolicTag {
  static constexpr auto value = val;
};

// --- Expression ---

template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {
  constexpr Expression() = default;
  constexpr Expression(Op, Args...) {}
};

// --- Wildcard Matchers ---

struct AnyArg : SymbolicTag {};
struct AnyExpr : SymbolicTag {};
struct AnyConstant : SymbolicTag {};
struct AnySymbol : SymbolicTag {};
struct Never : SymbolicTag {};

}  // namespace tempura
