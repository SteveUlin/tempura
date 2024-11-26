#include "matrix2/storage/inline_coordinate_list.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Default Constructor"_test = [] {
    constexpr matrix::InlineCoordinateList<int, 2, 3> m{};
    static_assert(m.size() == 0);
  };

  "Array Constructor"_test = [] {
    constexpr auto m = matrix::makeInlineCoordinateList<2, 3>(
        std::array<int64_t, 1>{0}, std::array<int64_t, 1>{1},
        std::array<int, 1>{2});
    static_assert(m.size() == 1);
    constexpr matrix::InlineDense expected{
        {0, 2, 0},
        {0, 0, 0},
    };
    static_assert(m == expected);
  };
}
