#include <iostream>

#include "symbolic2/symbolic.h"

using namespace tempura;

auto main() -> int {
  std::cout << "\n=== Symbolic2 Term Collecting Test ===\n\n";

  // Test: x + y + x
  {
    Symbol x, y, z;
    auto expr = x + y + x;
    auto result = simplify(expr);

    std::cout << "x + y + x:\n";
    std::cout << "  Type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 5});
    std::cout << "  Evaluates to: " << val << " (expected: 25)\n\n";
  }

  // Test: x*2 + y*3 + x*4
  {
    Symbol x, y;
    auto expr = x * 2_c + y * 3_c + x * 4_c;
    auto result = simplify(expr);

    std::cout << "x*2 + y*3 + x*4:\n";
    std::cout << "  Type: " << typeid(result).name() << "\n";

    auto val = evaluate(result, BinderPack{x = 10, y = 100});
    std::cout << "  Evaluates to: " << val << " (expected: 360)\n\n";
  }

  return 0;
}
