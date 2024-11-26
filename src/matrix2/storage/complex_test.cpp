#include "matrix2/storage/complex.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Complex - Default Constructor"_test = [] {
    constexpr matrix::Complex m{};
    constexpr matrix::InlineDense expected{
        {1., 0.},
        {0., 1.},
    };
    static_assert(m == expected);
  };

  "Complex - Constructor"_test = [] {
    constexpr matrix::Complex m{1., 2.};
    constexpr matrix::InlineDense expected{
        {1., -2.},
        {2., 1.},
    };
    static_assert(m == expected);
  };

  "Complex - Complex Constructor"_test = [] {
    constexpr matrix::Complex m{1., 2.};
    constexpr matrix::Complex n{m};
    static_assert(m == n);
  };

  "Complex - Complex Assignment"_test = [] {
    constexpr matrix::Complex m{1., 2.};
    constexpr auto n = m;
    static_assert(m == n);
  };

  "Complex - Shape"_test = [] {
    constexpr matrix::Complex m{};
    static_assert(m.shape() == matrix::RowCol{.row = 2, .col = 2});
  };

  "Complex - [] operator"_test = [] {
    constexpr matrix::Complex m{1., 2.};
    static_assert(m[0, 0] == 1.);
    static_assert(m[0, 1] == -2.);
    static_assert(m[1, 0] == 2.);
    static_assert(m[1, 1] == 1.);
  };

  return TestRegistry::result();
}
