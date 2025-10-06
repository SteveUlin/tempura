// Test is_expression directly
#include <iostream>
#include <type_traits>

#include "symbolic3/core.h"
#include "symbolic3/operators.h"

using namespace tempura::symbolic3;

int main() {
  using MyExpr = Expression<AddOp, Constant<1>, Constant<2>>;

  std::cout << "is_expression<MyExpr> = " << is_expression<MyExpr> << "\n";
  std::cout << "is_expression<Constant<1>> = "
            << is_expression<Constant<1>> << "\n";
  std::cout << "is_expression<Symbol<int>> = "
            << is_expression<Symbol<int>> << "\n";

  // Test with actual variable
  constexpr auto expr = Expression<AddOp, Constant<1>, Constant<2>>{};
  std::cout << "\nis_expression<decltype(expr)> = "
            << is_expression<decltype(expr)> << "\n";
  std::cout << "is_expression<std::remove_cvref_t<decltype(expr)>> = "
            << is_expression<std::remove_cvref_t<decltype(expr)>> << "\n";

  //  Test with operator result
  constexpr auto c1 = Constant<1>{};
  constexpr auto c2 = Constant<2>{};
  constexpr auto sum = c1 + c2;
  std::cout << "is_expression<decltype(sum)> = "
            << is_expression<decltype(sum)> << "\n";
  std::cout << "Type name: " << typeid(decltype(sum)).name() << "\n";

  return 0;
}
