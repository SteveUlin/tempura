#include "src/symbolic2/simplify.h"
#include "src/symbolic2/symbolic.h"
#include <iostream>

using namespace tempura;

int main() {
  Symbol x;

  auto expr = (x + 1_c) * (x + 1_c);
  std::cout << "Original: " << stringify(expr) << std::endl;

  auto s = simplify(expr);
  std::cout << "Simplified: " << stringify(s) << std::endl;

  auto result = evaluate(s, BinderPack{x = 5});
  std::cout << "Evaluated at x=5: " << result << std::endl;
  std::cout << "Expected: 36" << std::endl;

  return 0;
}
