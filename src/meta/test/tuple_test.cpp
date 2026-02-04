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

  "tupleSize"_test = [] {
    static_assert(tupleSize<Tuple<int, double, char>> == 3);
    static_assert(tupleSize<Tuple<int>> == 1);
    static_assert(tupleSize<int> == 0);  // non-tuple
  };

  "TupleElementT"_test = [] {
    static_assert(isSame<TupleElementT<0, Tuple<int, double, char>>, int>);
    static_assert(isSame<TupleElementT<1, Tuple<int, double, char>>, double>);
    static_assert(isSame<TupleElementT<2, Tuple<int, double, char>>, char>);
  };

  "makeTuple"_test = [] {
    constexpr auto t = makeTuple(1, 2.0, 'a');
    static_assert(t.get<0>() == 1);
    static_assert(t.get<1>() == 2.0);
    static_assert(t.get<2>() == 'a');
  };

  "tupleCat two tuples"_test = [] {
    constexpr Tuple<int, double> a{1, 2.0};
    constexpr Tuple<char, float> b{'x', 3.0f};
    constexpr auto c = tupleCat(a, b);
    static_assert(tupleSize<decltype(c)> == 4);
    static_assert(c.get<0>() == 1);
    static_assert(c.get<1>() == 2.0);
    static_assert(c.get<2>() == 'x');
    static_assert(c.get<3>() == 3.0f);
  };

  "tupleCat three tuples"_test = [] {
    constexpr Tuple<int> a{1};
    constexpr Tuple<double> b{2.0};
    constexpr Tuple<char> c{'x'};
    constexpr auto d = tupleCat(a, b, c);
    static_assert(tupleSize<decltype(d)> == 3);
    static_assert(d.get<0>() == 1);
    static_assert(d.get<1>() == 2.0);
    static_assert(d.get<2>() == 'x');
  };

  "tupleCat single tuple"_test = [] {
    constexpr Tuple<int, double> a{1, 2.0};
    constexpr auto b = tupleCat(a);
    static_assert(b.get<0>() == 1);
    static_assert(b.get<1>() == 2.0);
  };

  "free get function"_test = [] {
    constexpr Tuple<int, double> t{42, 3.14};
    static_assert(get<0>(t) == 42);
    static_assert(get<1>(t) == 3.14);
  };

  "EBO - empty types take no space"_test = [] {
    struct Empty {};
    struct Empty2 {};
    static_assert(sizeof(Tuple<int, Empty>) == sizeof(int));
    static_assert(sizeof(Tuple<Empty, int>) == sizeof(int));
    static_assert(sizeof(Tuple<Empty, Empty2, int>) == sizeof(int));
  };

  return 0;
}
