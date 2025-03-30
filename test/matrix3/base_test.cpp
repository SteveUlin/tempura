#include "matrix3/base.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "Basic Extent"_test = [] {
    constexpr Extents<int64_t, 2, 3> e{};
    static_assert(e.rank() == 2);
    static_assert(e.rank_dynamic() == 0);
    static_assert(e.static_extent(0) == 2);
    static_assert(e.static_extent(1) == 3);
    static_assert(e.extent(0) == 2);
    static_assert(e.extent(1) == 3);
  };

  "Dynamic Extent"_test = [] {
    constexpr Extents<int64_t, 0, kDynamic, 2, kDynamic, kDynamic> e(1, 3, 4);
    static_assert(e.rank() == 5);
    static_assert(e.rank_dynamic() == 3);
    static_assert(e.static_extent(0) == 0);
    static_assert(e.static_extent(1) == kDynamic);
    static_assert(e.static_extent(2) == 2);
    static_assert(e.static_extent(3) == kDynamic);
    static_assert(e.static_extent(4) == kDynamic);
    static_assert(e.extent(0) == 0);
    static_assert(e.extent(1) == 1);
    static_assert(e.extent(2) == 2);
    static_assert(e.extent(3) == 3);
    static_assert(e.extent(4) == 4);
  };

  "Dynamic Extent"_test = [] {
    constexpr Extents<int64_t, 0, kDynamic, 2, kDynamic, kDynamic> e(0, 1, 2, 3, 4);
    static_assert(e.rank() == 5);
    static_assert(e.rank_dynamic() == 3);
    static_assert(e.static_extent(0) == 0);
    static_assert(e.static_extent(1) == kDynamic);
    static_assert(e.static_extent(2) == 2);
    static_assert(e.static_extent(3) == kDynamic);
    static_assert(e.static_extent(4) == kDynamic);
    static_assert(e.extent(0) == 0);
    static_assert(e.extent(1) == 1);
    static_assert(e.extent(2) == 2);
    static_assert(e.extent(3) == 3);
    static_assert(e.extent(4) == 4);
  };

  return TestRegistry::result();
}
