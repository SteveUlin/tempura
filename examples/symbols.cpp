#include <iostream>
#include <print>
#include "symbolic/symbolic.h"
#include "symbolic/operators.h"

using namespace tempura::symbolic;

auto main() -> int {
  TEMPURA_SYMBOL(x);
  TEMPURA_SYMBOL(y);

  constexpr auto expr = sin(x) * x + x;
  double z;
  std::cin >> z;
  std::println("sin(x) * x + x = {}", expr(Substitution{x = z}));
}

