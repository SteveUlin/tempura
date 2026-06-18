#include "permutation.h"

#include <cstddef>

#include "mdarray.h"
#include "unit.h"

using namespace tempura;

// Object-level behaviour is constexpr: parity and dense queries at compile time.
static_assert([] {
  Permutation<3> p{1, 0, 2};  // the transposition (0 1)
  return p.determinantSign() == -1 && p[0, 1] && !p[0, 0];
}());
static_assert([] {
  Permutation<3> id{};
  return id.determinantSign() == 1 && id[0, 0] && id[1, 1] && id[2, 2];
}());

auto main() -> int {
  "identity leaves rows untouched"_test = [] {
    Permutation<3> id{};
    Dense<double, dyn, dyn> m(3, 1);
    m[0, 0] = 10; m[1, 0] = 20; m[2, 0] = 30;
    id.permuteRows(m);
    expectEq(m[0, 0], 10.0);
    expectEq(m[1, 0], 20.0);
    expectEq(m[2, 0], 30.0);
  };

  "permuteRows gathers whole rows: new row i <- old row order[i]"_test = [] {
    Permutation<3> p{1, 2, 0};  // row0<-1, row1<-2, row2<-0
    Dense<double, dyn, dyn> m(3, 2);
    for (std::size_t i = 0; i < 3; ++i) {
      m[i, 0] = static_cast<double>(i);
      m[i, 1] = static_cast<double>(10 + i);
    }
    p.permuteRows(m);
    expectEq(m[0, 0], 1.0); expectEq(m[0, 1], 11.0);  // old row 1
    expectEq(m[1, 0], 2.0); expectEq(m[1, 1], 12.0);  // old row 2
    expectEq(m[2, 0], 0.0); expectEq(m[2, 1], 10.0);  // old row 0
  };

  "parity tracks transpositions"_test = [] {
    Permutation<4> p{};
    expectEq(p.determinantSign(), 1);
    p.swap(0, 1);
    expectEq(p.determinantSign(), -1);
    p.swap(2, 3);
    expectEq(p.determinantSign(), 1);
  };

  "dynamic size behaves like static"_test = [] {
    Permutation<> p(3);
    expectEq(p.size(), std::size_t{3});
    p.swap(0, 2);
    expectEq(p.determinantSign(), -1);
  };

  return TestRegistry::result();
}
