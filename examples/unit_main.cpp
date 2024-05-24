#include "unit.h"

#include <print>

using namespace tempura;

int main() {
  "sum"_test = [] {
    expectEq((1 + 2), 6);
  };

  return TestRegistry::result();
}
