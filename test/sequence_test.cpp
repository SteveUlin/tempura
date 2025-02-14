#include "sequence.h"
#include "unit.h"

#include <cassert>
#include <print>
#include <algorithm>

using namespace tempura;

auto main() -> int {
  "Ref Data"_test = [] {
    constexpr static std::array data{1, 2, 3, 4, 5};
    constexpr InclusiveScanView view{data};
    constexpr static std::array expected{1, 3, 6, 10, 15};
    static_assert(std::ranges::equal(expected, view));
  };

  "Temporary View"_test = [] {
    InclusiveScanView view{std::vector<int>{1, 2, 3, 4, 5}};
    const std::array expected{1, 3, 6, 10, 15};
    expectRangeEq(expected, view);
  };

  "Pipe Syntax"_test = [] {
    const std::array data{1, 2, 3, 4, 5};
    const std::array expected{1, 3, 6, 10, 15};
    const auto view = data | inclusiveScan();
    expectRangeEq(expected, view);
  };

  "Ref Data Pipe"_test = [] {
    constexpr static std::array data{1, 2, 3, 4, 5};
    constexpr auto view = data | inclusiveScan();
    constexpr static std::array expected{1, 3, 6, 10, 15};
    static_assert(std::ranges::equal(expected, view));
  };

  return TestRegistry::result();
}
