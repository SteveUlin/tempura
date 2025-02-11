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
  };

  for (auto v : InclusiveScanView{std::vector<int>{1, 2, 3, 4, 5}}) {
    std::print("{}\n", v);
  }

  auto view = InclusiveScanView{std::vector<int>{1, 2, 3, 4, 5}};
  for (auto v : view) {
    std::print("{}\n", v);
  }

  return TestRegistry::result();
}
