// Tests for scheduler implementations

#include "task/task.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // ==========================================================================
  // InlineScheduler Tests
  // ==========================================================================

  "InlineScheduler - concept validation"_test = [] {
    static_assert(Scheduler<InlineScheduler>);
    expectTrue(true);
  };

  "InlineScheduler - schedule returns sender"_test = [] {
    InlineScheduler scheduler;
    auto sender = scheduler.schedule();
    static_assert(Sender<decltype(sender)>);
    expectTrue(true);
  };

  "InlineScheduler - executes on calling thread"_test = [] {
    InlineScheduler scheduler;
    auto calling_thread_id = std::this_thread::get_id();
    std::thread::id execution_thread_id;

    auto sender = scheduler.schedule() | then([&] {
                    execution_thread_id = std::this_thread::get_id();
                    return 42;
                  });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);
    expectTrue(calling_thread_id == execution_thread_id);
  };

  "InlineScheduler - chained operations"_test = [] {
    InlineScheduler scheduler;

    auto sender = scheduler.schedule() | then([] { return 10; }) |
                  then([](int x) { return x * 2; }) |
                  then([](int x) { return x + 5; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5
  };

  // ==========================================================================
  // EventLoopScheduler Tests
  // ==========================================================================

  "EventLoopScheduler - concept validation"_test = [] {
    static_assert(Scheduler<EventLoopScheduler>);
    expectTrue(true);
  };

  "EventLoopScheduler - schedule returns sender"_test = [] {
    EventLoop loop;
    EventLoopScheduler scheduler{loop};
    auto sender = scheduler.schedule();
    static_assert(Sender<decltype(sender)>);
    expectTrue(true);
  };

  "EventLoopScheduler - executes on worker thread"_test = [] {
    EventLoop loop;
    std::thread worker([&loop] { loop.run(); });

    auto calling_thread_id = std::this_thread::get_id();
    std::thread::id execution_thread_id;

    EventLoopScheduler scheduler{loop};
    auto sender = scheduler.schedule() | then([&] {
                    execution_thread_id = std::this_thread::get_id();
                    return 42;
                  });

    auto result = syncWait(std::move(sender));

    loop.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);
    expectTrue(calling_thread_id != execution_thread_id);
  };

  "EventLoopScheduler - multiple operations"_test = [] {
    EventLoop loop;
    std::thread worker([&loop] { loop.run(); });

    EventLoopScheduler scheduler{loop};
    std::atomic<int> counter{0};

    // Schedule multiple operations
    auto sender1 =
        scheduler.schedule() | then([&counter] { return ++counter; });
    auto sender2 =
        scheduler.schedule() | then([&counter] { return ++counter; });
    auto sender3 =
        scheduler.schedule() | then([&counter] { return ++counter; });

    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));
    auto result3 = syncWait(std::move(sender3));

    loop.stop();
    worker.join();

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectTrue(result3.has_value());
    expectEq(counter.load(), 3);
  };

  "EventLoopScheduler - chained operations"_test = [] {
    EventLoop loop;
    std::thread worker([&loop] { loop.run(); });

    EventLoopScheduler scheduler{loop};

    auto sender = scheduler.schedule() | then([] { return 10; }) |
                  then([](int x) { return x * 2; }) |
                  then([](int x) { return x + 5; });

    auto result = syncWait(std::move(sender));

    loop.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5
  };

  "EventLoopScheduler - letValue composition"_test = [] {
    EventLoop loop;
    std::thread worker([&loop] { loop.run(); });

    EventLoopScheduler scheduler{loop};

    auto sender =
        scheduler.schedule() | then([] { return 5; }) |
        letValue([](int x) { return just(x * 3, x + 2); }) |
        then([](int a, int b) { return a + b; });

    auto result = syncWait(std::move(sender));

    loop.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 22);  // (5 * 3) + (5 + 2) = 15 + 7 = 22
  };

  "EventLoopScheduler - shared event loop"_test = [] {
    EventLoop loop;
    std::thread worker([&loop] { loop.run(); });

    // Multiple schedulers sharing the same event loop
    EventLoopScheduler scheduler1{loop};
    EventLoopScheduler scheduler2{loop};

    std::atomic<int> counter{0};

    auto sender1 =
        scheduler1.schedule() | then([&counter] { return ++counter; });
    auto sender2 =
        scheduler2.schedule() | then([&counter] { return ++counter; });

    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));

    loop.stop();
    worker.join();

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectEq(counter.load(), 2);
  };

  "EventLoopScheduler - FIFO ordering"_test = [] {
    EventLoop loop;
    std::thread worker([&loop] { loop.run(); });

    EventLoopScheduler scheduler{loop};

    std::atomic<int> counter{0};
    std::vector<int> execution_order;
    std::mutex order_mutex;

    // Schedule operations that record their execution order
    auto sender1 = scheduler.schedule() | then([&] {
                     std::scoped_lock lock{order_mutex};
                     execution_order.push_back(1);
                     return 1;
                   });

    auto sender2 = scheduler.schedule() | then([&] {
                     std::scoped_lock lock{order_mutex};
                     execution_order.push_back(2);
                     return 2;
                   });

    auto sender3 = scheduler.schedule() | then([&] {
                     std::scoped_lock lock{order_mutex};
                     execution_order.push_back(3);
                     return 3;
                   });

    // Wait for all to complete
    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));
    auto result3 = syncWait(std::move(sender3));

    loop.stop();
    worker.join();

    // Verify FIFO order
    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectTrue(result3.has_value());
    expectEq(execution_order.size(), 3);
    expectEq(execution_order[0], 1);
    expectEq(execution_order[1], 2);
    expectEq(execution_order[2], 3);
  };

  return 0;
}
