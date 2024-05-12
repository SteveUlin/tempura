#include "unit.h"

namespace tempura {

TestRegistry& TestRegistry::instance = *(new TestRegistry{});

}  // namespace tempura
