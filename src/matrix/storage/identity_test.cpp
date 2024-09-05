#include "matrix/storage/identity.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    matrix::Identity<3> I;
    expectEq(I.shape(), matrix::RowCol{3, 3});
    expectEq(I[0, 0], 1);
    expectEq(I[0, 1], 0);
    expectEq(I[0, 2], 0);

    expectEq(I[1, 0], 0);
    expectEq(I[1, 1], 1);
    expectEq(I[1, 2], 0);

    expectEq(I[2, 0], 0);
    expectEq(I[2, 1], 0);
    expectEq(I[2, 2], 1);
  };
  return 0;
}
