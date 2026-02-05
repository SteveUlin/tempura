#pragma once

#include <type_traits>

#include "symbolic4/core.h"

// ============================================================================
// let.h - Identity tracking for DAG representation
// ============================================================================
//
// The Problem: Expression Trees vs Expression DAGs
//
// When the same subexpression appears multiple times in a computation,
// a pure tree representation duplicates it:
//
//   // Mathematically: (x*x) + (x*x) + (x*x)
//   auto sum = x*x + x*x + x*x;  // Three separate x*x subtrees!
//
// This leads to redundant computation. What we want is a DAG (Directed
// Acyclic Graph) where shared subexpressions are computed once:
//
//   let t = x*x in t + t + t  // Single x*x, referenced three times
//
// The Solution: LetNode
//
// LetNode wraps an expression with a unique identity type Id. This allows:
//   1. Recognizing when the same subexpression appears multiple times
//   2. Caching results during traversal (fold_unique, not yet implemented)
//   3. Named intermediate values for debugging and inspection
//
// Usage:
//   auto squared = let<struct Squared>(x * x);  // Wrap with identity
//   auto expr = squared.sym() + squared.sym();  // Reference it twice
//
// Or with the convenience macro:
//   TEMPURA_LET(squared, x * x);                // Auto-generates Id type
//
// IdSet for Tracking:
//
// During traversal, IdSet tracks which LetNode identities have been visited.
// This enables detecting and handling shared subexpressions efficiently.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// LetNode - Wraps an expression with a unique identity
// ============================================================================

// LetNode wraps an expression with an identity type Id.
//
// Type parameters:
//   Id - Unique type identifying this let-binding (typically struct Tag)
//   E  - The wrapped expression type
//
// Unlike Expression which is an internal node, LetNode is a wrapper that
// adds identity tracking without changing the expression structure.
template <typename Id, Symbolic E>
struct LetNode : SymbolicTag {
  using id_type = Id;
  using expr_type = E;

  [[no_unique_address]] E expr_;

  constexpr LetNode() = default;
  constexpr explicit LetNode(E e) : expr_{e} {}

  // Access the wrapped expression (for evaluation or display)
  constexpr auto expr() const { return expr_; }

  // Get a Symbol that references this LetNode by its identity.
  // Used to create references to the bound value in other expressions.
  static constexpr auto sym() { return Symbol<Id>{}; }
};

// Convenience function: let<Id>(expr) creates a LetNode
template <typename Id, Symbolic E>
constexpr auto let(E expr) {
  return LetNode<Id, E>{expr};
}

// TEMPURA_LET macro: auto-generates the identity type from the variable name.
//
// Usage:
//   TEMPURA_LET(squared, x * x);  // Expands to:
//   auto squared = let<struct squared_id>(x * x);
//
// The generated Id type (squared_id) is unique to this let-binding,
// allowing the system to track references to it.
#define TEMPURA_LET(name, expr) \
  auto name = ::tempura::symbolic4::let<struct name##_id>(expr)

// ============================================================================
// Type Traits for LetNode
// ============================================================================

// is_let_node_v<T> - check if T is a LetNode
template <typename T>
constexpr bool is_let_node_v = core_traits_detail::isSpecOf<T, LetNode>();

// get_let_id_t<T> - extract the Id type from a LetNode (first template argument)
template <typename T>
  requires is_let_node_v<T>
using get_let_id_t = [:std::meta::template_arguments_of(^^T)[0]:];

// get_let_expr_t<T> - extract the expression type from a LetNode (second template argument)
template <typename T>
  requires is_let_node_v<T>
using get_let_expr_t = [:std::meta::template_arguments_of(^^T)[1]:];

// ============================================================================
// IdSet - Compile-time set of identity types
// ============================================================================
//
// A type-level set for tracking which LetNode identities have been seen.
// Used during traversal to detect shared subexpressions and avoid
// redundant computation.
//
// Operations:
//   IdSet<A, B, C>                    - A set containing A, B, C
//   id_set_contains_v<X, Set>         - true if X is in Set
//   id_set_insert_t<X, Set>           - Set with X added (no-op if already present)
//
// Example:
//   using S0 = IdSet<>;                         // Empty set
//   using S1 = id_set_insert_t<A, S0>;          // {A}
//   using S2 = id_set_insert_t<B, S1>;          // {B, A}
//   static_assert(id_set_contains_v<A, S2>);    // true
//   static_assert(!id_set_contains_v<C, S2>);   // false

template <typename... Ids>
struct IdSet {
  static constexpr SizeT size = sizeof...(Ids);
};

// id_set_contains_v: fold expression over the pack (no recursive instantiation)
template <typename Id, typename Set>
struct IdSetContains;

template <typename Id, typename... Ids>
struct IdSetContains<Id, IdSet<Ids...>> {
  static constexpr bool value = (std::is_same_v<Id, Ids> || ...);
};

template <typename Id, typename Set>
constexpr bool id_set_contains_v = IdSetContains<Id, Set>::value;

// id_set_insert_t: add Id if not already present (maintains set semantics)
template <typename Id, typename Set>
struct IdSetInsert;

template <typename Id, typename... Ids>
struct IdSetInsert<Id, IdSet<Ids...>> {
  using type =
      std::conditional_t<id_set_contains_v<Id, IdSet<Ids...>>, IdSet<Ids...>,
                         IdSet<Id, Ids...>>;
};

template <typename Id, typename Set>
using id_set_insert_t = typename IdSetInsert<Id, Set>::type;

}  // namespace tempura::symbolic4
