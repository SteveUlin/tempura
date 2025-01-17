#include "matrix2/storage/banded.h"

#include <array>
#include <vector>

#include "matrix2/storage/block.h"
#include "matrix2/storage/inline_dense.h"
#include "matrix2/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix;

auto main() -> int {
  "Banded - Default Constructor"_test = [] {
    static constexpr InlineDense d{
        {1, 2, 3},
        {4, 5, 6},
        {7, 8, 9},
        {10, 11, 12},
    };

    constexpr Banded b{d};

    static constexpr InlineDense expected{
        {2, 3, 0, 0},
        {4, 5, 6, 0},
        {0, 7, 8, 9},
        {0, 0, 10, 11},
    };
    static_assert(b == expected);
  };

  static constexpr InlineDense d{
      {1, 2, 3},
      {4, 5, 6},
      {7, 8, 9},
      {10, 11, 12},
      {13, 14, 15},
  };

  constexpr Banded b{d};
  std::println("{}", matrix::toString(b));

  return TestRegistry::result();
}
