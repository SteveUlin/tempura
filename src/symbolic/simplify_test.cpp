#include "src/symbolic/simplify.h"

#include "src/symbolic/symbolic.h"
#include "src/symbolic/operators.h"
#include "src/symbolic/matchers.h"
#include "src/symbolic/to_string.h"

#include "src/unit.h"

#include <iostream>

using namespace tempura;
using namespace tempura::symbolic;

auto main() -> int {
  "Plus Reduce"_test = [] {
    constexpr auto a = SYMBOL();
    constexpr auto b = SYMBOL();
    std::cout << toString(reduceAddition(a + b + a), Substitution{a = "a", b = "b"}) << std::endl;

    std::cout << toString(reduceAddition(b * b + a + b + a + Constant<4>{} * b * b), Substitution{a = "a", b = "b"}) << std::endl;

    std::cout << toString(reduceMultiplication(b * b * a * b * a * Constant<4>{} * b * b), Substitution{a = "a", b = "b"}) << std::endl;

    std::cout << toString(simplify(a + a * b + a + a * b + a + Constant<4>{} * b * b), Substitution{a = "a", b = "b"}) << std::endl;
  };

  return TestRegistry::result();
}
