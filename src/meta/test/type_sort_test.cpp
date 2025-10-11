#include "meta/type_sort.h"

#include <print>
#include <type_traits>

#include "unit.h"

using namespace tempura;

// Simple comparison predicate for testing
struct IntComparator {
  template <auto A, auto B>
  constexpr bool operator()(std::integral_constant<int, A>,
                            std::integral_constant<int, B>) const {
    return A < B;
  }
};

int main() {
  "Empty list sorting"_test = [] {
    using empty = TypeList<>;
    using sorted = SortTypes_t<empty, IntComparator>;
    static_assert(std::is_same_v<sorted, TypeList<>>);
  };

  "Single element sorting"_test = [] {
    using single = TypeList<std::integral_constant<int, 5>>;
    using sorted = SortTypes_t<single, IntComparator>;
    static_assert(std::is_same_v<sorted, single>);
  };

  "Two element sorting - already sorted"_test = [] {
    using list = TypeList<std::integral_constant<int, 1>,
                          std::integral_constant<int, 2>>;
    using sorted = SortTypes_t<list, IntComparator>;
    static_assert(std::is_same_v<sorted, list>);
  };

  "Two element sorting - needs swap"_test = [] {
    using list = TypeList<std::integral_constant<int, 2>,
                          std::integral_constant<int, 1>>;
    using expected = TypeList<std::integral_constant<int, 1>,
                              std::integral_constant<int, 2>>;
    using sorted = SortTypes_t<list, IntComparator>;
    static_assert(std::is_same_v<sorted, expected>);
  };

  "Three element sorting"_test = [] {
    using list =
        TypeList<std::integral_constant<int, 3>, std::integral_constant<int, 1>,
                 std::integral_constant<int, 2>>;
    using expected =
        TypeList<std::integral_constant<int, 1>, std::integral_constant<int, 2>,
                 std::integral_constant<int, 3>>;
    using sorted = SortTypes_t<list, IntComparator>;
    static_assert(std::is_same_v<sorted, expected>);
  };

  "Five element sorting - reverse order"_test = [] {
    using list =
        TypeList<std::integral_constant<int, 5>, std::integral_constant<int, 4>,
                 std::integral_constant<int, 3>, std::integral_constant<int, 2>,
                 std::integral_constant<int, 1>>;
    using expected =
        TypeList<std::integral_constant<int, 1>, std::integral_constant<int, 2>,
                 std::integral_constant<int, 3>, std::integral_constant<int, 4>,
                 std::integral_constant<int, 5>>;
    using sorted = SortTypes_t<list, IntComparator>;
    static_assert(std::is_same_v<sorted, expected>);
  };

  "Compile-time verification"_test = [] {
    std::print("✓ All type sorting tests passed at compile time\n");
  };
}
