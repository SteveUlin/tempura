#include "matrix2/multiplication.h"

#include <algorithm>

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Basic Multiplication"_test = [] {
    constexpr matrix::Dense<double, 2, 2> m{{0., 1.}, {2., 3.}};
    constexpr matrix::Dense<double, 2, 2> n{{4., 5.}, {6., 7.}};
    constexpr matrix::Dense<double, 2, 2> o{{6., 7.}, {26., 31.}};
    static_assert(m * n == o);
  };

  "Distinct Size Multiplication"_test = [] {
    constexpr matrix::Dense<double, 2, 3> m{{0., 1., 2.}, {3., 4., 5.}};
    constexpr matrix::Dense<double, 3, 2> n{{6., 7.}, {8., 9.}, {10., 11.}};
    constexpr matrix::Dense<double, 2, 2> o{{28., 31.}, {100., 112.}};
    static_assert(m * n == o);
  };

  return TestRegistry::result();
}
