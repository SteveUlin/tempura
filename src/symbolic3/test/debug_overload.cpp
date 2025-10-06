// Ultra-simple test
#include <iostream>

#include "symbolic3/core.h"

using namespace tempura::symbolic3;

// Test overload resolution manually
constexpr bool test_match(Constant<1>, Constant<1>) { return true; }
constexpr bool test_match(Constant<1>, Constant<2>) { return false; }

int main() {
  constexpr bool result = test_match(Constant<1>{}, Constant<1>{});
  std::cout << "Manual test: " << (result ? "YES" : "NO") << "\n";
  return 0;
}
