#include <iostream>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

int main() {
  std::printf("=== Symbolic3 String Conversion and Debugging Demo ===\n\n");

  // Create some symbols
  Symbol x;
  Symbol y;
  Symbol z;

  // Build expressions
  auto expr1 = x + Constant<1>{};
  auto expr2 = Constant<2>{} * x;
  auto expr3 = (x + Constant<1>{}) * (y - Constant<3>{});
  auto expr4 = sin(x * y) + cos(z);
  auto expr5 = pow(x, Constant<2>{}) + Constant<2>{} * x + Constant<1>{};

  // Basic printing
  std::printf("1. Basic Expression Printing\n");
  std::printf("   =============================\n");
  debug_print(expr1, "x + 1");
  debug_print(expr2, "2 * x");
  debug_print(expr3, "(x + 1) * (y - 3)");
  debug_print(expr4, "sin(x*y) + cos(z)");
  debug_print(expr5, "x^2 + 2x + 1");

  std::printf("\n2. Compact Form (with type info)\n");
  std::printf("   =============================\n");
  debug_print_compact(expr1, "x + 1");
  debug_print_compact(expr4, "trig expression");

  std::printf("\n3. Tree Structure Visualization\n");
  std::printf("   =============================\n");
  debug_print_tree(expr3, 0, "(x + 1) * (y - 3)");

  std::printf("\n4. Detailed Tree for Complex Expression\n");
  std::printf("   =============================\n");
  auto complex = (sin(x) + cos(y)) * exp(-z);
  debug_print_tree(complex, 0, "(sin(x) + cos(y)) * exp(-z)");

  std::printf("\n5. More Complex Expressions\n");
  std::printf("   =============================\n");
  // Create some interesting expressions
  auto polynomial = pow(x, Constant<2>{}) + Constant<3>{} * x + Constant<2>{};
  auto rational = (x + y) / (x - y);

  debug_print(polynomial, "quadratic");
  debug_print(rational, "rational expression");

  std::printf("\n=== Demo Complete ===\n");
  return 0;
}
