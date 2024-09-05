#include "matrix/printer.h"

#include "matrix/storage/dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Printing Works"_test = []{
    matrix::Dense<int, {3, 3}> m{{9999, 1, 0}, {1, 0, 1}, {0, 1, 1}};
    constexpr auto expected =
      "⎡ 1 1 0 ⎤\n"
      "⎢ 1 0 1 ⎥\n"
      "⎣ 0 1 1 ⎦";
    expectEq(toString(m), expected);
    std::cout << toString(m) << std::endl;
  };
  "Printing Works"_test = []{
    matrix::Dense<double, {3, 3}> m{{9999.888, 1, 0}, {1, 0, 1}, {0, 1, 1}};
    constexpr auto expected =
      "⎡ 1 1 0 ⎤\n"
      "⎢ 1 0 1 ⎥\n"
      "⎣ 0 1 1 ⎦";
    expectEq(toString(m), expected);
    std::cout << toString(m) << std::endl;
  };
  return 0;
}
