#include "matrix/multiplication.h"

#include <cmath>
#include <ranges>

#include "matrix/storage/dense.h"
#include "profiler.h"
#include "unit.h"

using namespace tempura;

void print(const matrix::MatrixT auto& m) {
  for (const auto& row : m.rows()) {
    for (const auto val : row) {
      std::cout << val << " ";
    }
    std::cout << std::endl;
  }
}

template <size_t N>
void target_function() {
  matrix::Dense<int, {10'000, 10'000}, matrix::IndexOrder::kRowMajor> m{
      std::views::iota(0, 10'000 * 10'000)};
  matrix::Dense<int, {10'000, 10'000}, matrix::IndexOrder::kColMajor> n{
      std::views::iota(0, 10'000 * 10'000)};
  {
    TEMPURA_TRACE();
    auto out = matrix::bufMultiplyThreadpool<16, N>(m, n);
  }
}

auto main() -> int {
  "Test Naive"_test = [] {
    matrix::Dense<int, {1'000, 1'000}> m{std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::naiveMultiply(m, n);
    }
  };
  "Test Better Naive"_test = [] {
    matrix::Dense<int, {10'000, 10'000}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, 10'000 * 10'000)};
    matrix::Dense<int, {10'000, 10'000}> n{std::views::iota(0, 10'000 * 10'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::naiveMultiply(m, n);
    }
  };

  "Test Block"_test = [] {
    matrix::Dense<int, {1'000, 1'000}> m{std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::blockMultiply(m, n);
    }
  };

  "Test Better Block"_test = [] {
    matrix::Dense<int, {1'000, 1'000}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::blockMultiply(m, n);
    }
  };

  "Test Rev Block"_test = [] {
    matrix::Dense<int, {1'000, 1'000}> m{std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::revBlockMultiply(m, n);
    }
  };

  "Test Better Rev Block"_test = [] {
    matrix::Dense<int, {1'000, 1'000}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::revBlockMultiply(m, n);
    }
  };

  "Test Rev Block"_test = [] {
    matrix::Dense<int, {1'000, 1'000}> m{std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::revBlockMultiply<4>(m, n);
    }
  };

  "Test Better Rev Block"_test = [] {
    matrix::Dense<int, {1'000, 1'000}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::revBlockMultiply<4>(m, n);
    }
  };

  "Test Buf"_test = [] {
    matrix::Dense<int, {1'000, 1'000}> m{std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::bufMultiply<4>(m, n);
    }
  };

  "Test Better Buf"_test = [] {
    matrix::Dense<int, {1'000, 1'000}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, 1'000 * 1'000)};
    matrix::Dense<int, {1'000, 1'000}> n{std::views::iota(0, 1'000 * 1'000)};
    {
      TEMPURA_TRACE();
      auto out = matrix::bufMultiply<64>(m, n);
    }
  };

  "Test Better Buf"_test = [] {
    matrix::Dense<int, {10, 10}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, 10 * 10)};
    print(m);
    std::cout << "-----------" << std::endl;
    matrix::Dense<int, {10, 10}> n{std::views::iota(0, 10 * 10)};
    print(n);
    std::cout << "-----------" << std::endl;
    {
      TEMPURA_TRACE();
      auto out = matrix::bufMultiply<4>(m, n);
      print(out);
      std::cout << "-----------" << std::endl;
    }
    {
      TEMPURA_TRACE();
      auto out = matrix::naiveMultiply(m, n);
      print(out);
      std::cout << "-----------" << std::endl;
    }
  };

  [&]<int... j>(std::integer_sequence<int, j...>) {
    (target_function<j>(), ...);
  }(std::integer_sequence<int, 2, 4, 8, 16, 32, 64, 128, 256>{});

  return 0;
}
