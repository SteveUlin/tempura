#pragma once

#include "meta/tags.h"

// Type list operations for completion signature manipulation
// Extends the basic type_list.h with operations needed for P2300

namespace tempura {

// ============================================================================
// Concatenate - Merge multiple TypeLists
// ============================================================================

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

// ============================================================================
// Filter - Keep only types matching a predicate
// ============================================================================

template <template <typename> class Pred, typename List>
struct Filter;

template <template <typename> class Pred>
struct Filter<Pred, TypeList<>> {
  using Type = TypeList<>;
};

template <template <typename> class Pred, typename T, typename... Rest>
struct Filter<Pred, TypeList<T, Rest...>> {
  using RestFiltered = typename Filter<Pred, TypeList<Rest...>>::Type;
  using Type = std::conditional_t<Pred<T>::value,
                                  Concat_t<TypeList<T>, RestFiltered>,
                                  RestFiltered>;
};

template <template <typename> class Pred, typename List>
using Filter_t = typename Filter<Pred, List>::Type;

// ============================================================================
// Transform - Apply a metafunction to each element
// ============================================================================

template <template <typename> class MetaFn, typename List>
struct Transform;

template <template <typename> class MetaFn>
struct Transform<MetaFn, TypeList<>> {
  using Type = TypeList<>;
};

template <template <typename> class MetaFn, typename T, typename... Rest>
struct Transform<MetaFn, TypeList<T, Rest...>> {
  using Type = Concat_t<TypeList<typename MetaFn<T>::Type>,
                        typename Transform<MetaFn, TypeList<Rest...>>::Type>;
};

template <template <typename> class MetaFn, typename List>
using Transform_t = typename Transform<MetaFn, List>::Type;

// ============================================================================
// FlatMap - Transform each element and concatenate results
// ============================================================================

// MetaFn should return a TypeList
template <template <typename> class MetaFn, typename List>
struct FlatMap;

template <template <typename> class MetaFn>
struct FlatMap<MetaFn, TypeList<>> {
  using Type = TypeList<>;
};

template <template <typename> class MetaFn, typename T, typename... Rest>
struct FlatMap<MetaFn, TypeList<T, Rest...>> {
  using Type = Concat_t<typename MetaFn<T>::Type,
                        typename FlatMap<MetaFn, TypeList<Rest...>>::Type>;
};

template <template <typename> class MetaFn, typename List>
using FlatMap_t = typename FlatMap<MetaFn, List>::Type;

// ============================================================================
// Contains - Check if a type is in the list
// ============================================================================

template <typename T, typename List>
struct Contains;

template <typename T>
struct Contains<T, TypeList<>> : std::false_type {};

template <typename T, typename First, typename... Rest>
struct Contains<T, TypeList<First, Rest...>>
    : std::conditional_t<std::is_same_v<T, First>, std::true_type,
                         Contains<T, TypeList<Rest...>>> {};

template <typename T, typename List>
inline constexpr bool Contains_v = Contains<T, List>::value;

// ============================================================================
// Unique - Remove duplicate types
// ============================================================================

template <typename List>
struct Unique;

template <>
struct Unique<TypeList<>> {
  using Type = TypeList<>;
};

template <typename T, typename... Rest>
struct Unique<TypeList<T, Rest...>> {
  using RestUnique = typename Unique<TypeList<Rest...>>::Type;
  using Type =
      std::conditional_t<Contains_v<T, RestUnique>, RestUnique,
                         Concat_t<TypeList<T>, RestUnique>>;
};

template <typename List>
using Unique_t = typename Unique<List>::Type;

}  // namespace tempura
