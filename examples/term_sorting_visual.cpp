#include <iostream>

#include "symbolic3/canonical.h"
#include "symbolic3/constants.h"
#include "symbolic3/operators.h"
#include "symbolic3/term_structure.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n╔══════════════════════════════════════════════════════╗\n";
  std::cout << "║  Algebraic Sorting: The Key to Term Collection  ║\n";
  std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

  Symbol x;
  Symbol y;

  // Example 1: The core problem
  std::cout << "PROBLEM: How to simplify x + 3*x?\n";
  std::cout << "────────────────────────────────\n\n";

  std::cout << "Without algebraic sorting:\n";
  std::cout << "  Terms: [x, 3*x]\n";
  std::cout << "  Issue: Pattern matcher must search entire expression\n";
  std::cout << "         to find terms with same base\n";
  std::cout << "  Cost: O(n²) comparisons for n terms\n\n";

  std::cout << "With algebraic sorting:\n";
  std::cout << "  Terms after sort: [x, 3*x]  (adjacent!)\n";
  std::cout << "  Analysis:\n";
  std::cout << "    x   → coefficient=1, base=x\n";
  std::cout << "    3*x → coefficient=3, base=x\n";
  std::cout << "  Same base detected → combine coefficients: 1 + 3 = 4\n";
  std::cout << "  Result: 4*x\n";
  std::cout << "  Cost: O(n) linear scan\n\n";

  // Demonstrate the comparison
  auto term1 = x;
  auto term2 = 3_c * x;

  constexpr auto cmp = compareAdditionTerms(term1, term2);
  std::cout << "Comparison result: ";
  std::cout << (cmp == Ordering::Less      ? "x < 3*x"
                : cmp == Ordering::Greater ? "x > 3*x"
                                           : "x = 3*x");
  std::cout << " ✓\n\n";

  // Example 2: Multiple like terms
  std::cout << "EXAMPLE: x + 2*y + 3*x + y + 5\n";
  std::cout << "──────────────────────────────\n\n";

  using List = TypeList<decltype(x), decltype(2_c * y), decltype(3_c * x),
                        decltype(y), Constant<5> >;

  std::cout << "After sorting:\n";
  using Sorted = tempura::symbolic3::detail::SortForAddition_t<List>;
  std::cout << "  [5, x, 3*x, y, 2*y]\n";
  std::cout << "   ^   ^^^^^^  ^^^^^\n";
  std::cout << "   │   x terms y terms\n";
  std::cout << "   constant\n\n";

  std::cout << "Reduction (linear scan):\n";
  std::cout << "  Step 1: 5 (constant, keep as is)\n";
  std::cout << "  Step 2: x and 3*x adjacent → (1+3)*x = 4*x\n";
  std::cout << "  Step 3: y and 2*y adjacent → (1+2)*y = 3*y\n";
  std::cout << "  Final: 5 + 4*x + 3*y\n\n";

  // Example 3: Multiplication
  std::cout << "EXAMPLE: 2 * x^2 * 3 * x * y\n";
  std::cout << "────────────────────────────\n\n";

  using MulList = TypeList<Constant<2>, decltype(pow(x, 2_c)), Constant<3>,
                           decltype(x), decltype(y)>;

  std::cout << "After sorting:\n";
  using MulSorted =
      tempura::symbolic3::detail::SortForMultiplication_t<MulList>;
  std::cout << "  [2, 3, x, x^2, y]\n";
  std::cout << "   ^^^^  ^^^^^^^^ ^\n";
  std::cout << "   const x powers y\n\n";

  std::cout << "Reduction (linear scan):\n";
  std::cout << "  Step 1: 2 and 3 adjacent → 2*3 = 6\n";
  std::cout << "  Step 2: x and x^2 adjacent → x^(1+2) = x^3\n";
  std::cout << "  Step 3: y (keep as is)\n";
  std::cout << "  Final: 6 * x^3 * y\n\n";

  std::cout << "╔══════════════════════════════════════════════════════╗\n";
  std::cout << "║  Key Insight: Sorting = Grouping = Easy Reduction  ║\n";
  std::cout << "╚══════════════════════════════════════════════════════╝\n\n";

  std::cout << "By sorting with algebraic awareness:\n";
  std::cout << "  ✓ Like terms are adjacent\n";
  std::cout << "  ✓ Reduction rules are simple linear scans\n";
  std::cout << "  ✓ No complex pattern matching needed\n";
  std::cout << "  ✓ Predictable, canonical output\n\n";

  return 0;
}
