#include <iostream>
#include <typeinfo>

#include "symbolic3/evaluate.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"

using namespace tempura::symbolic3;

auto main() -> int {
  std::cout << "\n=== Testing Factoring Rules ===\n\n";

  Symbol x;

  // Test: x*2 + x should become x*(2+1) = x*3
  {
    auto expr = x * 2_c + x;
    std::cout << "Expression: x*2 + x\n";
    std::cout << "  Initial type: " << typeid(expr).name() << "\n";

    auto result = simplify(expr, default_context());
    std::cout << "  After simplify: " << typeid(result).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(result, BinderPack{x = 10})
              << " (expected: 30)\n\n";

    // Check structure
    constexpr bool is_single_mul = match(result, x * 𝐜);
    std::cout << "  Is 'x * const': " << (is_single_mul ? "YES" : "NO")
              << "\n\n";
  }

  // Test factoring rule directly
  {
    std::cout << "Testing factoring rule directly:\n";

    // Create x*2 + x*1
    auto lhs = x * 2_c;
    auto rhs = x * 1_c;
    auto expr = lhs + rhs;

    std::cout << "  Expression: (x*2) + (x*1)\n";
    std::cout << "  Type: " << typeid(expr).name() << "\n";

    // Check if it matches the factoring pattern
    constexpr bool matches_pattern = match(expr, x_ * a_ + x_ * b_);
    std::cout << "  Matches 'x_*a_ + x_*b_': "
              << (matches_pattern ? "YES" : "NO") << "\n";

    // Apply factoring rule directly
    constexpr auto factor_rule = Rewrite{x_ * a_ + x_ * b_, x_ * (a_ + b_)};
    auto factored = factor_rule.apply(expr, default_context());
    std::cout << "  After factoring: " << typeid(factored).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(factored, BinderPack{x = 10})
              << "\n\n";
  }

  // Test with canonical ordering in multiplication
  {
    std::cout << "Testing canonical ordering in multiplication:\n";
    auto expr1 = x * 2_c;
    auto expr2 = 2_c * x;

    std::cout << "  x*2 type: " << typeid(expr1).name() << "\n";
    std::cout << "  2*x type: " << typeid(expr2).name() << "\n";
    std::cout << "  Are same: "
              << (typeid(expr1).name() == typeid(expr2).name() ? "YES" : "NO")
              << "\n";

    auto simplified1 = simplify(expr1, default_context());
    auto simplified2 = simplify(expr2, default_context());
    std::cout << "  After simplify, x*2: " << typeid(simplified1).name()
              << "\n";
    std::cout << "  After simplify, 2*x: " << typeid(simplified2).name()
              << "\n";
    std::cout << "  Are same after simplify: "
              << (typeid(simplified1).name() == typeid(simplified2).name()
                      ? "YES"
                      : "NO")
              << "\n\n";
  }

  // Test constant folding directly
  {
    std::cout << "Testing constant folding:\n";

    auto expr = 1_c + 2_c;
    std::cout << "  1 + 2 type: " << typeid(expr).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(expr, BinderPack{}) << "\n";

    auto simplified = simplify(expr, default_context());
    std::cout << "  After simplify type: " << typeid(simplified).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(simplified, BinderPack{})
              << "\n";

    constexpr bool is_constant = match(simplified, 𝐜);
    std::cout << "  Is single constant: " << (is_constant ? "YES" : "NO")
              << "\n\n";
  }

  // Test nested constant in factored expression
  {
    std::cout << "Testing x*(1+2) factoring:\n";
    auto inner = 1_c + 2_c;
    auto expr = x * inner;

    std::cout << "  x*(1+2) type: " << typeid(expr).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(expr, BinderPack{x = 10})
              << "\n";

    auto simplified = simplify(expr, default_context());
    std::cout << "  After simplify type: " << typeid(simplified).name() << "\n";
    std::cout << "  Evaluates to: " << evaluate(simplified, BinderPack{x = 10})
              << "\n";

    constexpr bool is_x_times_const = match(simplified, x * 𝐜);
    std::cout << "  Is 'x * const': " << (is_x_times_const ? "YES" : "NO")
              << "\n\n";
  }

  return 0;
}
