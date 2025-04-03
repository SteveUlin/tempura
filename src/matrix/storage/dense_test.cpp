#include "matrix/storage/dense.h"

#include <ranges>

#include "matrix/matrix.h"
#include "matrix/printer.h"
#include "profiler.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    matrix::Dense<double, {2, 3}> m;
    expectEq(m.shape(), matrix::RowCol{2, 3});
    expectEq(0., m[0, 0]);
    expectEq(0., m[0, 1]);
    expectEq(0., m[1, 0]);
    expectEq(0., m[1, 1]);
  };

  "Array Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    expectEq(m.shape(), matrix::RowCol{2, 2});
    expectEq(0., m[0, 0]);
    expectEq(1., m[0, 1]);
    expectEq(2., m[1, 0]);
    expectEq(3., m[1, 1]);
  };

  "Copy Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n{m};
    expectEq(n.shape(), matrix::RowCol{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n{std::move(m)};
    expectEq(n.shape(), matrix::RowCol{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n = m;
    expectEq(n.shape(), matrix::RowCol{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    auto n = std::move(m);
    expectEq(n.shape(), matrix::RowCol{2, 2});
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Constructor From Dynamic"_test = [] {
    matrix::Dense<double, {matrix::kDynamic, matrix::kDynamic}> m = {{0., 1.},
                                                                  {2., 3.}};

    matrix::Dense<double, {2, 2}> n{m};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor From Dynamic"_test = [] {
    matrix::Dense<double, {matrix::kDynamic, matrix::kDynamic}> m{{0., 1.},
                                                                  {2., 3.}};

    matrix::Dense<double, {2, 2}> n{std::move(m)};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment From Dynamic"_test = [] {
    matrix::Dense<double, {matrix::kDynamic, matrix::kDynamic}> m{{0., 1.},
                                                                  {2., 3.}};

    matrix::Dense<double, {2, 2}> n;
    n = m;
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment From Dynamic"_test = [] {
    matrix::Dense<double, {matrix::kDynamic, matrix::kDynamic}> m{{0., 1.},
                                                                  {2., 3.}};

    matrix::Dense<double, {2, 2}> n;
    n = std::move(m);
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Constructor From Other"_test = [] {
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                   {2., 3.}};
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kRowMajor> n{m};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Constructor From Other"_test = [] {
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                   {2., 3.}};
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kRowMajor> n{
        std::move(m)};
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Copy Assignment From Other"_test = [] {
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                   {2., 3.}};
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kRowMajor> n;
    n = m;
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Move Assignment From Other"_test = [] {
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kColMajor> m{{0., 1.},
                                                                   {2., 3.}};
    matrix::Dense<double, {2, 2}, matrix::IndexOrder::kRowMajor> n;
    n = std::move(m);
    expectEq(0., n[0, 0]);
    expectEq(1., n[0, 1]);
    expectEq(2., n[1, 0]);
    expectEq(3., n[1, 1]);
  };

  "Set and get"_test = [] {
    matrix::Dense<int, {2, 2}> m;
    expectEq(0, m[0, 1]);
    m[0, 1] = 2;
    expectEq(2, m[0, 1]);
  };

  "Static Size"_test = [] {
    matrix::Dense<int, {2, 3}> m;
    static_assert(decltype(m)::kExtent.row == 2);
    static_assert(decltype(m)::kExtent.col == 3);
  };

  "Iterate"_test = [] {
    matrix::Dense<int, {2, 2}, tempura::matrix::IndexOrder::kRowMajor> m{
        {0, 1}, {2, 3}};
    int i = 0;
    for (const int val : m) {
      expectEq(i++, val);
    }
    expectEq(4, i);
  };

  "Iteration Single"_test = [] {
    matrix::Dense<int, {10'000, 10'000}> m(
      {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (int i = 0; i < 10'000 * 10'000; ++i) {
          sum += m.data()[i];
      }
    }
    std::cout << "SUM: " << sum << '\n';
  };

  "Iter Elements"_test = [] {
    const matrix::Dense<int, {10'000, 10'000}> m(
      {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (const int val : m) {
        sum += val;
      }
    }
    std::cout << "SUM: " << sum << '\n';
  };

  "Iteration Double For"_test = [] {
    matrix::Dense<int, {10'000, 10'000}> m(
      {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (int i = 0; i < 10'000; ++i) {
        for (int j = 0; j < 10'000; ++j) {
          sum += m[i, j];
        }
      }
    }
    std::cout << "SUM: " << sum << '\n';
    expectEq(sum, 0);
  };

  "Iteration Double For Reversed"_test = [] {
    matrix::Dense<int, {10'000, 10'000}> m(
      {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (int j = 0; j < 10'000; ++j) {
        for (int i = 0; i < 10'000; ++i) {
          sum += m[i, j];
        }
      }
    }
    std::cout << "SUM: " << sum << '\n';
    expectEq(sum, 0);
  };

  "Iteration rows "_test = [] {
    const matrix::Dense<int, {10'000, 10'000}> m(
      {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (auto row : m.rows()) {
        for (const int val : row) {
          sum += val;
        }
      }
    }
    std::cout << "SUM: " << sum << '\n';
  };

  "Iteration cols"_test = [] {
    const matrix::Dense<int, {10'000, 10'000}> m(
      {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (auto col : m.cols()) {
        for (const int val : col) {
          sum += val;
        }
      }
    }
    std::cout << "SUM: " << sum << '\n';
  };

  // "Iteration cols"_test = [] {
  //   const matrix::Dense<int, {10'000, 10'000}> m(
  //     {10'000, 10'000}, std::ranges::views::iota(0, 10'000 * 10'000));
  //   int sum = 0;
  //   {
  //     TEMPURA_TRACE();
  //     for (const auto col : m.cols()) {
  //       for (int val : col) {
  //         sum += val;
  //       }
  //     }
  //   }
  //   std::cout << "SUM: " << sum << '\n';
  // };

  return 0;
}
