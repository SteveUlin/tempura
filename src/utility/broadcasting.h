#pragma once

// Defines the Broadcasting class template for working with tuples as if they
// were valarrays. That is, Broadcasting provides into a broadcasting interface
// for any tuple-like structure.
//
// Example usage:
//   std::tuple<int, double> t1{1, 2.0};
//   std::tuple<int, double> t2{3, 4.0};
//   Broadcasting{t1} += t2;
//   assert(std::get<0>(t1) == 4);
//   assert(std::get<1>(t1) == 6.0);

// Broadcasting objects are expected to be short lived and do not own
// the underlying data.
//
// If you create a broadcasting

#include <tuple>
#include <type_traits>
#include <utility>

namespace tempura {

// --- Concept for objects to behave list std::tuple ---
template <typename T>
concept TupleLike = requires(T t) {
  { std::tuple_size<T>::value } -> std::convertible_to<std::size_t>;
  typename std::tuple_element<0, T>::type;
  requires((std::tuple_size<T>::value == 0) || (requires { get<0>(t); }));
};

template <typename T>
  requires(TupleLike<std::remove_cv_t<T>>)
class TupleRef {
 public:
  TupleRef() = delete;

  constexpr TupleRef(T& tuple) : data_(&tuple) {}

  template <std::size_t I>
  constexpr auto get(this auto&& self) noexcept -> decltype(auto) {
    return std::get<I>(*self.data_);
  }

 private:
  T* data_;
};

template <typename T>
  requires(TupleLike<std::remove_cv_t<T>>)
auto tupleAllOf(T&& tuple) {
  if constexpr (std::is_reference_v<T>) {
    return TupleRef<std::remove_reference_t<T>>(tuple);
  } else {
    return std::forward<T>(tuple);
  }
}

}  // namespace tempura

namespace std {

template <typename T>
struct tuple_size<tempura::TupleRef<T>> {
  static constexpr std::size_t value =
      std::tuple_size<std::remove_cv_t<T>>::value;
};

template <std::size_t I, typename T>
constexpr auto get(tempura::TupleRef<T>& tuple) noexcept -> decltype(auto) {
  return tuple.template get<I>();
}

template <std::size_t I, typename T>
constexpr auto get(const tempura::TupleRef<T>& tuple) noexcept
    -> decltype(auto) {
  return tuple.template get<I>();
}

template <std::size_t I, typename T>
constexpr auto get(tempura::TupleRef<T>&& tuple) noexcept -> decltype(auto) {
  return std::move(tuple.template get<I>());
}

template <std::size_t I, typename T>
  requires(tempura::TupleLike<std::remove_cv_t<T>>)
struct tuple_element<I, tempura::TupleRef<T>> {
  using type = decltype(std::get<I>(std::declval<tempura::TupleRef<T>>()));
};

}  // namespace std
