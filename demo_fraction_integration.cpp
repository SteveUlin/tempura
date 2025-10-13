#include <iostream>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

int main() {
  std::cout << "=== Fraction Integration Demo ===\n\n";

  // 1. Exact integer division
  std::cout << "1. Exact Division:\n";
  {
    auto expr = Constant<6>{} / Constant<2>{};
    auto result = simplify(expr, default_context());
    std::cout << "   6 / 2 = " << toStringRuntime(result) << "\n";
    std::cout << "   Type: Constant<3>\n\n";
  }

  // 2. Non-integer division
  std::cout << "2. Non-Integer Division:\n";
  {
    auto expr = Constant<5>{} / Constant<2>{};
    auto result = simplify(expr, default_context());
    std::cout << "   5 / 2 = " << toStringRuntime(result) << "\n";
    std::cout << "   Type: Fraction<5, 2>\n";
    std::cout << "   Value: " << evaluate(result, BinderPack{}) << "\n\n";
  }

  // 3. Automatic GCD reduction
  std::cout << "3. Automatic GCD Reduction:\n";
  {
    auto expr = Constant<4>{} / Constant<6>{};
    auto result = simplify(expr, default_context());
    std::cout << "   4 / 6 = " << toStringRuntime(result) << " (reduced)\n";
    std::cout << "   Type: Fraction<2, 3>\n\n";
  }

  // 4. Fractions in symbolic expressions
  std::cout << "4. Fractions in Symbolic Expressions:\n";
  {
    Symbol x;
    auto half_expr = Constant<1>{} / Constant<2>{};
    auto expr = x * half_expr;
    auto result = simplify(expr, default_context());
    std::cout << "   x * (1/2) = " << toStringRuntime(result) << "\n";

    auto value = evaluate(result, BinderPack{x = 10.0});
    std::cout << "   When x=10: " << value << "\n\n";
  }

  // 5. Complex expression with multiple fractions
  std::cout << "5. Complex Expression:\n";
  {
    Symbol x;
    auto expr = x * (Constant<1>{} / Constant<3>{}) +
                x * (Constant<2>{} / Constant<3>{});
    auto result = simplify(expr, default_context());
    std::cout << "   x*(1/3) + x*(2/3) = " << toStringRuntime(result) << "\n";
    std::cout << "   (Should simplify to x after factoring)\n\n";
  }

  // 6. Fraction arithmetic
  std::cout << "6. Fraction Arithmetic (Manual):\n";
  {
    auto half = Fraction<1, 2>{};
    auto third = Fraction<1, 3>{};
    auto sum = half + third;
    std::cout << "   1/2 + 1/3 = " << toStringRuntime(sum) << "\n";
    std::cout << "   Numerator: " << sum.numerator
              << ", Denominator: " << sum.denominator << "\n\n";
  }

  // 7. Comparison with float evaluation
  std::cout << "7. Exact vs. Float:\n";
  {
    auto frac_expr = Constant<1>{} / Constant<3>{};
    auto frac = simplify(frac_expr, default_context());

    std::cout << "   Fraction: " << toStringRuntime(frac) << " (exact)\n";
    std::cout << "   As double: " << evaluate(frac, BinderPack{}) << "\n";
    std::cout << "   Maintains exact representation until evaluation!\n\n";
  }

  std::cout << "=== All examples completed successfully! ===\n";
  return 0;
}
