#include "containers/list.h"

#include "unit.h"

using namespace tempura;

auto main () -> int {
  "Size test"_test = [] {
    List<int> list;
    expectEq(0, list.size());
  };
}
