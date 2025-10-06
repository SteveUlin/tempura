// Test pattern matching directly
#include <iostream>
#include <typeinfo>

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"

using namespace tempura::symbolic3;

int main() {
  constexpr Symbol y;
  constexpr auto expr = y + 0_c;
  constexpr auto ctx = default_context();

  std::cout << "Testing pattern matching...\n\n";

  // Test if pattern x_ + 0_c matches y + 0_c
  {
    std::cout << "Test 1: Does x_ + 0_c match y + 0_c?\n";
    constexpr auto pattern = x_ + 0_c;
    constexpr auto result = match(pattern, expr);

    std::cout << "  Pattern type: " << typeid(pattern).name() << "\n";
    std::cout << "  Expr type: " << typeid(expr).name() << "\n";
    std::cout << "  Result type: " << typeid(result).name() << "\n";

    if constexpr (std::is_same_v<decltype(result), NoMatch>) {
      std::cout << "  ✗ Pattern didn't match!\n";
    } else {
      std::cout << "  ✓ Pattern matched!\n";

      // Now test instantiate
      std::cout << "\nTest 2: Instantiating x_ with match result\n";
      constexpr auto instantiated = instantiate(x_, result);
      std::cout << "  Instantiated type: " << typeid(instantiated).name()
                << "\n";

      // Check if it's the same as y
      if constexpr (std::is_same_v<decltype(instantiated), decltype(y)>) {
        std::cout << "  ✓ Correctly bound x_ → y\n";
      } else {
        std::cout << "  ✗ Binding produced wrong type\n";
      }
    }
  }

  // Test the full rewrite
  {
    std::cout << "\nTest 3: Testing full Rewrite{x_ + 0_c, x_}\n";
    constexpr auto rule = Rewrite{x_ + 0_c, x_};
    constexpr auto result = rule.apply(expr, ctx);

    std::cout << "  Result type: " << typeid(result).name() << "\n";

    if constexpr (std::is_same_v<decltype(result), Never>) {
      std::cout << "  ✗ Rewrite failed (returned Never)\n";
    } else if constexpr (std::is_same_v<decltype(result), decltype(expr)>) {
      std::cout << "  ✗ Rewrite didn't change type\n";
    } else {
      std::cout << "  ✓ Rewrite succeeded!\n";
    }
  }

  return 0;
}
