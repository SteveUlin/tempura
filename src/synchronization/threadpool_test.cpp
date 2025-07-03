#include "synchronization/threadpool.h"

#include <print>

#include "unit.h"
#include "profiler.h"

using namespace tempura;

auto main() -> int {
  TEMPURA_TRACE();
  ThreadPool pool(4);

  // Test that we can enqueue a task and get the result
  auto future = pool.enqueue([] { return 42; });
  assert(future.get() == 42);

  // Test that we can enqueue multiple tasks
  std::vector<std::future<int>> futures;
  for (int i = 0; i < 1'000; ++i) {
    std::this_thread::sleep_for(1ms);  // Simulate some work
    futures.push_back(pool.enqueue([i] {
      return i * i;
    }));
  }

  for (int i = 0; i < 1'000; ++i) {
    assert(futures[i].get() == i * i);
  }


  return 0;
}

