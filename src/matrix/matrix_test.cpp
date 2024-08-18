#include "matrix/matrix.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    matrix::Dense<double, 2, 3> m{};
    expectEq(m.shape(), matrix::Shape{2, 3});
    expectEq(0., m[0, 0]);
    expectEq(0., m[0, 1]);
    expectEq(0., m[1, 0]);
    expectEq(0., m[1, 1]);
  };

  "Array Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    expectEq(m.shape(), matrix::Shape{2, 2});
    expectEq(0., m[0, 0]);
    expectEq(1., m[0, 1]);
    expectEq(2., m[1, 0]);
    expectEq(3., m[1, 1]);
  };

  "Copy Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n{m};
    expectEq(n.shape(), matrix::Shape{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n{std::move(m)};
    expectEq(n.shape(), matrix::Shape{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n = m;
    expectEq(n.shape(), matrix::Shape{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n = std::move(m);
    expectEq(n.shape(), matrix::Shape{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Constructor From Dynamic"_test = [] {
    matrix::Dense<double, matrix::kDynamic, matrix::kDynamic> m{
        {0., 1.}, {2., 3.}};

    matrix::Dense<double, 2, 2> n{m};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor From Dynamic"_test = [] {
    matrix::Dense<double, matrix::kDynamic, matrix::kDynamic> m{
        {0., 1.}, {2., 3.}};

    matrix::Dense<double, 2, 2> n{std::move(m)};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment From Dynamic"_test = [] {
    matrix::Dense<double, matrix::kDynamic, matrix::kDynamic> m{
        {0., 1.}, {2., 3.}};

    matrix::Dense<double, 2, 2> n;
    n = m;
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment From Dynamic"_test = [] {
    matrix::Dense<double, matrix::kDynamic, matrix::kDynamic> m{
        {0., 1.}, {2., 3.}};

    matrix::Dense<double, 2, 2> n;
    n = std::move(m);
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Constructor From Other"_test = [] {
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                 {2., 3.}};
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kRowMajor> n{m};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor From Other"_test = [] {
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                 {2., 3.}};
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kRowMajor> n{std::move(m)};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment From Other"_test = [] {
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                 {2., 3.}};
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kRowMajor> n;
    n = m;
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment From Other"_test = [] {
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                 {2., 3.}};
    matrix::Dense<double, 2, 2, matrix::IndexOrder::kRowMajor> n;
    n = std::move(m);
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Dyanmic Shape"_test = [] {

  };

  "Set and get"_test = [] {
    matrix::Dense<int, 2, 2> m;
    expectEq(0, m[0, 1]);
    m[0, 1] = 2;
    expectEq(2, m[0, 1]);
  };

  "Static Size"_test = [] {
    matrix::Dense<int, 2, 3> m;
    static_assert(decltype(m)::kRow == 2);
    static_assert(decltype(m)::kCol == 3);
  };

  return 0;
}
