// Minimal test of Expression matching
#include <iostream>

#include "symbolic3/constants.h"
#include "symbolic3/core.h"
#include "symbolic3/matching.h"
#include "symbolic3/operators.h"

using namespace tempura::symbolic3;

int main() {
  // Test value-based matching
  constexpr auto expr1 = Expression<AddOp, Constant<1>, Constant<2>>{};
  constexpr auto expr2 = Expression<AddOp, Constant<1>, Constant<2>>{};

  // Fully qualify to ensure we're calling the right function
  constexpr bool result = tempura::symbolic3::match(expr1, expr2);
  std::cout << "Same Expression instances match: " << (result ? "YES" : "NO")
            << "\n";

  // Test different arguments
  constexpr auto expr3 = Expression<AddOp, Constant<1>, Constant<3>>{};
  constexpr bool result2 = tempura::symbolic3::match(expr1, expr3);
  std::cout << "Different constants match: " << (result2 ? "YES" : "NO")
            << "\n";

  // Test simple constant
  constexpr auto c1 = Constant<5>{};
  constexpr auto c2 = Constant<5>{};
  constexpr bool result3 = tempura::symbolic3::match(c1, c2);
  std::cout << "Same constants match: " << (result3 ? "YES" : "NO") << "\n";

  // Check if is_expression works
  std::cout << "\nis_expression<decltype(expr1)> = "
            << is_expression<decltype(expr1)> << "\n";

  return 0;
}
