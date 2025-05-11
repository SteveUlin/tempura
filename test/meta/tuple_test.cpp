#include "meta/tuple.h"

#include "unit.h"

using namespace tempura;

auto fail(auto a) {
  static_assert(false, "This should never be reached");
}

auto main() -> int {
  "Tuple"_test = [] {
    constexpr Tuple<int, double, char> t{1, 2.0, 'a'};
    static_assert(t.get<0>() == 1);
    static_assert(t.get<1>() == 2.0);
    static_assert(t.get<2>() == 'a');
  };

  "Tuple Mutation"_test = [] {
    constexpr Tuple<int, double, char> t{1, 2.0, 'a'};
    static_assert(t.get<0>() == 1);
    static_assert(t.get<1>() == 2.0);
    static_assert(t.get<2>() == 'a');

    constexpr auto t2 = [] {
      TupleImpl<IndexSequence<0, 1>, int, char> tup{0, 'a'};
      tup.get<0>() = 1;
      return tup;
    }();

    static_assert(t2.get<0>() == 1);
    static_assert(t2.get<1>() == 'a');
  };

  "Apply"_test = [] {
    constexpr auto f = [](int a, double b) {
      return a + b;
    };
    constexpr Tuple<int, double> t{1, 2.0};
    static_assert(apply(f, t) == 3.0);
  };

  return 0;
}
