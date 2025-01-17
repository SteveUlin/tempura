#include "matrix2/storage/block.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Block Row"_test = [] {
    constexpr matrix::InlineDense a{
        {1, 2, 3},
        {4, 5, 6},
    };
    constexpr matrix::InlineDense b{
      {7, 8, 9},
      {10, 11, 12},
    };

    constexpr matrix::BlockRow block{a, b};

    constexpr matrix::InlineDense expected{
      {1, 2, 3, 7, 8, 9},
      {4, 5, 6, 10, 11, 12},
    };

    static_assert(block == expected);
  };


  return TestRegistry::result();
}
