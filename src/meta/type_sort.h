#pragma once

#include "meta/tags.h"

// Compile-time type sorting for TypeList
// Simple API that can be updated with better implementations later
// Zero runtime overhead - all computation at compile-time

namespace tempura {

// ============================================================================
// Public Interface
// ============================================================================

// Sort a TypeList using a comparison predicate
// Compare should be a type with constexpr operator() that takes two types
// and returns true if the first should come before the second
//
// Current implementation: Insertion sort (simple, correct, slow for large
// lists) Future: Can be replaced with heapsort, mergesort, or other algorithms
template <typename List, typename Compare>
struct SortTypes;

// Empty and single-element lists are already sorted
template <typename Compare>
struct SortTypes<TypeList<>, Compare> {
  using type = TypeList<>;
};

template <typename T, typename Compare>
struct SortTypes<TypeList<T>, Compare> {
  using type = TypeList<T>;
};

// Insert a type into a sorted list
template <typename T, typename List, typename Compare>
struct InsertSorted;

template <typename T, typename Compare>
struct InsertSorted<T, TypeList<>, Compare> {
  using type = TypeList<T>;
};

// Helper to prepend a type to a TypeList
template <typename T, typename List>
struct Prepend;

template <typename T, typename... Rest>
struct Prepend<T, TypeList<Rest...>> {
  using type = TypeList<T, Rest...>;
};

template <typename T, typename List>
using Prepend_t = typename Prepend<T, List>::type;

template <typename T, typename First, typename... Rest, typename Compare>
struct InsertSorted<T, TypeList<First, Rest...>, Compare> {
  using type = std::conditional_t<
      Compare{}(T{}, First{}),
      TypeList<T, First, Rest...>,  // T goes before First
      Prepend_t<First, typename InsertSorted<T, TypeList<Rest...>, Compare>::
                           type>  // First stays, recurse
      >;
};

// Insertion sort implementation
template <typename First, typename... Rest, typename Compare>
struct SortTypes<TypeList<First, Rest...>, Compare> {
  using sorted_rest = typename SortTypes<TypeList<Rest...>, Compare>::type;
  using type = typename InsertSorted<First, sorted_rest, Compare>::type;
};

template <typename List, typename Compare>
using SortTypes_t = typename SortTypes<List, Compare>::type;

}  // namespace tempura
