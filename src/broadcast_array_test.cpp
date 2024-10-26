#include "broadcast_array.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Addition"_test = [] {
    {
      constexpr BroadcastArray a{1, 2, 3};
      constexpr BroadcastArray b{4, 5, 6};
      static_assert((a + b) == BroadcastArray{5, 7, 9});
    }
    {
      BroadcastArray a{1, 2, 3};
      BroadcastArray b{4, 5, 6};
      a += b;
      expectRangeEq(a, BroadcastArray{5, 7, 9});
    }
    {
      constexpr BroadcastArray a{1, 2, 3};
      static_assert(a + 4 == BroadcastArray{5, 6, 7});
      static_assert(4 + a == BroadcastArray{5, 6, 7});
    }
    {
      BroadcastArray a{1, 2, 3};
      a += 4;
      expectRangeEq(a, BroadcastArray{5, 6, 7});
    }
  };

  "Subtraction"_test = [] {
    {
      constexpr BroadcastArray a{1, 2, 3};
      constexpr BroadcastArray b{4, 5, 6};
      static_assert((a - b) == BroadcastArray{-3, -3, -3});
    }
    {
      BroadcastArray a{1, 2, 3};
      BroadcastArray b{4, 5, 6};
      a -= b;
      expectRangeEq(a, BroadcastArray{-3, -3, -3});
    }
    {
      constexpr BroadcastArray a{1, 2, 3};
      static_assert(a - 4 == BroadcastArray{-3, -2, -1});
      static_assert(4 - a == BroadcastArray{3, 2, 1});
    }
    {
      BroadcastArray a{1, 2, 3};
      a -= 4;
      expectRangeEq(a, BroadcastArray{-3, -2, -1});
    }
  };

  "Multiplication"_test = [] {
    {
      constexpr BroadcastArray a{1, 2, 3};
      constexpr BroadcastArray b{4, 5, 6};
      static_assert((a * b) == BroadcastArray{4, 10, 18});
    }
    {
      BroadcastArray a{1, 2, 3};
      BroadcastArray b{4, 5, 6};
      a *= b;
      expectRangeEq(a, BroadcastArray{4, 10, 18});
    }
    {
      constexpr BroadcastArray a{1, 2, 3};
      static_assert(a * 4 == BroadcastArray{4, 8, 12});
      static_assert(4 * a == BroadcastArray{4, 8, 12});
    }
    {
      BroadcastArray a{1, 2, 3};
      a *= 4;
      expectRangeEq(a, BroadcastArray{4, 8, 12});
    }
  };

  "Division"_test = [] {
    {
      constexpr BroadcastArray a{1., 2., 3.};
      constexpr BroadcastArray b{2., 4., 6.};
      static_assert((a / b) == BroadcastArray{.5, .5, .5});
    }
    {
      BroadcastArray a{1., 2., 3.};
      BroadcastArray b{2., 4., 6.};
      a /= b;
      expectRangeEq(a, BroadcastArray{.5, .5, .5});
    }
    {
      constexpr BroadcastArray a{1., 2., 4.};
      static_assert(a / 4. == BroadcastArray{.25, .50, 1.0});
      static_assert(4. / a == BroadcastArray{4., 2., 1.});
    }
    {
      BroadcastArray a{1., 2., 4.};
      a /= 4;
      expectRangeEq(a, BroadcastArray{.25, .50, 1.0});
    }
  };

  "Negation"_test = [] {
    {
      constexpr BroadcastArray a{1., 2., 3.};
      static_assert(-a == BroadcastArray{-1., -2., -3.});
    }
  };

  "Positive"_test = [] {
    {
      constexpr BroadcastArray a{1., 2., 3.};
      static_assert(+a == BroadcastArray{1., 2., 3.});
    }
  };

  "Exponential"_test = [] {
    {
      constexpr BroadcastArray a{1., 2., 3.};
      static_assert(exp(a) ==
                    BroadcastArray{std::exp(1.), std::exp(2.), std::exp(3.)});
    }
  };

  "Logarithm"_test = [] {
    {
      constexpr BroadcastArray a{1., 2., 3.};
      static_assert(log(a) ==
                    BroadcastArray{std::log(1.), std::log(2.), std::log(3.)});
    }
  };

  "Square Root"_test = [] {
    {
      constexpr BroadcastArray a{1., 2., 3.};
      static_assert(sqrt(a) == BroadcastArray{std::sqrt(1.), std::sqrt(2.),
                                              std::sqrt(3.)});
    }
  };

  "Sin"_test = [] {
    {
      constexpr BroadcastArray a{0., 1., 2.};
      static_assert(sin(a) ==
                    BroadcastArray{std::sin(0.), std::sin(1.), std::sin(2.)});
    }
  };

  "Cos"_test = [] {
    {
      constexpr BroadcastArray a{0., 1., 2.};
      static_assert(cos(a) ==
                    BroadcastArray{std::cos(0.), std::cos(1.), std::cos(2.)});
    }
  };

  "Tan"_test = [] {
    {
      constexpr BroadcastArray a{0., 1., 2.};
      expectRangeEq(tan(a),
                    BroadcastArray{std::tan(0.), std::tan(1.), std::tan(2.)});
    }
  };

  "Arcsin"_test = [] {
    {
      constexpr BroadcastArray a{0., 1., 2.};
      expectRangeEq(
          asin(a), BroadcastArray{std::asin(0.), std::asin(1.), std::asin(2.)});
    }
  };

  "Arccos"_test = [] {
    {
      constexpr BroadcastArray a{0., 1., 2.};
      expectRangeEq(
          acos(a), BroadcastArray{std::acos(0.), std::acos(1.), std::acos(2.)});
    }
  };

  "Arctan"_test = [] {
    {
      constexpr BroadcastArray a{0., 1., 2.};
      expectRangeEq(
          atan(a), BroadcastArray{std::atan(0.), std::atan(1.), std::atan(2.)});
    }
  };
}
