#include <ranges>

#include "profiler.h"
#include "matrix/storage/dense.h"
#include "matrix/printer.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    std::cout << "HERE: " << m[0, 0] << '\n';
    auto a = matrix::Slicer<{1, 2}>::at({0, 0}, m);

    // expectEq(m.shape(), matrix::RowCol{2, 3});
    expectEq(1., a[0, 0]);
    expectEq(0., a[0, 1]);
    expectEq(0., a[0, 0]);
    std::cout << "HERE: " << a[0, 0] << '\n';
  };
  "Iteration Single"_test = [] {
    matrix::Dense<int, {10'000, 10'000}> m;
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (int i = 0; i < 10'000 * 10'000; ++i) {
          sum += m.data()[i];
      }
    }
    expectEq(sum, 0);
  };

  "Swap"_test = [] {
    matrix::Dense<int, {10'000, 10'000}> m{
        std::views::iota(0, 10'000 * 10'000)};
    int sum = 0;
    {
      TEMPURA_TRACE();
      for (auto col : matrix::Cols(m)) {
        for (int val : col) {
          sum += val;
        }
      }
    }
    std::cout << "SUM: " << sum << '\n';
    expectEq(sum, 0);
    auto r_buf = matrix::Slicer<{2, 2}>::at({2, 2}, m);
    auto l_buf = matrix::Slicer<{2, 2}>::at({10, 10}, m);
    matrix::swap(r_buf, l_buf);
  };

  "Swap"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    std::cout << matrix::toString(m) << std::endl;
    matrix::Slicer<{1, 2}>::at({1, 0}, m) *= 2;
    std::cout << matrix::toString(m) << std::endl;
  };

  return 0;
}
