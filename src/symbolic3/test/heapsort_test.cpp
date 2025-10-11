#include <iostream>

#include "symbolic3/symbolic3.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

int main() {
  "HeapSort for TypeList - basic ordering"_test = [] {
    // Create three symbols with different type IDs
    constexpr Symbol a;
    constexpr Symbol b;
    constexpr Symbol c;

    // Get their type IDs for comparison
    constexpr auto id_a = kMeta<decltype(a)>;
    constexpr auto id_b = kMeta<decltype(b)>;
    constexpr auto id_c = kMeta<decltype(c)>;

    std::cout << "Symbol type IDs: a=" << id_a << ", b=" << id_b
              << ", c=" << id_c << "\n";

    // Test that lessThan works
    constexpr bool a_less_b = lessThan(a, b);
    constexpr bool b_less_c = lessThan(b, c);
    constexpr bool a_less_c = lessThan(a, c);

    std::cout << "a < b: " << a_less_b << ", b < c: " << b_less_c
              << ", a < c: " << a_less_c << "\n";
  };

  "HeapSort for TypeList - sort constants"_test = [] {
    // Test sorting constants
    using Unsorted = TypeList<Constant<5>, Constant<1>, Constant<3>,
                              Constant<2>, Constant<4>>;
    using Sorted = detail::HeapSortTypes_t<Unsorted>;

    // Verify the sorted list has the same size
    static_assert(detail::TypeListSize_v<Sorted> == 5,
                  "Should have 5 elements");

    // Verify first element is smallest
    using First = detail::GetAt_t<0, Sorted>;
    static_assert(std::is_same_v<First, Constant<1>>,
                  "First should be Constant<1>");

    // Verify last element is largest
    using Last = detail::GetAt_t<4, Sorted>;
    static_assert(std::is_same_v<Last, Constant<5>>,
                  "Last should be Constant<5>");

    std::cout << "✓ HeapSort correctly sorts constants\n";
    std::cout << "  Unsorted: Constant<5>, Constant<1>, Constant<3>, "
                 "Constant<2>, Constant<4>\n";
    std::cout << "  Sorted: Constant<1>, ..., Constant<5>\n";
  };

  "HeapSort for TypeList - empty and single element"_test = [] {
    // Test empty list
    using Empty = TypeList<>;
    using SortedEmpty = detail::HeapSortTypes_t<Empty>;
    static_assert(std::is_same_v<Empty, SortedEmpty>,
                  "Empty list should remain empty");

    // Test single element
    using Single = TypeList<Constant<42>>;
    using SortedSingle = detail::HeapSortTypes_t<Single>;
    static_assert(std::is_same_v<Single, SortedSingle>,
                  "Single element should be unchanged");

    std::cout << "✓ HeapSort handles edge cases (empty, single element)\n";
  };

  "HeapSort for TypeList - already sorted"_test = [] {
    // Test already sorted list
    using AlreadySorted = TypeList<Constant<1>, Constant<2>, Constant<3>>;
    using Sorted = detail::HeapSortTypes_t<AlreadySorted>;

    // Verify all elements are in order
    using First = detail::GetAt_t<0, Sorted>;
    using Second = detail::GetAt_t<1, Sorted>;
    using Third = detail::GetAt_t<2, Sorted>;

    static_assert(std::is_same_v<First, Constant<1>>,
                  "First should be Constant<1>");
    static_assert(std::is_same_v<Second, Constant<2>>,
                  "Second should be Constant<2>");
    static_assert(std::is_same_v<Third, Constant<3>>,
                  "Third should be Constant<3>");

    std::cout << "✓ HeapSort handles already-sorted list\n";
  };

  "HeapSort for TypeList - reverse sorted"_test = [] {
    // Test reverse-sorted list (worst case)
    using ReverseSorted = TypeList<Constant<5>, Constant<4>, Constant<3>,
                                   Constant<2>, Constant<1>>;
    using Sorted = detail::HeapSortTypes_t<ReverseSorted>;

    // Verify sorting worked
    using First = detail::GetAt_t<0, Sorted>;
    using Last = detail::GetAt_t<4, Sorted>;

    static_assert(std::is_same_v<First, Constant<1>>,
                  "First should be Constant<1>");
    static_assert(std::is_same_v<Last, Constant<5>>,
                  "Last should be Constant<5>");

    std::cout << "✓ HeapSort handles reverse-sorted list\n";
  };

  return 0;
}
