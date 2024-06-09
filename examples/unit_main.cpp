#include "src/unit.h"

using namespace tempura;

auto main() -> int {
  "sum"_test = [] {
    expectEq((1 + 2), 6);
  };

  return TestRegistry::result();
}
