#include "matrix2/to_string.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Printing Works"_test = [] {
    matrix::InlineDense m{
        {9999, 1, 0},
        {1, 0, 1},
        {0, 1, 1},
    };
    constexpr auto expected =
        "⎡ 9999 1 0 ⎤\n"
        "⎢    1 0 1 ⎥\n"
        "⎣    0 1 1 ⎦";
    expectEq(toString(m), expected);
  };

  return 0;
}
