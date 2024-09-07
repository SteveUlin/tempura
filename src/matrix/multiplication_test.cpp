#include "matrix/multiplication.h"

#include <cmath>
#include <ranges>

#include "matrix/printer.h"
#include "matrix/storage/dense.h"
#include "profiler.h"
#include "unit.h"

using namespace tempura;

constexpr int N = 1024 + 16;

template <int64_t N>
void target_function() {
  matrix::Dense<int, {10'000, 10'000}, matrix::IndexOrder::kRowMajor> m{
      std::views::iota(0, 10'000 * 10'000)};
  matrix::Dense<int, {10'000, 10'000}, matrix::IndexOrder::kColMajor> n{
      std::views::iota(0, 10'000 * 10'000)};
  {
    TEMPURA_TRACE();
    matrix::bufMultiplyThreadpool<16, N>(m, n);
  }
}

auto main() -> int {
  "Test Naive"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::naiveMultiplyAdd(m, n, o);
    }
  };
  "Test Better Naive"_test = [] {
    matrix::Dense<int, {N, N}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(1, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::naiveMultiplyAdd(m, n, o);
    }
  };

  "Test Block"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::blockMultiply(m, n, o);
    }
  };

  "Test Better Block"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::blockMultiply<4>(m, n, o);
    }
  };

  "Test Rev Block"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::revBlockMultiply(m, n, o);
    }
  };

  "Test Better Rev Block"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::revBlockMultiply(m, n, o);
    }
  };

  "Test Rev Block"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::revBlockMultiply<4>(m, n, o);
    }
  };

  "Test Better Rev Block"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::revBlockMultiply<4>(m, n, o);
    }
  };

  "Test Buf"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::bufMultiply<4>(m, n, o);
    }
  };

  "Test Better Buf"_test = [] {
    matrix::Dense<int, {N, N}> m{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::bufMultiply<512>(m, n, o);
    }
  };

  "Test Better Buf"_test = [] {
    matrix::Dense<int, {N, N}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::tileMultiply<16>(m, n, o);
    }
  };

  "Test Better Buf Slice"_test = [] {
    matrix::Dense<int, {N, N}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::bufMultiplySlice<16>(m, n, o);
    }
  };

  "Test Better Buf Slice"_test = [] {
    matrix::Dense<int, {N, N}, matrix::IndexOrder::kRowMajor> m{
        std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> n{std::views::iota(0, N * N)};
    matrix::Dense<int, {N, N}> o{};
    {
      TEMPURA_TRACE();
      matrix::bufMultiplyThreadpool<8, 16>(m, n, o);
    }
    {
      TEMPURA_TRACE();
      matrix::bufMultiplyThreadpool<8, 16>(m, n, o);
    }
    std::cout << "-----------" << std::endl;
  };

  "Test mul"_test = [] {
    constexpr int K = 4;
    matrix::Dense<double, {K, K}, matrix::kRowMajor> n{
        std::views::iota(0, K * K)};
    std::cout << matrix::toString(n) << std::endl;

    n *= 2.0;
    std::cout << matrix::toString(n) << std::endl;
    std::cout << matrix::toString(2.0 * n) << std::endl;
    matrix::Slicer<{1, 3}>::at({0, 1}, n) *= -1;
    std::cout << matrix::toString(n) << std::endl;
  };

  // "Test Better Buf"_test = [] {
  //   matrix::Dense<int, {10, 10}, matrix::IndexOrder::kRowMajor> m{
  //       std::views::iota(0, 10 * 10)};
  //   std::cout << toString(m) << std::endl;
  //   std::cout << "-----------" << std::endl;
  //   matrix::Dense<int, {10, 10}> n{std::views::iota(0, 10 * 10)};
  //   std::cout << toString(n) << std::endl;
  //   std::cout << "-----------" << std::endl;
  //   {
  //     TEMPURA_TRACE();
  //     auto out = matrix::bufMultiply<4>(m, n);
  //     std::cout << toString(out) << std::endl;
  //     std::cout << "-----------" << std::endl;
  //   }
  //   // {
  //   //   TEMPURA_TRACE();
  //   //   auto out = matrix::naiveMultiply(m, n);
  //   //   print(out);
  //   //   std::cout << "-----------" << std::endl;
  //   // }
  // };

  // [&]<int... j>(std::integer_sequence<int, j...>) {
  //   (target_function<j>(), ...);
  // }(std::integer_sequence<int, 2, 4, 8, 16, 32, 64, 128, 256>{});

  return 0;
}
