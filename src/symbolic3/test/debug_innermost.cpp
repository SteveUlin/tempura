// Debug innermost traversal behavior
#include <iostream>
#include <typeinfo>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/simplify.h"
#include "symbolic3/traversal.h"

using namespace tempura::symbolic3;

int main() {
  // Create simple symbols and context
  constexpr Symbol x;
  constexpr Symbol y;
  constexpr auto ctx = default_context();

  // Test 1: Does Addition identity rule work at all?
  {
    std::cout << "Test 1: Direct application of algebraic_simplify to y + 0\n";
    constexpr auto simple_expr = y + 0_c;
    std::cout << "  Input type: " << typeid(simple_expr).name() << "\n";

    constexpr auto simplified = algebraic_simplify.apply(simple_expr, ctx);
    std::cout << "  Output type: " << typeid(simplified).name() << "\n";

    constexpr bool same =
        std::is_same_v<decltype(simplified), decltype(simple_expr)>;
    std::cout << "  Same type? " << (same ? "YES" : "NO") << "\n";

    if constexpr (!same) {
      std::cout << "  ✓ Addition identity rule works!\n";
    } else {
      std::cout << "  ✗ Addition identity rule didn't fire\n";
    }
  }

  // Test 2: Does innermost work on flat expression?
  {
    std::cout << "\nTest 2: Innermost on flat expression (y + 0)\n";
    constexpr auto simple_expr = y + 0_c;

    constexpr auto with_innermost =
        innermost(algebraic_simplify).apply(simple_expr, ctx);

    constexpr bool same =
        std::is_same_v<decltype(with_innermost), decltype(simple_expr)>;
    std::cout << "  Same type? " << (same ? "YES" : "NO") << "\n";

    if constexpr (!same) {
      std::cout << "  ✓ Innermost simplifies flat expression\n";
    } else {
      std::cout << "  ✗ Innermost didn't simplify\n";
    }
  }

  // Test 3: Nested expression
  {
    std::cout << "\nTest 3: Innermost on nested expression (x * (y + 0))\n";
    constexpr auto nested = x * (y + 0_c);
    std::cout << "  Input type: " << typeid(nested).name() << "\n";

    constexpr auto with_innermost =
        innermost(algebraic_simplify).apply(nested, ctx);
    std::cout << "  Output type: " << typeid(with_innermost).name() << "\n";

    constexpr bool same =
        std::is_same_v<decltype(with_innermost), decltype(nested)>;
    std::cout << "  Same type? " << (same ? "YES" : "NO") << "\n";

    if constexpr (!same) {
      std::cout << "  ✓ Innermost simplifies nested expression\n";
    } else {
      std::cout << "  ✗ Innermost didn't simplify\n";
    }
  }

  return 0;
}
