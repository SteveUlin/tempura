#include "complie_time_string.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "CST Compare"_test = []{
    static_assert("A test string"_cts == "A test string"_cts);
  };
  return 0;
}
