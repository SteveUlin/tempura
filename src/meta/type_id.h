#pragma once

#include "meta/tags.h"
#include "meta/utility.h"

// meta.h provides value based template meta-programming utilities.
//
// For fast compilation times, meta.h does not rely on any external imports

namespace tempura {

enum class TypeId : SizeT {};

template <TypeId>
struct TypeIdResolver {
  consteval friend auto typeIdResolverFriend(TypeIdResolver);
};

// This is a meta-programming utility to generate unique type ids at
// compile-time. It uses a binary search algorithm to find the next available
// type id by checking if the type id is already used.
template <class T>
struct Meta {
  using ValueType = T;
  template <SizeT N>
  static consteval auto grow() -> SizeT {
    if constexpr (requires {
                    typeIdResolverFriend(TypeIdResolver<TypeId{N}>{});
                  }) {
      return grow<N * 2>();
    } else {
      return N;
    }
  }

  template <SizeT left, SizeT right>
  static consteval auto gen() -> SizeT {
    if constexpr (left >= right) {
      return left +
             requires { typeIdResolverFriend(TypeIdResolver<TypeId{left}>{}); };
    } else if constexpr (constexpr auto mid = left + (right - left) / 2U;
                         requires {
                           typeIdResolverFriend(TypeIdResolver<TypeId{mid}>{});
                         }) {
      return gen<mid + 1U, right>();
    } else {
      return gen<left, mid - 1U>();
    }
  }
  static constexpr auto id = [] {
    constexpr auto max = grow<1U>();
    return TypeId{gen<max / 2, max>()};
  }();
  consteval auto friend typeIdResolverFriend(TypeIdResolver<id> /*unused*/) {
    return Meta{};
  }
};

// --- Public API ---

template <typename T>
constexpr TypeId kMeta = Meta<T>::id;

template <typename T>
constexpr auto getTypeId(T) {
  return kMeta<T>;
}

template <TypeId value>
using TypeOf =
    decltype(typeIdResolverFriend(TypeIdResolver<value>{}))::ValueType;

template <template <typename...> typename T>
constexpr auto withTypes(Invocable auto&& expr) {
  constexpr Range auto range = expr();
  return [range]<SizeT... Ns>(IndexSequence<Ns...>) {
    return T<TypeOf<range[Ns]>...>{};
  }(MakeIndexSequence<range.size()>());
}

}  // namespace tempura
