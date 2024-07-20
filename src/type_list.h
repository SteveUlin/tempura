#pragma once

#include "type_name.h"

#include <cstddef>
#include <type_traits>

namespace tempura {

template<typename... Args>
consteval auto concat(Args...);

// An empty struct that holds a list of types
template <typename... Args>
struct TypeList {
  consteval TypeList() = default;
  consteval TypeList(Args...) requires (sizeof...(Args) > 0) {};

  // Get the list of types out of the TypeList by using a lambda
  // typelist.apply([](auto... args) { ... });
  static constexpr auto apply(auto func) {
    return func(Args{}...);
  }

  // A fancy way to write apply
  constexpr auto operator>>=(auto func) const {
    return apply(func);
  }

  static consteval auto empty() -> bool {
    return (sizeof...(Args) == 0);
  }

  static consteval auto size() -> size_t {
    return sizeof...(Args);
  }

  static consteval auto head()
  requires (sizeof...(Args) > 0) {
    return apply([](auto head, auto...) { return head; });
  };

  static consteval auto tail()
  requires (sizeof...(Args) > 0) {
    return apply([](auto, auto... rest) {
      return TypeList<decltype(rest)...>{};
    });
  }

  template <size_t N>
  static consteval auto get() {
    if constexpr (N == 0) {
      return head();
    } else {
      return tail().template get<N - 1>();
    }
  }

  template <template <typename> typename Predicate>
  static consteval auto filter() {
    return concat(std::conditional_t<Predicate<Args>::value, TypeList<Args>, TypeList<>>{}...);
  }

  template <template <typename> typename Predicate>
  static consteval auto invFilter() {
    return concat(std::conditional_t<Predicate<Args>::value, TypeList<>, TypeList<Args>>{}...);
  }
};

consteval auto concat() -> TypeList<> {
  return TypeList{};
}
template <typename... Args>
consteval auto concat(TypeList<Args...>) {
  return TypeList<Args...>{};
}

template <typename... Lhs, typename... Rhs>
consteval auto concat(TypeList<Lhs...>, TypeList<Rhs...>) {
  return TypeList<Lhs..., Rhs...>{};
}

template <typename... Args, typename... Rest>
consteval auto concat(TypeList<Args...>, Rest...) {
  return concat(TypeList<Args...>{}, concat(Rest{}...));
}

template <typename... Lhs, typename... Rhs>
consteval auto operator==(TypeList<Lhs...>, TypeList<Rhs...>) -> bool {
  return std::is_same_v<TypeList<Lhs...>, TypeList<Rhs...>>;
}

template <typename T, typename U>
struct TypeNameCmp : std::conditional_t<
    (typeName<T>() < typeName<U>()), std::true_type, std::false_type>{};

template <typename T, typename U>
struct TypeNameLess: std::conditional_t<
    (typeName<T>() < typeName<U>()), std::true_type, std::false_type>{};

template <typename T, typename U>
struct TypeNameEq: std::conditional_t<
    (typeName<T>() == typeName<U>()), std::true_type, std::false_type>{};

template <typename T>
struct Not {
  constexpr Not() = default;

  template <typename U>
  static constexpr auto operator()() -> bool {
    return !(T::template operator()<U>());
  }
};

template <template <typename, typename> typename Cmp, typename... Args>
consteval auto sort(TypeList<Args...> list) {
  if constexpr (sizeof...(Args) <= 1) {
    return TypeList<Args...>{};
  } else {
    // not constexpr to not add const to the output type
    auto pivot = list.head();
    auto lhs = list.tail() >>= [](auto... terms) {
      return concat(std::conditional_t<!Cmp<decltype(pivot), decltype(terms)>::value,
                                       TypeList<decltype(terms)>, TypeList<>>{}...);
    };
    auto rhs = list.tail() >>= [](auto... terms) {
      return concat(std::conditional_t<Cmp<decltype(pivot), decltype(terms)>::value,
                                       TypeList<decltype(terms)>, TypeList<>>{}...);
    };
    return concat(sort<Cmp>(lhs), TypeList<decltype(pivot)>{}, sort<Cmp>(rhs));
  }
}

namespace internal {

template <template <typename, typename> typename Cmp, typename... Lhs, typename... Rhs>
  requires (sizeof...(Lhs) > 0)
consteval auto takeWhile(TypeList<Lhs...> lhs, TypeList<Rhs...> rhs) {
  if constexpr (rhs.empty()) {
    return TypeList{lhs, TypeList{}};
  } else if constexpr (Cmp<decltype(lhs.head()), decltype(rhs.head())>::value) {
    return takeWhile<Cmp>(concat(lhs, TypeList{rhs.head()}), rhs.tail());
  } else {
    return TypeList{lhs, rhs};
  }
}

}  // namespace internal

template <template <typename, typename> typename Cmp, typename... Args>
consteval auto groupBy(TypeList<Args...> list) {
  if constexpr (list.empty()) {
    return TypeList{};
  } else {
    auto group = internal::takeWhile<Cmp>(TypeList{list.head()}, list.tail());
    auto lhs = group.template get<0>();
    auto rhs = group.template get<1>();
    if constexpr (rhs.empty()) {
      return TypeList{lhs};
    } else {
      return concat(TypeList<decltype(TypeList{lhs})>{}, TypeList<decltype(groupBy<Cmp>(rhs))>{});
    }
  }
}

} // namespace
