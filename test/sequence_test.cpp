#include "sequence.h"

#include <algorithm>
#include <cassert>
#include <print>
#include <ranges>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "FnGenerator"_test = [] {
    FnGenerator gen{[i = 0] mutable -> int { return ++i; }};
    std::ranges::take_view take{std::move(gen), 5};
    // auto arr = gen | std::views::take(5) | std::ranges::to<std::vector<int>>();
    // expectRangeEq(arr, std::vector{2, 2, 2, 2, 2});
  };
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
