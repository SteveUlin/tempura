#include "compile_time_string.h"

#include "unit.h"

#include <iostream>

using namespace tempura;

auto main() -> int {
  "CST Compare"_test = []{
    static_assert("A test string"_cts == "A test string"_cts);
  };

  "CST Add"_test = []{
    static_assert("A test "_cts + "string"_cts == "A test string"_cts);
  };

  "CST Convert Int"_test = []{
    static_assert(toCTS<124>() == "124"_cts);
    static_assert(toCTS<(-552)>() == "-552"_cts);
    static_assert(toCTS<0.5>() == "0.500"_cts);
  };

  return 0;
}
