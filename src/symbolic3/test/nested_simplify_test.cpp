#include <iostream>
#include <typeinfo>

#include "symbolic3/evaluate.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Testing Nested Expression Simplification ===\n\n";

  Symbol x;
  Symbol y;

  // Create manually nested expression: (x + x) + y
  auto inner = x + x;
  auto outer = inner + y;

  std::cout << "Expression: (x + x) + y\n";
  std::cout << "  Inner (x + x) type: " << typeid(inner).name() << "\n";
  std::cout << "  Outer ((x+x) + y) type: " << typeid(outer).name() << "\n\n";

  // Apply AdditionRules to inner
  {
    std::cout << "Applying AdditionRules to inner (x + x):\n";
    auto result = AdditionRules.apply(inner, default_context());
    std::cout << "  Result type: " << typeid(result).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(result, BinderPack{x = 10})
              << "\n";
    constexpr bool is_mul = match(result, x * 𝐜) || match(result, 𝐜 * x);
    std::cout << "  Is multiplication: " << (is_mul ? "YES" : "NO") << "\n\n";
  }

  // Apply innermost to outer
  {
    std::cout << "Applying innermost(AdditionRules) to ((x+x) + y):\n";
    auto result = innermost(AdditionRules).apply(outer, default_context());
    std::cout << "  Result type: " << typeid(result).name() << "\n";
    std::cout << "  Evaluates to: "
              << evaluate(result, BinderPack{x = 10, y = 5}) << "\n\n";
  }

  // Apply full_simplify to outer
  {
    std::cout << "Applying full_simplify to ((x+x) + y):\n";
    auto result = full_simplify(outer, default_context());
    std::cout << "  Result type: " << typeid(result).name() << "\n";
    std::cout << "  Evaluates to: "
              << evaluate(result, BinderPack{x = 10, y = 5}) << "\n\n";
  }

  return 0;
}
