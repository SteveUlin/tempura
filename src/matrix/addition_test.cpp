#include "matrix/addition.h"

#include "matrix/dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Simple Addition"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    matrix::Dense n{{4., 5.}, {6., 7.}};

    matrix::Dense o = m + n;
    expectEq(o[0, 0], 4.);
    expectEq(o[0, 1], 6.);
    expectEq(o[1, 0], 8.);
    expectEq(o[1, 1], 10.);
  };

  "Simple Subtraction"_test = [] {
    matrix::Dense m{{0., 1.}, {2., 3.}};
    matrix::Dense n{{4., 5.}, {6., 7.}};
    matrix::Dense o = n - m;
    expectEq(o[0, 0], 4.);
    expectEq(o[0, 1], 4.);
    expectEq(o[1, 0], 4.);
    expectEq(o[1, 1], 4.);
  };
  return 0;
}


