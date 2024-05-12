#include "unit.h"

using namespace tempura;

int main() {
  "sum"_test = [] {
    expectEq((1 + 2), 6);
  };
}
