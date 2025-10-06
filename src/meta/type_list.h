#pragma once

#include "meta/tags.h"
#include "meta/utility.h"

// Compile-time type list operations with zero STL dependencies
// Used for compile-time symbolic expression manipulation

namespace tempura {

// TypeList already defined in meta/tags.h, we just add operations

// ============================================================================
// Get - Extract type at index (STL-free)
// ============================================================================

namespace detail {
template <SizeT Idx, typename T, typename... Rest>
struct GetImpl {
  using type = typename GetImpl<Idx - 1, Rest...>::type;
};

template <typename T, typename... Rest>
struct GetImpl<0, T, Rest...> {
  using type = T;
};
}  // namespace detail

template <SizeT Idx, typename... Types>
struct Get {
  using type = typename detail::GetImpl<Idx, Types...>::type;
};

template <SizeT Idx, typename... Types>
struct Get<Idx, TypeList<Types...>> {
  using type = typename detail::GetImpl<Idx, Types...>::type;
};

template <SizeT Idx, typename... Types>
using Get_t = typename Get<Idx, Types...>::type;

// ============================================================================
// Head - Get first type
// ============================================================================

template <typename... Types>
struct Head;

template <typename First, typename... Rest>
struct Head<First, Rest...> {
  using type = First;
};

template <typename First, typename... Rest>
struct Head<TypeList<First, Rest...>> {
  using type = First;
};

template <typename... Types>
using Head_t = typename Head<Types...>::type;

// ============================================================================
// Tail - Get all types except first
// ============================================================================

template <typename... Types>
struct Tail;

template <typename First, typename... Rest>
struct Tail<First, Rest...> {
  using type = TypeList<Rest...>;
};

template <typename First, typename... Rest>
struct Tail<TypeList<First, Rest...>> {
  using type = TypeList<Rest...>;
};

template <typename... Types>
using Tail_t = typename Tail<Types...>::type;

// ============================================================================
// Size - Get number of types in list
// ============================================================================

template <typename... Types>
struct Size;

template <typename... Types>
struct Size<TypeList<Types...>> {
  static constexpr SizeT value = sizeof...(Types);
};

template <typename T>
constexpr SizeT Size_v = Size<T>::value;

}  // namespace tempura
