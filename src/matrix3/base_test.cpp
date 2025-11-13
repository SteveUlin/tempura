#include "matrix3/accessors.h"
#include "matrix3/extents.h"
#include "matrix3/layouts.h"
#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "Basic Extent"_test = [] {
    constexpr Extents<int64_t, 2, 3> e{};
    static_assert(e.rank() == 2);
    static_assert(e.rankDynamic() == 0);
    static_assert(e.staticExtent(0) == 2);
    static_assert(e.staticExtent(1) == 3);
    static_assert(e.extent(0) == 2);
    static_assert(e.extent(1) == 3);
  };

  "Dynamic Extent"_test = [] {
    constexpr Extents<int64_t, 0, kDynamic, 2, kDynamic, kDynamic> e(1, 3, 4);
    static_assert(e.rank() == 5);
    static_assert(e.rankDynamic() == 3);
    static_assert(e.staticExtent(0) == 0);
    static_assert(e.staticExtent(1) == kDynamic);
    static_assert(e.staticExtent(2) == 2);
    static_assert(e.staticExtent(3) == kDynamic);
    static_assert(e.staticExtent(4) == kDynamic);
    static_assert(e.extent(0) == 0);
    static_assert(e.extent(1) == 1);
    static_assert(e.extent(2) == 2);
    static_assert(e.extent(3) == 3);
    static_assert(e.extent(4) == 4);
  };

  "Dynamic Extent Full"_test = [] {
    constexpr Extents<int64_t, 0, kDynamic, 2, kDynamic, kDynamic> e(0, 1, 2, 3,
                                                                     4);
    static_assert(e.rank() == 5);
    static_assert(e.rankDynamic() == 3);
    static_assert(e.staticExtent(0) == 0);
    static_assert(e.staticExtent(1) == kDynamic);
    static_assert(e.staticExtent(2) == 2);
    static_assert(e.staticExtent(3) == kDynamic);
    static_assert(e.staticExtent(4) == kDynamic);
    static_assert(e.extent(0) == 0);
    static_assert(e.extent(1) == 1);
    static_assert(e.extent(2) == 2);
    static_assert(e.extent(3) == 3);
    static_assert(e.extent(4) == 4);
  };

  "LayoutLeft"_test = [] {
    static constexpr Extents<int64_t, 2, 3, 4> e{};
    LayoutLeft::Mapping layout(e);
    static_assert(layout(0, 0, 0) == 0);
    static_assert(layout(1, 0, 0) == 1);
    static_assert(layout(0, 1, 0) == 2);
    static_assert(layout(0, 0, 1) == 6);
    static_assert(layout(1, 1, 1) == 9);
  };

  "LayoutRight"_test = [] {
    static constexpr Extents<int64_t, 2, 3, 4> e{};
    constexpr LayoutRight::Mapping layout(e);
    static_assert(layout(0, 0, 0) == 0);
    static_assert(layout(0, 0, 1) == 1);
    static_assert(layout(0, 1, 0) == 4);
    static_assert(layout(1, 0, 0) == 12);
    static_assert(layout(1, 1, 1) == 17);
  };

  "Dense"_test = [] {
    Dense<int64_t, 2, 3> mat{};
    mat[0, 1] = 1;
    mat[1, 2] = 2;
    expectEq(mat[0, 0], 0);
    expectEq(mat[0, 1], 1);
    expectEq(mat[1, 2], 2);
    expectEq(mat.data()[2], 1);
  };

  "InlineDense"_test = [] {
    constexpr auto mat = [] consteval {
      InlineDense<int64_t, 2, 3> mat{};
      mat[0, 1] = 1;
      mat[1, 2] = 2;
      return mat;
    }();
    static_assert(mat[0, 0] == 0);
    static_assert(mat[0, 1] == 1);
    static_assert(mat[1, 2] == 2);
  };

  "Identity"_test = [] {
    constexpr Identity<int64_t, 2, 2> mat{};
    static_assert(mat[0, 0] == 1);
    static_assert(mat[0, 1] == 0);
    static_assert(mat[1, 0] == 0);
    static_assert(mat[1, 1] == 1);
  };

  "hard code 2d init"_test = [] {
    constexpr InlineDense mat = {
        {0, 1},
        {2, 3},
    };
    static_assert(mat[0, 0] == 0);
    static_assert(mat[0, 1] == 1);
    static_assert(mat[1, 0] == 2);
    static_assert(mat[1, 1] == 3);
  };

  "hard code 2d init"_test = [] {
    Dense mat{
        {0, 1},
        {2, 3},
    };
    expectEq(mat[0, 0], 0);
    expectEq(mat[0, 1], 1);
    expectEq(mat[1, 1], 3);
  };

  return TestRegistry::result();
}
