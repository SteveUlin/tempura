#include "synchronization/threadsafe_stack.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "ThreadSafeStack"_test = [] {
    ThreadSafeStack<int> stack;

    // Test pushing and popping elements
    stack.push(1);
    stack.push(2);
    stack.push(3);

    int value;
    expectTrue(stack.pop(value));
    expectEq(3, value);
    expectTrue(stack.pop(value));
    expectEq(2, value);
    expectTrue(stack.pop(value));
    expectEq(1, value);

    // Test popping from an empty stack
    expectTrue(!stack.pop(value));
  };

  return TestRegistry::result();
}
