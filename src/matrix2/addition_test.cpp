#include "matrix2/addition.h"

#include "matrix2/storage/dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Basic Addition"_test = [] {
    constexpr matrix::InlineDense<double, 2, 2> m{{0., 1.}, {2., 3.}};
    constexpr matrix::InlineDense<double, 2, 2> n{{4., 5.}, {6., 7.}};
    constexpr matrix::InlineDense<double, 2, 2> o{{4., 6.}, {8., 10.}};
    static_assert(m + n == o);
  };

  "Add into"_test = [] {
    constexpr matrix::InlineDense<double, 2, 2> m{{0., 1.}, {2., 3.}};
    constexpr matrix::InlineDense<double, 2, 2> n{{4., 5.}, {6., 7.}};
    constexpr auto o = [&] {
      matrix::InlineDense<double, 2, 2> o = m;
      o += n;
      return o;
    }();
    constexpr matrix::InlineDense<double, 2, 2> expected{{4., 6.}, {8., 10.}};
    static_assert(o == expected);
  };

  "Basic Subtraction"_test = [] {
    constexpr matrix::InlineDense<double, 2, 2> m{{0., 1.}, {2., 3.}};
    constexpr matrix::InlineDense<double, 2, 2> n{{4., 5.}, {6., 7.}};
    constexpr matrix::InlineDense<double, 2, 2> o{{-4., -4.}, {-4., -4.}};
    static_assert(m - n == o);
  };

  "Subtract into"_test = [] {
    constexpr matrix::InlineDense<double, 2, 2> m{{0., 1.}, {2., 3.}};
    constexpr matrix::InlineDense<double, 2, 2> n{{4., 5.}, {6., 7.}};
    constexpr auto o = [&] {
      matrix::InlineDense<double, 2, 2> o = m;
      o -= n;
      return o;
    }();
    constexpr matrix::InlineDense<double, 2, 2> expected{{-4., -4.}, {-4., -4.}};
    static_assert(o == expected);
  };

  return TestRegistry::result();
};

