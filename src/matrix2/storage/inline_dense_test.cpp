#include "matrix2/storage/inline_dense.h"

#include <algorithm>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    constexpr matrix::InlineDense<double, 2, 3> m{};
    static_assert(m.data() == std::array{0., 0., 0., 0., 0., 0.});
  };

  "Array Constructor"_test = [] {
    constexpr matrix::InlineDense m{{0., 1.}, {2., 3.}};
    // Default to Col Major
    static_assert(m.data() == std::array{0., 2., 1., 3.});
  };

  "Copy Constructor"_test = [] {
    constexpr matrix::InlineDense m{{0., 1.}, {2., 3.}};
    constexpr auto n{m};
    static_assert(n.data() == std::array{0., 2., 1., 3.});
  };

  "Copy Assignment"_test = [] {
    constexpr matrix::InlineDense m{{0., 1.}, {2., 3.}};
    constexpr auto n = m;
    static_assert(n.data() == std::array{0., 2., 1., 3.});
  };

  "Data Constructor"_test = [] {
    constexpr matrix::InlineDense<double, 2, 2> m{std::array{0., 2., 1., 3.}};
    static_assert(m.data() == std::array{0., 2., 1., 3.});
  };

  "Shape"_test = [] {
    constexpr matrix::InlineDense<double, 2, 3> m{};
    static_assert(m.shape() == matrix::RowCol{.row = 2, .col = 3});
  };

  "[] operator"_test = [] {
    constexpr matrix::InlineDense m{{0., 1.}, {2., 3.}};
    static_assert(m[0, 0] == 0.);
    static_assert(m[0, 1] == 1.);
    static_assert(m[1, 0] == 2.);
    static_assert(m[1, 1] == 3.);
  };

  "[] row operator"_test = [] {
    constexpr matrix::InlineDense m{{0., 1., 2., 3.}};
    static_assert(m[0] == 0.);
    static_assert(m[1] == 1.);
    static_assert(m[2] == 2.);
    static_assert(m[3] == 3.);
  };

  "[] col operator"_test = [] {
    constexpr matrix::InlineDense m{{0., 1., 2., 3.}};
    static_assert(m[0] == 0.);
    static_assert(m[1] == 1.);
    static_assert(m[2] == 2.);
    static_assert(m[3] == 3.);
  };

  "const for loop"_test = [] {
    constexpr matrix::InlineDense m{{0., 1., 2., 3.}};
    constexpr double sum = std::ranges::fold_left(m, 0.0, std::plus<double>{});
    static_assert(sum == 6.);
  };

  "mutuable for loop"_test = [] {
    constexpr matrix::InlineDense m = [] {
      matrix::InlineDense out{{0., 1., 2., 3.}};
      for (auto& element : out) {
        element += 1;
      }
      return out;
    }();
    static_assert(m.data() == std::array{1., 2., 3., 4.});
  };

  return TestRegistry::result();
}
