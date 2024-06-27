#pragma once

#include "type_name.h"

#include <cstddef>

namespace tempura {

// A bunch of forward declarations because things are pretty interdependent
template <typename... Args>
struct TypeList;

namespace internal {

template <typename T>
struct TypeListTrait {
  constexpr static bool value = false;
};

template <typename... Args>
struct TypeListTrait<TypeList<Args...>> {
  constexpr static bool value = true;
};

} // namespace internal

template <typename T>
concept TypeListed = internal::TypeListTrait<T>::value;

constexpr auto concat(TypeListed auto first, TypeListed auto... rest);

// An empty struct that holds a list of types
template <typename... Args>
struct TypeList {
  constexpr TypeList() = default;
  constexpr TypeList(Args...) requires (sizeof...(Args) > 0) {};

  // Get the list of types out of the TypeList by using a lambda
  // typelist.apply([](auto... args) { ... });
  static constexpr auto apply(auto func) {
    return func(Args{}...);
  }

  // A fancy way to write apply
  constexpr auto operator>>=(auto func) const {
    return apply(func);
  }

  static constexpr auto empty() -> bool {
    return (sizeof...(Args) == 0);
  }

  static constexpr auto size() -> size_t {
    return sizeof...(Args);
  }

  static constexpr auto head()
  requires (sizeof...(Args) > 0) {
    return apply([](auto head, auto...) { return head; });
  };

  static constexpr auto tail()
  requires (sizeof...(Args) > 0) {
    return apply([](auto, auto... rest) {
      return TypeList<decltype(rest)...>{};
    });
  }

  template<typename Fn>
  static constexpr auto filter() {
    constexpr auto helper = []<typename Arg>() constexpr {
      if constexpr (Fn::template operator()<Arg>()) {
        return TypeList<Arg>{};
      } else {
        return TypeList<>{};
      }
    };
    return concat(helper.template operator()<Args>()...);
  }
};

consteval auto concat() -> TypeList<> {
  return TypeList{};
}

constexpr auto concat(TypeListed auto first, TypeListed auto... rest) {
  return first >>= [&](auto... firsts) {
    return concat(rest...) >>= [&](auto... rests) {
      return TypeList(firsts..., rests...);
    };
  };
}

namespace internal {

template <typename T>
struct TypeNameCmp {
  constexpr TypeNameCmp() = default;

  template <typename U>
  static constexpr auto operator()() -> bool {
    return typeName<T>() < typeName<U>();
  }
};

template <typename T>
struct Not {
  constexpr Not() = default;

  template <typename U>
  static constexpr auto operator()() -> bool {
    return !(T::template operator()<U>());
  }
};

} // namespace internal

template <template <typename> typename Fn, typename... Args>
constexpr auto sort(TypeList<Args...> list) {

  if constexpr (sizeof...(Args) <= 1) {
    return TypeList<Args...>{};
  } else {
    auto pivot = list.head();
    auto lhs = list.tail().template filter<internal::Not<Fn<decltype(pivot)>>>();
    auto rhs = list.tail().template filter<Fn<decltype(pivot)>>();

    return concat(sort<Fn>(lhs), TypeList<decltype(pivot)>{}, sort<Fn>(rhs));
  }
}



} // namespace
