#include "event_loop.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace std::chrono_literals;

auto main() -> int {
  "Basic task execution"_test = [] {
    EventLoop loop;
    std::atomic<int> counter{0};

    std::thread worker([&loop] { loop.run(); });

    loop.post([&counter] { counter++; });
    loop.post([&counter] { counter++; });
    loop.post([&counter] { counter++; });

    loop.stop();
    worker.join();

    expectEq(3, counter.load());
  };

  "Multiple producers"_test = [] {
    EventLoop loop;
    std::atomic<int> counter{0};

    std::thread worker([&loop] { loop.run(); });

    // Multiple threads posting tasks
    std::vector<std::thread> producers;
    for (int i = 0; i < 5; ++i) {
      producers.emplace_back([&loop, &counter] {
        for (int j = 0; j < 10; ++j) {
          loop.post([&counter] { counter++; });
        }
      });
    }

    for (auto& t : producers) {
      t.join();
    }

    loop.stop();
    worker.join();

    // All tasks from producers should have executed (5 threads × 10 tasks)
    expectEq(50, counter.load());
  };

  "Stop with draining queue"_test = [] {
    EventLoop loop;
    std::atomic<int> counter{0};

    std::thread worker([&loop] { loop.run(); });

    // Post many tasks
    for (int i = 0; i < 100; ++i) {
      loop.post([&counter] { counter++; });
    }

    // Stop - should drain all 100 tasks before returning
    loop.stop();
    worker.join();

    // All tasks should have executed
    expectEq(100, counter.load());
  };

  "Rejects tasks after shutdown"_test = [] {
    EventLoop loop;

    std::thread worker([&loop] { loop.run(); });

    // Shut down the loop
    loop.stop();
    worker.join();

    // These tasks should be rejected after shutdown
    expectEq(false, loop.post([] {}));
    expectEq(false, loop.post([] {}));
    expectEq(false, loop.post([] {}));
  };

  "Serial execution order"_test = [] {
    EventLoop loop;
    std::vector<int> execution_order;
    std::mutex order_mutex;

    std::thread worker([&loop] { loop.run(); });

    // Post tasks in order
    for (int i = 0; i < 10; ++i) {
      loop.post([i, &execution_order, &order_mutex] {
        std::scoped_lock lock{order_mutex};
        execution_order.push_back(i);
      });
    }

    loop.stop();
    worker.join();

    // Verify tasks executed in FIFO order
    expectEq(10UL, execution_order.size());
    for (size_t i = 0; i < execution_order.size(); ++i) {
      expectEq(static_cast<int>(i), execution_order[i]);
    }
  };

  "Empty queue doesn't block stop"_test = [] {
    EventLoop loop;

    std::thread worker([&loop] { loop.run(); });

    // Stop without posting any tasks - should not block
    loop.stop();
    worker.join();

    expectTrue(true);  // If we get here, it worked
  };

  return 0;
}
