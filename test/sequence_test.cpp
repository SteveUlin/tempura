#include "sequence.h"

#include <algorithm>
#include <cassert>
#include <print>
#include <ranges>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "FnGenerator"_test = [] {
    constexpr int a =
        FnGenerator{[i = 0] mutable -> int { return ++i; }} | TakeFirst{};
    static_assert(a == 1);

    constexpr auto value = std::views::repeat(std::pair{1.0, 4.0}) |
                           continuants() | Converges{.epsilon = 1e-15};
    std::println("{}", value);
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
