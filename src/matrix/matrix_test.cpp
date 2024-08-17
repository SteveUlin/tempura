#include "matrix/matrix.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Set and get"_test = [] {
    matrix::Dense<int, 2, 2> m;
    expectEq(0, m[0, 1]);
    m[0, 1] = 2;
    expectEq(2, m[0, 1]);
  };

  "Array Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    expectEq(0., m[0, 0]);
    expectEq(1., m[0, 1]);
    expectEq(2., m[1, 0]);
    expectEq(3., m[1, 1]);
    for (const auto& val : m.data()) {
      std::cout << "Value: " << val << std::endl;
    }
  };

  "Copy Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n{m};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor"_test = [] {
    auto m = matrix::Dense{{0., 1.}, {2., 3.}};
    auto n{std::move(m)};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n = m;
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment"_test = [] {
    auto m = matrix::Dense{{0., 1.}, {2., 3.}};
    auto n = std::move(m);
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Static Size"_test = [] {
    matrix::Dense<int, 2, 3> m;
    static_assert(m.kRow == 2);
    static_assert(m.kCol == 3);
    matrix::Dense<int, 0, 0> z;
  };

  "Copy Assignment"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kRowMajor> n = m;
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  return 0;
}
