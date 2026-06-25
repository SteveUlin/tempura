#pragma once

#include <meta>
#include <vector>

#include "meta/tags.h"

// Type list operations via P2996 reflection.
// template for + substitute replace recursive template instantiation.

namespace tempura {

// Concat — merge multiple TypeLists
template <typename... Lists>
struct Concat;

template <>
struct Concat<> {
  using Type = TypeList<>;
};

template <typename... Ts>
struct Concat<TypeList<Ts...>> {
  using Type = TypeList<Ts...>;
};

template <typename... Ts, typename... Us, typename... Rest>
struct Concat<TypeList<Ts...>, TypeList<Us...>, Rest...> {
  using Type = typename Concat<TypeList<Ts..., Us...>, Rest...>::Type;
};

template <typename... Lists>
using Concat_t = typename Concat<Lists...>::Type;

// Filter — keep types matching predicate (recursive template, no heap in consteval)
template <template <typename> class Pred, typename List, typename Acc = TypeList<>>
struct Filter;

// Base: list exhausted — return accumulator
template <template <typename> class Pred, typename Acc>
struct Filter<Pred, TypeList<>, Acc> {
  using Type = Acc;
};

// Head matches predicate: append to accumulator, recurse
template <template <typename> class Pred, typename Head, typename... Tail, typename... Acc>
struct Filter<Pred, TypeList<Head, Tail...>, TypeList<Acc...>>
    : std::conditional_t<
          Pred<Head>::value,
          Filter<Pred, TypeList<Tail...>, TypeList<Acc..., Head>>,
          Filter<Pred, TypeList<Tail...>, TypeList<Acc...>>> {};

template <template <typename> class Pred, typename List>
using Filter_t = typename Filter<Pred, List>::Type;

// Transform — apply metafunction to each element
template <template <typename> class MetaFn, typename List>
struct Transform;

template <template <typename> class MetaFn, typename... Ts>
struct Transform<MetaFn, TypeList<Ts...>> {
 private:
  static consteval auto compute() -> std::meta::info {
    std::vector<std::meta::info> result;
    template for (constexpr auto arg : std::meta::template_arguments_of(^^TypeList<Ts...>)) {
      using Elem = [:arg:];
      result.push_back(^^typename MetaFn<Elem>::Type);
    }
    return std::meta::substitute(^^TypeList, result);
  }

 public:
  using Type = [:compute():];
};

template <template <typename> class MetaFn, typename List>
using Transform_t = typename Transform<MetaFn, List>::Type;

// FlatMap — transform each element into a TypeList, then concatenate
template <template <typename> class MetaFn, typename List>
struct FlatMap;

template <template <typename> class MetaFn, typename... Ts>
struct FlatMap<MetaFn, TypeList<Ts...>> {
 private:
  static consteval auto compute() -> std::meta::info {
    std::vector<std::meta::info> result;
    template for (constexpr auto arg : std::meta::template_arguments_of(^^TypeList<Ts...>)) {
      // MetaFn<T>::Type must be a TypeList — unpack its args
      using Elem = [:arg:];
      using Mapped = typename MetaFn<Elem>::Type;
      for (auto inner : std::meta::template_arguments_of(^^Mapped)) {
        result.push_back(inner);
      }
    }
    return std::meta::substitute(^^TypeList, result);
  }

 public:
  using Type = [:compute():];
};

template <template <typename> class MetaFn, typename List>
using FlatMap_t = typename FlatMap<MetaFn, List>::Type;

// Contains — O(N) consteval loop, no type instantiation.
// Uses a regular for loop (not template for) since we only compare info values,
// no splicing needed. template for with template_arguments_of has heap-allocation
// issues in clang-p2996 that prevent constexpr range initialization.
template <typename T, typename List>
struct Contains;

template <typename T, typename... Ts>
struct Contains<T, TypeList<Ts...>> {
 private:
  static consteval bool compute() {
    auto args = std::meta::template_arguments_of(^^TypeList<Ts...>);
    for (auto arg : args) {
      if (^^T == arg) return true;
    }
    return false;
  }

 public:
  static constexpr bool value = compute();
};

template <typename T, typename List>
inline constexpr bool Contains_v = Contains<T, List>::value;

// Unique — remove duplicates (O(N²) consteval, no recursive instantiation).
// Same as Contains: uses regular for loops to avoid template for heap issues.
template <typename List>
struct Unique;

template <typename... Ts>
struct Unique<TypeList<Ts...>> {
 private:
  static consteval auto compute() -> std::meta::info {
    auto all_args = std::meta::template_arguments_of(^^TypeList<Ts...>);
    std::vector<std::meta::info> result;
    for (auto arg : all_args) {
      bool found = false;
      for (auto existing : result) {
        if (existing == arg) {
          found = true;
          break;
        }
      }
      if (!found) result.push_back(arg);
    }
    return std::meta::substitute(^^TypeList, result);
  }

 public:
  using Type = [:compute():];
};

template <typename List>
using Unique_t = typename Unique<List>::Type;

}  // namespace tempura
