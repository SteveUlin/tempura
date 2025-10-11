#pragma once

#include "meta/type_sort.h"
#include "symbolic3/core.h"
#include "symbolic3/ordering.h"
#include "symbolic3/term_structure.h"

// Canonical form implementation for associative/commutative operators
// Flattens nested operations and sorts arguments for a unique representation
//
// This addresses Recommendation #2: Instead of representing a+b+c as
// Expression<Add, Expression<Add, a, b>, c>, we flatten to
// Expression<Add, a, b, c> with sorted arguments.
//
// Benefits:
// - Automatic commutativity: a+b and b+a have the same type
// - Automatic associativity: (a+b)+c and a+(b+c) have the same type
// - Drastically reduces rewrite rules needed
// - Easier term collection: iterate arguments and combine like terms

namespace tempura::symbolic3 {

// ============================================================================
// TypeList Utilities (minimal, local to this header)
// ============================================================================

namespace detail {

// Prepend type to pack
template <typename T, typename... Pack>
struct Prepend {
  using type = TypeList<T, Pack...>;
};

template <typename T, typename... Pack>
struct Prepend<T, TypeList<Pack...>> {
  using type = TypeList<T, Pack...>;
};

template <typename T, typename Pack>
using Prepend_t = typename Prepend<T, Pack>::type;

// Append type to pack
template <typename... Pack, typename T>
constexpr auto append_impl(TypeList<Pack...>, TypeList<T>)
    -> TypeList<Pack..., T>;

template <typename Pack, typename T>
using Append_t = decltype(append_impl(Pack{}, TypeList<T>{}));

// Concatenate two packs
template <typename... Pack1, typename... Pack2>
constexpr auto concat_impl(TypeList<Pack1...>, TypeList<Pack2...>)
    -> TypeList<Pack1..., Pack2...>;

template <typename Pack1, typename Pack2>
using Concat_t = decltype(concat_impl(Pack1{}, Pack2{}));

// ============================================================================
// Type Sorting Utilities
// ============================================================================

// Comparison predicate for symbolic3 ordering
struct LessThanComparator {
  template <typename A, typename B>
  constexpr bool operator()(A, B) const {
    return lessThan(A{}, B{});
  }
};

// Sort TypeList using symbolic3 ordering
template <typename List>
using SortTypes_t = SortTypes_t<List, LessThanComparator>;

// ============================================================================
// Flattening Utilities
// ============================================================================

// Check if an expression has the same operation type
template <typename Op, typename Expr>
struct HasSameOp : std::false_type {};

template <typename Op, Symbolic... Args>
struct HasSameOp<Op, Expression<Op, Args...>> : std::true_type {};

template <typename Op, typename Expr>
constexpr bool has_same_op = HasSameOp<Op, Expr>::value;

// Flatten nested operations of the same type
// E.g., flatten(Add, [a, Add(b, c), d]) -> [a, b, c, d]
template <typename Op, typename ArgList>
struct FlattenArgs;

template <typename Op>
struct FlattenArgs<Op, TypeList<>> {
  using type = TypeList<>;
};

template <typename Op, typename First, typename... Rest>
struct FlattenArgs<Op, TypeList<First, Rest...>> {
  using rest_flattened = typename FlattenArgs<Op, TypeList<Rest...>>::type;

  // If First is Expression<Op, ...>, extract its args and flatten recursively
  using type = std::conditional_t<
      has_same_op<Op, First>,
      typename FlattenArgs<
          Op, Concat_t<typename get_args<First>::type, rest_flattened>>::type,
      Prepend_t<First, rest_flattened>>;
};

template <typename Op, typename ArgList>
using FlattenArgs_t = typename FlattenArgs<Op, ArgList>::type;

// ============================================================================
// Operation-Specific Sorting Strategies
// ============================================================================

// For addition: use algebraic term structure to group like terms
// Groups by base (e.g., x and 3*x together), then sorts by coefficient
// This enables better term collection: x + 3*x + 2 + y -> 2 + x + 3*x + y
template <typename List>
struct SortForAddition {
  using type = ::tempura::SortTypes_t<List, AdditionTermComparator>;
};

template <typename List>
using SortForAddition_t = typename SortForAddition<List>::type;

// For multiplication: use algebraic term structure to group powers
// Groups by base (e.g., x and x^2 together), then sorts by exponent
// This enables: x*2*x^2*3*y -> 2*3*x*x^2*y -> 6*x^3*y (after reduction)
template <typename List>
struct SortForMultiplication {
  using type = ::tempura::SortTypes_t<List, MultiplicationTermComparator>;
};

template <typename List>
using SortForMultiplication_t = typename SortForMultiplication<List>::type;

}  // namespace detail

// ============================================================================
// Canonical Form Construction
// ============================================================================

// Trait to determine if an operator should use canonical form
template <typename Op>
struct UsesCanonicalForm : std::false_type {};

// Forward declarations for operation types we'll specialize
struct AddOp;
struct MulOp;

template <>
struct UsesCanonicalForm<AddOp> : std::true_type {};

template <>
struct UsesCanonicalForm<MulOp> : std::true_type {};

template <typename Op>
constexpr bool uses_canonical_form = UsesCanonicalForm<Op>::value;

// Build canonical form: flatten + sort
template <typename Op, typename... Args>
struct MakeCanonical {
  // Step 1: Flatten nested operations
  using flattened = detail::FlattenArgs_t<Op, TypeList<Args...>>;

  // Step 2: Sort according to operation-specific strategy
  using sorted = std::conditional_t<
      std::is_same_v<Op, AddOp>, detail::SortForAddition_t<flattened>,
      std::conditional_t<
          std::is_same_v<Op, MulOp>, detail::SortForMultiplication_t<flattened>,
          detail::SortTypes_t<flattened>  // Default: generic sort
          >>;

  // Step 3: Convert TypeList back to Expression
  template <typename TL>
  struct ToExpression;

  template <Symbolic... SortedArgs>
  struct ToExpression<TypeList<SortedArgs...>> {
    using type = Expression<Op, SortedArgs...>;
  };

  using type = typename ToExpression<sorted>::type;
};

template <typename Op, typename... Args>
using MakeCanonical_t = typename MakeCanonical<Op, Args...>::type;

// ============================================================================
// Canonicalization Strategy
// ============================================================================

// Helper to build canonical form from std::tuple of args
template <typename Op, typename Tuple>
struct MakeCanonicalFromTuple;

template <typename Op, Symbolic... Args>
struct MakeCanonicalFromTuple<Op, std::tuple<Args...>> {
  using type = MakeCanonical_t<Op, Args...>;
};

template <typename Op, typename Tuple>
using MakeCanonicalFromTuple_t =
    typename MakeCanonicalFromTuple<Op, Tuple>::type;

// Strategy that converts expressions to canonical form
// Applies flattening and sorting for associative/commutative operations
struct Canonicalize {
  template <Symbolic Expr, typename Context>
  constexpr auto apply(Expr expr, Context ctx) const {
    // Only apply to expressions
    if constexpr (is_expression<Expr>) {
      using Op = get_op_t<Expr>;

      // Check if this operation uses canonical form
      if constexpr (uses_canonical_form<Op>) {
        // Extract args and canonicalize
        using ArgsTuple = get_args_t<Expr>;
        return MakeCanonicalFromTuple_t<Op, ArgsTuple>{};
      } else {
        // Not a canonical operation, return unchanged
        return expr;
      }
    } else {
      // Not an expression, return unchanged
      return expr;
    }
  }
};

constexpr inline Canonicalize to_canonical{};

}  // namespace tempura::symbolic3
