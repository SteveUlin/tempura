#pragma once

#include <meta>

#include "meta/tags.h"
#include "meta/utility.h"

// Compile-time type list operations via P2996 reflection.
// template_arguments_of + splice replace recursive template specialization.

namespace tempura {

// TypeList defined in meta/tags.h

// Get<I, TypeList<...>> — extract type at index via splice
template <SizeT Idx, typename... Types>
struct Get;

// TypeList-wrapped: Get<1, TypeList<int, double>> → double
template <SizeT Idx, typename... Types>
struct Get<Idx, TypeList<Types...>> {
  using type = [:std::meta::template_arguments_of(^^TypeList<Types...>)[Idx]:];
};

// Raw type list: Get<0, int, double> → int (used by CompressedTuple)
template <SizeT Idx, typename First, typename... Rest>
struct Get<Idx, First, Rest...> {
  using type = [:std::meta::template_arguments_of(^^TypeList<First, Rest...>)[Idx]:];
};

template <SizeT Idx, typename... Types>
using Get_t = typename Get<Idx, Types...>::type;

// Head — first element
template <typename... Types>
struct Head;

template <typename... Types>
struct Head<TypeList<Types...>> {
  using type = [:std::meta::template_arguments_of(^^TypeList<Types...>)[0]:];
};

template <typename First, typename... Rest>
struct Head<First, Rest...> {
  using type = First;
};

template <typename... Types>
using Head_t = typename Head<Types...>::type;

// Tail — all elements except the first
template <typename... Types>
struct Tail;

template <typename First, typename... Rest>
struct Tail<TypeList<First, Rest...>> {
  using type = TypeList<Rest...>;
};

template <typename First, typename... Rest>
struct Tail<First, Rest...> {
  using type = TypeList<Rest...>;
};

template <typename... Types>
using Tail_t = typename Tail<Types...>::type;

// Size — number of elements
template <typename... Types>
struct Size;

template <typename... Types>
struct Size<TypeList<Types...>> {
  static constexpr SizeT value = sizeof...(Types);
};

template <typename T>
constexpr SizeT Size_v = Size<T>::value;

}  // namespace tempura
