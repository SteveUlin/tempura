#include "utility/broadcasting.h"

#include <print>

#include "unit.h"
#include "meta.h"

using namespace tempura;

template <std::size_t I, typename T>
constexpr auto get(tempura::TupleRef<T>& tuple) noexcept -> decltype(auto) {
  return tuple.template get<I>();
}

auto main() -> int {
  static_assert(TupleLike<std::tuple<int, double>>);
  static_assert(TupleLike<std::array<int, 2>>);
  static_assert(!TupleLike<int>);
  static_assert(TupleLike<TupleRef<std::tuple<int, double>>>);

  TypeOf<Meta<int>> i = 0;

  constexpr static std::tuple t1{1.0, 2.0};
  constexpr static TupleRef t1_ref{t1};
  static_assert(t1_ref.get<0>() == 1.0);
  static_assert(t1_ref.get<1>() == 2.0);
  const auto& [x, y] = t1_ref;
  std::println("t2 [{}, {}]", x, y);

  // static constexpr std::tuple<double, double> t1{1.0, 2.0};
  // static constexpr std::array<double, 2> a2{1.0, 2.0};
  // static constexpr Broadcasting b2(a2);

  // // static_assert(b1 == b2);
  // static_assert(b1.get<0>() == b2.get<0>());
  // static_assert(b1.get<1>() == b2.get<1>());
  // static_assert(std::get<0>(b1) == 1.0);

  // const auto& [x, y] = b1;
  // static_assert(x == 1.0);
  // static_assert(y == 2.0);

  // static_assert(b1 == t1);

  // std::tuple t2{3.0, 4.0};
  // std::tuple t3{5.0, 6.0};
  // Broadcasting{t2} += t3;
  // std::println("t2 [{}, {}]", std::get<0>(t2), std::get<1>(t2));
}
