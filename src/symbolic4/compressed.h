#pragma once

#include "meta/type_list.h"
#include "meta/utility.h"

// ============================================================================
// compressed.h - Zero-overhead tuple storage for symbolic types
// ============================================================================
//
// Problem: std::tuple doesn't guarantee the Empty Base Optimization (EBO).
// When storing many empty types (like operator tags or strategy types), this
// wastes memory and prevents constexpr evaluation in some cases.
//
// Solution: Use C++20's [[no_unique_address]] attribute to eliminate storage
// for empty members. This is critical for symbolic expressions where most
// nodes are composed of empty tag types.
//
// Example:
//   struct Empty {};
//   Pair<int, Empty> p{42, {}};
//   static_assert(sizeof(p) == sizeof(int));  // Empty member takes no space
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Pair - Two-element compressed storage
// ============================================================================
//
// A simple pair where empty types consume no space. The [[no_unique_address]]
// attribute tells the compiler that the member's address doesn't need to be
// unique, allowing it to overlap with other members or padding.

template <typename A, typename B>
struct Pair {
  [[no_unique_address]] A first;
  [[no_unique_address]] B second;

  constexpr Pair() = default;
  constexpr Pair(A a, B b) : first{a}, second{b} {}
};

template <typename A, typename B>
Pair(A, B) -> Pair<A, B>;

// ============================================================================
// CompressedTuple - Variadic tuple with zero-overhead empty types
// ============================================================================
//
// Implementation uses multiple inheritance from indexed "leaf" types.
// Each element is stored in a TupleLeaf<Index, Type>, and the full tuple
// inherits from all leaves. This enables:
//   1. Type-based access via static_cast to the appropriate leaf
//   2. Zero storage for empty types via [[no_unique_address]]
//   3. Constant-time indexed access
//
// Usage:
//   CompressedTuple<int, double, char> t{1, 2.5, 'a'};
//   get<0>(t) == 1
//   get<1>(t) == 2.5
//   get<2>(t) == 'a'

namespace detail {

// Each tuple element is stored in an indexed leaf
template <SizeT I, typename T>
struct TupleLeaf {
  [[no_unique_address]] T value;

  constexpr TupleLeaf() = default;
  constexpr TupleLeaf(T v) : value{v} {}
};

// Tuple implementation inherits from all leaves
template <typename IndexSeq, typename... Ts>
struct TupleImpl;

template <SizeT... Is, typename... Ts>
struct TupleImpl<IndexSequence<Is...>, Ts...> : TupleLeaf<Is, Ts>... {
  constexpr TupleImpl() = default;
  constexpr TupleImpl(Ts... args)
    requires(sizeof...(Ts) > 0)
      : TupleLeaf<Is, Ts>{args}... {}
};

// Empty tuple specialization - avoids ambiguous constructor
template <>
struct TupleImpl<IndexSequence<>> {
  constexpr TupleImpl() = default;
};

}  // namespace detail

template <typename... Ts>
struct CompressedTuple : detail::TupleImpl<MakeIndexSequence<sizeof...(Ts)>, Ts...> {
  using Base = detail::TupleImpl<MakeIndexSequence<sizeof...(Ts)>, Ts...>;
  using Base::Base;

  static constexpr SizeT size = sizeof...(Ts);
};

template <typename... Ts>
CompressedTuple(Ts...) -> CompressedTuple<Ts...>;

// ============================================================================
// Element Access - get<I>(tuple)
// ============================================================================

template <SizeT I, typename... Ts>
constexpr auto get(const CompressedTuple<Ts...>& tuple) {
  return static_cast<const detail::TupleLeaf<I, Get_t<I, Ts...>>&>(tuple).value;
}

template <SizeT I, typename... Ts>
constexpr auto& get(CompressedTuple<Ts...>& tuple) {
  return static_cast<detail::TupleLeaf<I, Get_t<I, Ts...>>&>(tuple).value;
}

// ============================================================================
// Tuple Size Trait
// ============================================================================

template <typename T>
struct TupleSize;

template <typename... Ts>
struct TupleSize<CompressedTuple<Ts...>> {
  static constexpr SizeT value = sizeof...(Ts);
};

template <typename T>
constexpr SizeT tuple_size_v = TupleSize<T>::value;

}  // namespace tempura::symbolic4
