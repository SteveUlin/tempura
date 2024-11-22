#include "matrix2/storage/dense.h"

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

  // Col Major

  "ColMajor - Default Constructor"_test = [] {
    constexpr matrix::InlineDenseColMajor<double, 2, 3> m{};
    static_assert(m.data() == std::array{0., 0., 0., 0., 0., 0.});
  };

  "ColMajor - Array Constructor"_test = [] {
    constexpr matrix::InlineDenseColMajor m{{0., 1.}, {2., 3.}};
    // Default to Col Major
    static_assert(m.data() == std::array{0., 2., 1., 3.});
  };

  "ColMajor - Copy Constructor"_test = [] {
    constexpr matrix::InlineDenseColMajor m{{0., 1.}, {2., 3.}};
    constexpr auto n{m};
    static_assert(n.data() == std::array{0., 2., 1., 3.});
  };

  "ColMajor - Copy Assignment"_test = [] {
    constexpr matrix::InlineDenseColMajor m{{0., 1.}, {2., 3.}};
    constexpr auto n = m;
    static_assert(n.data() == std::array{0., 2., 1., 3.});
  };

  "ColMajor - Data Constructor"_test = [] {
    constexpr matrix::InlineDenseColMajor<double, 2, 2> m{
        std::array{0., 2., 1., 3.}};
    static_assert(m.data() == std::array{0., 2., 1., 3.});
  };

  // Row Major

  "RowMajor - Default Constructor"_test = [] {
    constexpr matrix::InlineDenseRowMajor<double, 2, 3> m{};
    static_assert(m.data() == std::array{0., 0., 0., 0., 0., 0.});
  };

  "RowMajor - Array Constructor"_test = [] {
    constexpr matrix::InlineDenseRowMajor m{{0., 1.}, {2., 3.}};
    // Default to Col Major
    static_assert(m.data() == std::array{0., 1., 2., 3.});
  };

  "RowMajor - Copy Constructor"_test = [] {
    constexpr matrix::InlineDenseRowMajor m{{0., 1.}, {2., 3.}};
    constexpr auto n{m};
    static_assert(n.data() == std::array{0., 1., 2., 3.});
  };

  "RowMajor - Copy Assignment"_test = [] {
    constexpr matrix::InlineDenseRowMajor m{{0., 1.}, {2., 3.}};
    constexpr auto n = m;
    static_assert(n.data() == std::array{0., 1., 2., 3.});
  };

  "RowMajor - Data Constructor"_test = [] {
    constexpr matrix::InlineDenseRowMajor<double, 2, 2> m{
        std::array{0., 1., 2., 3.}};
    static_assert(m.data() == std::array{0., 1., 2., 3.});
  };

  return TestRegistry::result();
}
