#include <iostream>

#include "symbolic3/symbolic3.h"

using namespace tempura::symbolic3;

int main() {
  constexpr Symbol x;
  constexpr Symbol y;
  constexpr Symbol z;
  constexpr auto ctx = default_context();

  // Test case: x * (y + (z * 0))
  constexpr auto expr = x * (y + (z * 0_c));
  constexpr auto result = full_simplify(expr, ctx);

  std::cout << "Expression: x * (y + (z * 0))\n";
  std::cout << "Result type: " << type_name<decltype(result)>() << "\n";

  // Try intermediate steps
  std::cout << "\nTesting intermediate simplifications:\n";

  // First: z * 0 should become 0
  constexpr auto step1 = z * 0_c;
  constexpr auto step1_simp = algebraic_simplify.apply(step1, ctx);
  std::cout << "z * 0 simplifies to: " << to_string(step1_simp) << "\n";

  // Second: y + 0 should become y
  constexpr auto step2 = y + 0_c;
  constexpr auto step2_simp = algebraic_simplify.apply(step2, ctx);
  std::cout << "y + 0 simplifies to: " << to_string(step2_simp) << "\n";

  // Third: x * y should stay as x * y
  constexpr auto step3 = x * y;
  std::cout << "x * y stays as: " << to_string(step3) << "\n";

  return 0;
}
