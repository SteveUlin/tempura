#include "autodiff/taylor.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // Array initialization
  "Accessors"_test = [] {
    auto t = Taylor<double, 4>{0., 1., 2., 3.};
    expectEq(0., t[0]);
    expectEq(1., t[1]);
    expectEq(2., t[2]);
    expectEq(3., t[3]);
  };

  "Multiplication"_test = [] {
    auto t = Taylor<double, 4>{1., 2., 3., 4.};
    auto u = Taylor<double, 4>{1., 2., 3., 4.};
    expectEq(Taylor<double, 4>{0., 0., 0., 0.}, t * u);
  };

  "Power"_test = [] {
    auto t = Taylor<double, 4>{1., 2., 3., 4.};
    auto u = Taylor<double, 4>{2., 2., 3., 4.};
    expectEq(Taylor<double, 4>{0., 0., 0., 0.}, pow(t, u));
  };
  return 0;
}
