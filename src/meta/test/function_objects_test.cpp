#include "meta/function_objects.h"

#include <memory>
#include <print>

#include "type_id.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "AddOp"_test = [] {
    static_assert(AddOp{}(2, 3) == 5);
  };
}
