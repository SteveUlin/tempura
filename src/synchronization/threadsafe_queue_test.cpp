#include "synchronization/threadsafe_queue.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "ThreadSafeQueue"_test = [] {
    ThreadSafeQueue<int> queue;

    // Test pushing and popping elements
    queue.push(1);
    queue.push(2);
    queue.push(3);

    int value;
    queue.waitAndPop(value);
    expectEq(1, value);
    queue.waitAndPop(value);
    expectEq(2, value);
    queue.waitAndPop(value);
    expectEq(3, value);

    // Test popping from an empty queue
    expectTrue(!queue.tryPop(value));
  };

  return TestRegistry::result();
}
