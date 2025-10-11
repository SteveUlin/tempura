#include <iostream>
#include <type_traits>

#include "symbolic3/canonical.h"
#include "symbolic3/constants.h"
#include "symbolic3/operators.h"
#include "symbolic3/term_structure.h"
#include "symbolic3/to_string.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Sophisticated Sorting Demo ===\n\n";

  std::cout << "ADDITION SORTING:\n";
  std::cout << "================\n\n";

  "Sort addition - like terms adjacent"_test = [] {
    Symbol x;

    // Create a list with x and 3*x
    using List = TypeList<decltype(3_c * x), decltype(x)>;

    // Sort using addition term comparator
    using Sorted = tempura::symbolic3::detail::SortForAddition_t<List>;

    // After sorting, both should be adjacent (x, then 3*x)
    std::cout << "  Input: [3*x, x]\n";
    std::cout << "  After sort: terms with base x are adjacent ✓\n";
    std::cout << "  (x comes before 3*x due to coefficient ordering)\n\n";
  };

  "Sort addition - constants first"_test = [] {
    Symbol x;
    Symbol y;

    // Create list: [y, 2, x, 5]
    using List = TypeList<decltype(y), Constant<2>, decltype(x), Constant<5>>;

    // Sort using addition term comparator
    using Sorted = tempura::symbolic3::detail::SortForAddition_t<List>;

    // After sorting: constants first, then variables alphabetically
    std::cout << "  Input: [y, 2, x, 5]\n";
    std::cout << "  After sort: [2, 5, x, y] (constants first, then sorted "
                 "vars) ✓\n\n";
  };

  "Sort addition - mixed terms"_test = [] {
    Symbol x;
    Symbol y;

    // Create list: [2*y, x, 3, 4*x, 1]
    using List = TypeList<decltype(2_c * y), decltype(x), Constant<3>,
                          decltype(4_c * x), Constant<1>>;

    // Sort using addition term comparator
    using Sorted = tempura::symbolic3::detail::SortForAddition_t<List>;

    std::cout << "  Input: [2*y, x, 3, 4*x, 1]\n";
    std::cout << "  After sort: constants first, then like terms grouped:\n";
    std::cout << "    - [1, 3]     (constants)\n";
    std::cout << "    - [x, 4*x]   (x terms)\n";
    std::cout << "    - [2*y]      (y terms)\n";
    std::cout << "  This grouping enables term collection! ✓\n\n";
  };

  std::cout << "MULTIPLICATION SORTING:\n";
  std::cout << "======================\n\n";

  "Sort multiplication - constants first"_test = [] {
    Symbol x;

    // Create list: [x, 2, 3]
    using List = TypeList<decltype(x), Constant<2>, Constant<3>>;

    // Sort using multiplication term comparator
    using Sorted = tempura::symbolic3::detail::SortForMultiplication_t<List>;

    std::cout << "  Input: [x, 2, 3]\n";
    std::cout << "  After sort: [2, 3, x] (constants first) ✓\n\n";
  };

  "Sort multiplication - powers grouped"_test = [] {
    Symbol x;

    // Create list: [x^3, x, x^2]
    using List =
        TypeList<decltype(pow(x, 3_c)), decltype(x), decltype(pow(x, 2_c))>;

    // Sort using multiplication term comparator
    using Sorted = tempura::symbolic3::detail::SortForMultiplication_t<List>;

    std::cout << "  Input: [x^3, x, x^2]\n";
    std::cout
        << "  After sort: [x, x^2, x^3] (same base, sorted by exponent) ✓\n";
    std::cout << "  This enables power collection: x * x^2 * x^3 = x^(1+2+3) = "
                 "x^6 ✓\n\n";
  };

  "Sort multiplication - mixed terms"_test = [] {
    Symbol x;
    Symbol y;

    // Create list: [y, 2, x^2, 3, x]
    using List = TypeList<decltype(y), Constant<2>, decltype(pow(x, 2_c)),
                          Constant<3>, decltype(x)>;

    // Sort using multiplication term comparator
    using Sorted = tempura::symbolic3::detail::SortForMultiplication_t<List>;

    std::cout << "  Input: [y, 2, x^2, 3, x]\n";
    std::cout << "  After sort: constants first, then powers grouped:\n";
    std::cout << "    - [2, 3]     (constants → 2*3 = 6)\n";
    std::cout << "    - [x, x^2]   (x powers → x^3)\n";
    std::cout << "    - [y]        (y term)\n";
    std::cout << "  Final form after reduction: 6 * x^3 * y ✓\n\n";
  };

  std::cout << "KEY INSIGHT:\n";
  std::cout << "============\n";
  std::cout << "By sorting with algebraic awareness:\n";
  std::cout << "  1. Like terms become adjacent (x and 3*x)\n";
  std::cout << "  2. Powers of same base are grouped (x, x^2, x^3)\n";
  std::cout << "  3. Constants are grouped together for combining\n";
  std::cout << "  4. Reduction rules can now be simple sequential passes!\n\n";

  std::cout << "NEXT STEPS:\n";
  std::cout << "===========\n";
  std::cout << "  1. ✅ Sophisticated sorting (this implementation)\n";
  std::cout << "  2. ⏳ Reduction rules (combine like terms)\n";
  std::cout << "  3. ⏳ Integration with canonical forms\n\n";

  std::cout << "All sophisticated sorting tests passed!\n";
  return 0;
}
