#pragma once

#include "meta/containers.h"
#include "meta/type_id.h"
#include "meta/utility.h"

namespace tempura {

template <SizeT N, typename... Args>
using TypeAt = TypeOf<(MinimalArray{kMeta<Args>...}[N])>;

template <SizeT N, typename T>
struct TupleLeaf {
  using ValueType = T;

  [[no_unique_address]] ValueType value;
};

template <typename Sequence, typename... Ts>
class TupleImpl;

// Empty tuple specialization
template <>
class TupleImpl<IndexSequence<>> {
 public:
  constexpr TupleImpl() = default;
};

template <SizeT... Is, typename... Ts>
class TupleImpl<IndexSequence<Is...>, Ts...> : public TupleLeaf<Is, Ts>... {
 public:
  constexpr TupleImpl() = default;
  constexpr TupleImpl(const TupleImpl&) = default;
  constexpr TupleImpl(TupleImpl&&) = default;
  constexpr TupleImpl& operator=(const TupleImpl&) = default;
  constexpr TupleImpl& operator=(TupleImpl&&) = default;

  template <typename... Args>
    requires(sizeof...(Args) == sizeof...(Ts) && sizeof...(Args) > 0)
  constexpr TupleImpl(Args&&... args)
      : TupleLeaf<Is, Ts>{static_cast<Args&&>(args)}... {}

  template <SizeT I>
  constexpr auto get() -> TypeAt<I, Ts...>& {
    return static_cast<TupleLeaf<I, TypeAt<I, Ts...>>*>(this)->value;
  }

  template <SizeT I>
  constexpr auto get() const -> const TypeAt<I, Ts...>& {
    return static_cast<const TupleLeaf<I, TypeAt<I, Ts...>>*>(this)->value;
  }
};

// Primary template
template <typename... Ts>
class Tuple : public TupleImpl<MakeIndexSequence<sizeof...(Ts)>, Ts...> {
 public:
  using Base = TupleImpl<MakeIndexSequence<sizeof...(Ts)>, Ts...>;

  constexpr Tuple() = default;
  constexpr Tuple(const Tuple&) = default;
  constexpr Tuple(Tuple&&) = default;
  constexpr Tuple& operator=(const Tuple&) = default;
  constexpr Tuple& operator=(Tuple&&) = default;

  template <typename... Args>
    requires(sizeof...(Args) == sizeof...(Ts) && sizeof...(Args) > 0)
  constexpr Tuple(Args&&... args) : Base{static_cast<Args&&>(args)...} {}

  template <SizeT I>
  constexpr auto get() -> TypeAt<I, Ts...>& {
    return Base::template get<I>();
  }

  template <SizeT I>
  constexpr auto get() const -> const TypeAt<I, Ts...>& {
    return Base::template get<I>();
  }
};

// Empty tuple specialization
template <>
class Tuple<> {
 public:
  constexpr Tuple() = default;
};

template <typename... Ts, typename... Us>
constexpr auto operator==(const Tuple<Ts...>& lhs, const Tuple<Us...>& rhs) -> bool {
  static_assert(sizeof...(Ts) == sizeof...(Us), "Tuple sizes must match");
  return [&]<SizeT... Is>(IndexSequence<Is...>) {
    return ((lhs.template get<Is>() == rhs.template get<Is>()) && ...);
  }(MakeIndexSequence<sizeof...(Ts)>());
}

template <typename... Ts>
Tuple(Ts...) -> Tuple<Ts...>;

// --- Tuple Size ---

template <typename T>
inline constexpr SizeT tupleSize = 0;

template <typename... Ts>
inline constexpr SizeT tupleSize<Tuple<Ts...>> = sizeof...(Ts);

template <typename... Ts>
inline constexpr SizeT tupleSize<const Tuple<Ts...>> = sizeof...(Ts);

template <>
inline constexpr SizeT tupleSize<Tuple<>> = 0;

template <>
inline constexpr SizeT tupleSize<const Tuple<>> = 0;

// --- Tuple Element ---

template <SizeT I, typename T>
struct TupleElement;

template <SizeT I, typename... Ts>
struct TupleElement<I, Tuple<Ts...>> {
  using Type = TypeAt<I, Ts...>;
};

template <SizeT I, typename T>
using TupleElementT = typename TupleElement<I, T>::Type;

// --- Free get function ---

template <SizeT N>
constexpr auto get(auto&& tup) -> decltype(auto) {
  return static_cast<decltype(tup)>(tup).template get<N>();
}

// --- makeTuple ---

template <typename... Ts>
constexpr auto makeTuple(Ts&&... args) -> Tuple<Ts...> {
  return Tuple<Ts...>{static_cast<Ts&&>(args)...};
}

// --- apply ---

template <typename Func, typename... Args>
constexpr auto apply(Func&& func, const Tuple<Args...>& tup) {
  return [&]<SizeT... Is>(IndexSequence<Is...>) {
    return static_cast<Func&&>(func)(tup.template get<Is>()...);
  }(MakeIndexSequence<sizeof...(Args)>());
}

template <typename Func, typename... Args>
constexpr auto apply(Func&& func, Tuple<Args...>&& tup) {
  return [&]<SizeT... Is>(IndexSequence<Is...>) {
    return static_cast<Func&&>(func)(
        static_cast<Args&&>(tup.template get<Is>())...);
  }(MakeIndexSequence<sizeof...(Args)>());
}

// Non-const lvalue reference overload
template <typename Func, typename... Args>
constexpr auto apply(Func&& func, Tuple<Args...>& tup) {
  return [&]<SizeT... Is>(IndexSequence<Is...>) {
    return static_cast<Func&&>(func)(tup.template get<Is>()...);
  }(MakeIndexSequence<sizeof...(Args)>());
}

// --- tupleCat ---

namespace tuple_detail {

template <typename T, typename U, SizeT... Is, SizeT... Js>
constexpr auto tupleCatImpl(const T& a, const U& b, IndexSequence<Is...>,
                            IndexSequence<Js...>) {
  return Tuple{a.template get<Is>()..., b.template get<Js>()...};
}

}  // namespace tuple_detail

template <typename... Ts, typename... Us>
constexpr auto tupleCat(const Tuple<Ts...>& a, const Tuple<Us...>& b) {
  return tuple_detail::tupleCatImpl(a, b, MakeIndexSequence<sizeof...(Ts)>{},
                                    MakeIndexSequence<sizeof...(Us)>{});
}

// Variadic tupleCat for 3+ tuples
template <typename T, typename U, typename V, typename... Rest>
constexpr auto tupleCat(const T& a, const U& b, const V& c, const Rest&... rest) {
  return tupleCat(tupleCat(a, b), c, rest...);
}

// Single tuple (copy)
template <typename... Ts>
constexpr auto tupleCat(const Tuple<Ts...>& t) -> Tuple<Ts...> {
  return t;
}

}  // namespace tempura

// ============================================================================
// Structured Binding Support
// ============================================================================
// C++ structured bindings require std::tuple_size and std::tuple_element
// specializations. The compile time benefit still comes from our simpler
// Tuple implementation - we're not instantiating std::tuple templates.

#include <tuple>  // for std::tuple_size, std::tuple_element declarations

namespace std {

template <typename... Ts>
struct tuple_size<tempura::Tuple<Ts...>> {
  static constexpr size_t value = sizeof...(Ts);
};

template <size_t I, typename... Ts>
struct tuple_element<I, tempura::Tuple<Ts...>> {
  using type = tempura::TypeAt<I, Ts...>;
};

}  // namespace std
