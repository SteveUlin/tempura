#include "synchronization/threadsafe_queue.h"

#include <thread>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "ThreadSafeQueue basic operations"_test = [] {
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

    // Test tryPop on empty queue
    expectFalse(queue.tryPop(value));
  };

  "ThreadSafeQueue multi-threaded"_test = [] {
    ThreadSafeQueue<int> queue;

    // Producer thread
    std::thread producer([&queue] {
      for (int i = 0; i < 10; ++i) {
        queue.push(i);
      }
    });

    // Consumer thread
    std::thread consumer([&queue] {
      for (int i = 0; i < 10; ++i) {
        int value;
        queue.waitAndPop(value);
        expectEq(i, value);
      }
    });

    producer.join();
    consumer.join();
  };

  return TestRegistry::result();
}
