#include "meta/vector_to_array.h"

#include <cassert>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "vectorToArray"_test = [] {
    constexpr auto generator = []() {
      return std::vector<int>{1, 2, 3, 4, 5};
    };

    constexpr auto arr = tempura::vectorToArray(generator);
    static_assert(arr.size() == 5);

    static_assert(arr == std::array<int, 5>{1, 2, 3, 4, 5});
  };
}
