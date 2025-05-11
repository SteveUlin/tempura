#pragma once

#include <cstddef>
#include <ranges>
#include <utility>

#include "utility/overloaded.h"

namespace tempura {

// Generalize getters for any indexable container, tuple, array or range.
// The getters do not check the bounds of the index, so be careful when using
// them.

// Implementation Note: Rank1 is preferred over Rank0.

// --- atIndex ---

template <std::size_t I, typename T>
constexpr auto atIndexImpl(Rank1 /*unused*/, T&& t) -> decltype(auto) {
  return t[I];
}

template <std::size_t I, typename T>
constexpr auto atIndexImpl(Rank0 /*unused*/, T&& t) -> decltype(auto) {
  return std::get<I>(std::forward<T>(t));
}

template <std::size_t I, typename T>
constexpr auto atIndex(T&& t) -> decltype(auto) {
  return atIndexImpl<I>(Rank1{}, std::forward<T>(t));
}

// --- sizeOf ---

template <typename T>
constexpr auto sizeOfImpl(Rank1 /*unused*/, T&& t) -> decltype(auto) {
  return std::tuple_size<std::decay_t<T>>::value;
}

template <typename T>
constexpr auto sizeOfImpl(Rank0 /*unused*/, T&& t) -> decltype(auto) {
  return std::ranges::size(t);
}

template <typename T>
constexpr auto sizeOf(T&& t) -> decltype(auto) {
  return sizeOfImpl(Rank1{}, std::forward<T>(t));
}

// --- forEach ---

// Prefer range based loops
template <typename T, typename Func>
constexpr void forEachImpl(Rank1 /*unused*/, T&& t, Func&& func) {
  for (auto& elem : t) {
    func(elem);
  }
}

template <typename T, typename Func>
constexpr void forEachImpl(Rank0 /*unused*/, T&& t, Func&& func) {
  [&]<std::size_t... I>(std::index_sequence<I...>) {
    (func(atIndex<I>(t)), ...);
  }(std::make_index_sequence<sizeOf<0>(t)>());
}

// --- innerProduct ---
template <typename T, typename U, typename V, typename CombineFunc,
          typename ReduceFunc>
constexpr auto innerProduct(Rank1 /*unused*/, T&& t, U&& u, V init,
                            CombineFunc&& combineFunc,
                            ReduceFunc&& reduceFunc) {
  forEach(std::forward<T>(t), [&](auto&& elem) {
    init = reduceFunc(init, combineFunc(std::forward<decltype(elem)>(elem),
                                        std::forward<U>(u)));
  });
  return init;
}

}  // namespace tempura
