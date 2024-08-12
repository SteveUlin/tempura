#include "src/containers/list.h"

#include "src/unit.h"

using namespace tempura;

auto main () -> int {
  "Size test"_test = [] {
    List<int> list;
    expectEq(0, list.size());
  };
}
