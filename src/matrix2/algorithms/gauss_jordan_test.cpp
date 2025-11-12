#include "matrix2/algorithms/gauss_jordan.h"

#include "matrix2/multiplication.h"
#include "matrix2/storage/identity.h"
#include "matrix2/storage/inline_dense.h"
#include "matrix2/to_string.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Inversion kNone"_test = [] {
    constexpr auto A = matrix::InlineDense{
        {1., 1., 2.},
        {1., 2., 0.},
        {4., 14., 4.},
    };
    constexpr auto output = [&] {
      auto invA = A;
      gaussJordan<matrix::Pivot::kNone>(invA);
      return invA;
    }();
    std::println("{}", matrix::toString(output));
    static_assert(matrix::approxEqual(A * output, matrix::Identity<3>{}));
  };

  "Solve kNone"_test = [] {
    constexpr auto A = matrix::InlineDense<double, 3, 3>{
        {1., 1., 2.},
        {1., 2., 0.},
        {4., 14., 4.},
    };
    constexpr auto b0 = matrix::InlineDense{
        {1.},
        {2.},
        {3.},
    };
    constexpr auto b1 = matrix::InlineDense{
        {4.},
        {5.},
        {6.},
    };

    constexpr auto out = [A = A, x0 = b0, x1 = b1] mutable {
      CHECK(gaussJordan<matrix::Pivot::kNone>(A, x0, x1));
      return std::pair{x0, x1};
    }();

    std::println("{}", matrix::toString(out.first));
    static_assert(matrix::approxEqual(A * out.first, b0));
    static_assert(matrix::approxEqual(A * out.second, b1));
  };

  "Inversion kRow"_test = [] {
    constexpr auto A = matrix::InlineDense{
        {1., 1., 2.},
        {1., 2., 0.},
        {4., 14., 4.},
    };
    constexpr auto output = [&] {
      auto invA = A;
      gaussJordan<matrix::Pivot::kRow>(invA);
      return invA;
    }();
    std::println("{}", matrix::toString(output));
  };

  "Solve kNone"_test = [] {
    constexpr auto A = matrix::InlineDense<double, 3, 3>{
        {1., 1., 2.},
        {1., 2., 0.},
        {4., 14., 4.},
    };
    constexpr auto b0 = matrix::InlineDense{
        {1.},
        {2.},
        {3.},
    };
    constexpr auto b1 = matrix::InlineDense{
        {4.},
        {5.},
        {6.},
    };

    constexpr auto out = [A = A, x0 = b0, x1 = b1] mutable {
      CHECK(gaussJordan<matrix::Pivot::kNone>(A, x0, x1));
      return std::pair{x0, x1};
    }();

    std::println("{}", matrix::toString(out.first));
    static_assert(matrix::approxEqual(A * out.first, b0));
    static_assert(matrix::approxEqual(A * out.second, b1));
  };
}
