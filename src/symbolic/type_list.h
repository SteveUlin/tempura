#pragma once

#include <cstddef>
#include <type_traits>

namespace tempura::symbolic {

// Compile-time list of types supporting head/tail operations.
template <typename... Args>
struct TypeList;

// Base case: empty type list.
template <>
struct TypeList<> {
  consteval TypeList() = default;

  static constexpr bool empty = true;
  static constexpr size_t size = 0;
};

// Recursive case: non-empty type list with head and tail.
template <typename First, typename... Rest>
struct TypeList<First, Rest...> {
  consteval TypeList() = default;
  consteval TypeList(First /*unused*/, Rest... /*unused*/) {}

  using HeadType = First;
  using TailType = TypeList<Rest...>;
  static constexpr bool empty = false;
  static constexpr size_t size = 1 + sizeof...(Rest);

  static constexpr auto head() -> HeadType { return {}; }
  static constexpr auto tail() -> TailType { return {}; }
};

template <typename... Args>
TypeList(Args...) -> TypeList<Args...>;

// Implementation helper for concatenating two TypeLists.
template <typename Lhs, typename Rhs>
struct ConcatImpl;

template <typename... Lhs, typename... Rhs>
struct ConcatImpl<TypeList<Lhs...>, TypeList<Rhs...>> {
  using Type = TypeList<Lhs..., Rhs...>;
};

template <typename Lhs, typename Rhs>
using Concat = ConcatImpl<Lhs, Rhs>::Type;

// Concatenate one or more TypeLists.
template <typename... Lhs>
consteval auto concat(TypeList<Lhs...> /*unused*/) {
  return TypeList<Lhs...>{};
}

template <typename... Lhs, typename... Rhs>
consteval auto concat(TypeList<Lhs...> /*unused*/,
                      TypeList<Rhs...> /*unused*/) {
  return TypeList<Lhs..., Rhs...>{};
}

template <typename First, typename Second, typename... Args>
consteval auto concat(First /*unused*/, Second /*unused*/, Args... /*unused*/) {
  return concat(concat(First{}, Second{}), Args{}...);
}

// Type-based equality for TypeLists.
template <typename... Lhs, typename... Rhs>
consteval auto operator==(TypeList<Lhs...> /*unused*/,
                          TypeList<Rhs...> /*unused*/) -> bool {
  return std::is_same_v<TypeList<Lhs...>, TypeList<Rhs...>>;
}

template <typename... Lhs, typename... Rhs>
consteval auto operator!=(TypeList<Lhs...> /*unused*/,
                          TypeList<Rhs...> /*unused*/) -> bool {
  return not std::is_same_v<TypeList<Lhs...>, TypeList<Rhs...>>;
}

// template <typename T, typename U>
// struct TypeNameCmp : std::conditional_t<(typeName<T>() < typeName<U>()),
//                                         std::true_type, std::false_type> {};
//
// template <typename T, typename U>
// struct TypeNameLess : std::conditional_t<(typeName<T>() < typeName<U>()),
//                                          std::true_type, std::false_type> {};
//
// template <typename T, typename U>
// struct TypeNameEq : std::conditional_t<(typeName<T>() == typeName<U>()),
//                                        std::true_type, std::false_type> {};
//
// template <typename T>
// struct Not {
//   constexpr Not() = default;
//
//   template <typename U>
//   static constexpr auto operator()() -> bool {
//     return !(T::template operator()<U>());
//   }
// };
//
// template <template <typename, typename> typename Cmp, typename... Args>
// consteval auto s(TypeList<Args...> list) {
//   if constexpr (sizeof...(Args) <= 1) {
//     return TypeList<Args...>{};
//   } else {
//     // not constexpr to not add const to the output type
//     auto pivot = list.head();
//     auto lhs = list.tail() >>= [](auto... terms) {
//       return concat(
//           std::conditional_t<!Cmp<decltype(pivot), decltype(terms)>::value,
//                              TypeList<decltype(terms)>, TypeList<>>{}...);
//     };
//     auto rhs = list.tail() >>= [](auto... terms) {
//       return concat(
//           std::conditional_t<Cmp<decltype(pivot), decltype(terms)>::value,
//                              TypeList<decltype(terms)>, TypeList<>>{}...);
//     };
//     return concat(sort<Cmp>(lhs), TypeList<decltype(pivot)>{},
//     sort<Cmp>(rhs));
//   }
// }
//
// namespace internal {
//
// template <template <typename, typename> typename Cmp, typename... Lhs,
//           typename... Rhs>
//   requires(sizeof...(Lhs) > 0)
// consteval auto takeWhile(TypeList<Lhs...> lhs, TypeList<Rhs...> rhs) {
//   if constexpr (rhs.empty()) {
//     return TypeList{lhs, TypeList{}};
//   } else if constexpr (Cmp<decltype(lhs.head()),
//   decltype(rhs.head())>::value) {
//     return takeWhile<Cmp>(concat(lhs, TypeList{rhs.head()}), rhs.tail());
//   } else {
//     return TypeList{lhs, rhs};
//   }
// }
//
// }  // namespace internal
//
// template <template <typename, typename> typename Cmp, typename... Args>
// consteval auto groupBy(TypeList<Args...> list) {
//   if constexpr (list.empty()) {
//     return TypeList{};
//   } else {
//     auto group = internal::takeWhile<Cmp>(TypeList{list.head()},
//     list.tail()); auto lhs = group.template get<0>(); auto rhs =
//     group.template get<1>(); if constexpr (rhs.empty()) {
//       return TypeList{lhs};
//     } else {
//       return concat(TypeList<decltype(TypeList{lhs})>{},
//                     TypeList<decltype(groupBy<Cmp>(rhs))>{});
//     }
//   }
// }

}  // namespace tempura::symbolic
