// Tests for scheduler implementations

#include "task/task.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
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

  // ==========================================================================
  // ThreadPoolScheduler Tests
  // ==========================================================================

  "ThreadPoolScheduler - concept validation"_test = [] {
    static_assert(Scheduler<ThreadPoolScheduler>);
    expectTrue(true);
  };

  "ThreadPoolScheduler - schedule returns sender"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};
    auto sender = scheduler.schedule();
    static_assert(Sender<decltype(sender)>);
    expectTrue(true);
  };

  "ThreadPoolScheduler - executes on worker thread"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    auto calling_thread_id = std::this_thread::get_id();
    std::thread::id execution_thread_id;

    auto sender = scheduler.schedule() | then([&] {
                    execution_thread_id = std::this_thread::get_id();
                    return 42;
                  });

    auto result = syncWait(std::move(sender));

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);
    expectTrue(calling_thread_id != execution_thread_id);
  };

  "ThreadPoolScheduler - multiple operations"_test = [] {
    ThreadPool pool{4};
    ThreadPoolScheduler scheduler{pool};

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

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectTrue(result3.has_value());
    expectEq(counter.load(), 3);
  };

  "ThreadPoolScheduler - chained operations"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    auto sender = scheduler.schedule() | then([] { return 10; }) |
                  then([](int x) { return x * 2; }) |
                  then([](int x) { return x + 5; });

    auto result = syncWait(std::move(sender));

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5
  };

  "ThreadPoolScheduler - multiple workers available"_test = [] {
    // Test that a thread pool with multiple threads can handle
    // multiple concurrent operations
    ThreadPool pool{4};
    ThreadPoolScheduler scheduler{pool};

    std::atomic<int> completed_count{0};
    std::mutex ids_mutex;
    std::set<std::thread::id> thread_ids;

    auto task = [&] {
      {
        std::scoped_lock lock{ids_mutex};
        thread_ids.insert(std::this_thread::get_id());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      ++completed_count;
      return 1;
    };

    // Schedule 8 tasks
    auto sender1 = scheduler.schedule() | then(task);
    auto sender2 = scheduler.schedule() | then(task);
    auto sender3 = scheduler.schedule() | then(task);
    auto sender4 = scheduler.schedule() | then(task);
    auto sender5 = scheduler.schedule() | then(task);
    auto sender6 = scheduler.schedule() | then(task);
    auto sender7 = scheduler.schedule() | then(task);
    auto sender8 = scheduler.schedule() | then(task);

    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));
    auto result3 = syncWait(std::move(sender3));
    auto result4 = syncWait(std::move(sender4));
    auto result5 = syncWait(std::move(sender5));
    auto result6 = syncWait(std::move(sender6));
    auto result7 = syncWait(std::move(sender7));
    auto result8 = syncWait(std::move(sender8));

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectTrue(result3.has_value());
    expectTrue(result4.has_value());
    expectTrue(result5.has_value());
    expectTrue(result6.has_value());
    expectTrue(result7.has_value());
    expectTrue(result8.has_value());

    // All 8 tasks completed
    expectEq(completed_count.load(), 8);

    // Tasks used multiple threads from the pool
    expectTrue(thread_ids.size() > 1);
    expectTrue(thread_ids.size() <= 4);
  };

  "ThreadPoolScheduler - shared pool"_test = [] {
    ThreadPool pool{2};

    // Multiple schedulers sharing the same pool
    ThreadPoolScheduler scheduler1{pool};
    ThreadPoolScheduler scheduler2{pool};

    std::atomic<int> counter{0};

    auto sender1 =
        scheduler1.schedule() | then([&counter] { return ++counter; });
    auto sender2 =
        scheduler2.schedule() | then([&counter] { return ++counter; });

    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectEq(counter.load(), 2);
  };

  "ThreadPoolScheduler - letValue composition"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    auto sender =
        scheduler.schedule() | then([] { return 5; }) |
        letValue([](int x) { return just(x * 3, x + 2); }) |
        then([](int a, int b) { return a + b; });

    auto result = syncWait(std::move(sender));

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 22);  // (5 * 3) + (5 + 2) = 15 + 7 = 22
  };

  // ==========================================================================
  // NewThreadScheduler Tests
  // ==========================================================================

  "NewThreadScheduler - concept validation"_test = [] {
    static_assert(Scheduler<NewThreadScheduler>);
    expectTrue(true);
  };

  "NewThreadScheduler - schedule returns sender"_test = [] {
    NewThreadContext context;
    NewThreadScheduler scheduler{context};
    auto sender = scheduler.schedule();
    static_assert(Sender<decltype(sender)>);
    expectTrue(true);
  };

  "NewThreadScheduler - executes on different thread"_test = [] {
    NewThreadContext context;
    NewThreadScheduler scheduler{context};

    auto calling_thread_id = std::this_thread::get_id();
    std::thread::id execution_thread_id;

    auto sender = scheduler.schedule() | then([&] {
                    execution_thread_id = std::this_thread::get_id();
                    return 42;
                  });

    auto result = syncWait(std::move(sender));

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);
    expectTrue(calling_thread_id != execution_thread_id);
  };

  "NewThreadScheduler - multiple operations create multiple threads"_test = [] {
    NewThreadContext context;
    NewThreadScheduler scheduler{context};

    std::set<std::thread::id> thread_ids;
    std::mutex ids_mutex;

    auto sender1 = scheduler.schedule() | then([&] {
                     std::scoped_lock lock{ids_mutex};
                     thread_ids.insert(std::this_thread::get_id());
                     return 1;
                   });

    auto sender2 = scheduler.schedule() | then([&] {
                     std::scoped_lock lock{ids_mutex};
                     thread_ids.insert(std::this_thread::get_id());
                     return 2;
                   });

    auto sender3 = scheduler.schedule() | then([&] {
                     std::scoped_lock lock{ids_mutex};
                     thread_ids.insert(std::this_thread::get_id());
                     return 3;
                   });

    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));
    auto result3 = syncWait(std::move(sender3));

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectTrue(result3.has_value());

    // Each task should run on a different thread
    expectEq(thread_ids.size(), 3);
  };

  "NewThreadScheduler - chained operations"_test = [] {
    NewThreadContext context;
    NewThreadScheduler scheduler{context};

    auto sender = scheduler.schedule() | then([] { return 10; }) |
                  then([](int x) { return x * 2; }) |
                  then([](int x) { return x + 5; });

    auto result = syncWait(std::move(sender));

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5
  };

  "NewThreadScheduler - letValue composition"_test = [] {
    NewThreadContext context;
    NewThreadScheduler scheduler{context};

    auto sender =
        scheduler.schedule() | then([] { return 5; }) |
        letValue([](int x) { return just(x * 3, x + 2); }) |
        then([](int a, int b) { return a + b; });

    auto result = syncWait(std::move(sender));

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 22);  // (5 * 3) + (5 + 2) = 15 + 7 = 22
  };

  "NewThreadScheduler - threads are joined on destruction"_test = [] {
    std::atomic<bool> task_completed{false};
    std::atomic<bool> task_started{false};

    {
      NewThreadContext context;
      NewThreadScheduler scheduler{context};

      auto sender = scheduler.schedule() | then([&] {
                      task_started.store(true);
                      std::this_thread::sleep_for(std::chrono::milliseconds(50));
                      task_completed.store(true);
                      return 42;
                    });

      // Trigger the task asynchronously without waiting
      std::thread trigger([s = std::move(sender)]() mutable {
        auto result = syncWait(std::move(s));
      });

      // Wait for task to start
      while (!task_started.load()) {
        std::this_thread::yield();
      }

      trigger.join();

      // Context destructor should join the thread
    }

    // After context destruction, thread should have been joined
    // and task should have completed
    expectTrue(task_completed.load());
  };

  // ==========================================================================
  // TimerQueue Tests
  // ==========================================================================

  "TimerQueue - scheduleAfter executes task"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    std::atomic<bool> executed{false};

    auto sender = scheduleAfter(queue, std::chrono::milliseconds(50)) |
                  then([&] {
                    executed.store(true);
                    return 42;
                  });

    auto result = syncWait(std::move(sender));

    queue.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);
    expectTrue(executed.load());
  };

  "TimerQueue - scheduleAfter respects delay"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    auto start = std::chrono::steady_clock::now();
    auto delay = std::chrono::milliseconds(100);

    auto sender =
        scheduleAfter(queue, delay) | then([] { return 42; });

    auto result = syncWait(std::move(sender));
    auto elapsed = std::chrono::steady_clock::now() - start;

    queue.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);

    // Verify delay was respected (allow 10ms tolerance)
    expectTrue(elapsed >= delay - std::chrono::milliseconds(10));
  };

  "TimerQueue - scheduleAt executes at correct time"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    auto delay = std::chrono::milliseconds(100);
    auto when = TimerQueue::Clock::now() + delay;
    auto start = std::chrono::steady_clock::now();

    auto sender = scheduleAt(queue, when) | then([] { return 42; });

    auto result = syncWait(std::move(sender));
    auto elapsed = std::chrono::steady_clock::now() - start;

    queue.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);

    // Verify execution happened at approximately the right time
    expectTrue(elapsed >= delay - std::chrono::milliseconds(10));
  };

  "TimerQueue - multiple tasks execute in order"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    std::vector<int> execution_order;
    std::mutex order_mutex;

    // Schedule tasks with different delays (reverse order)
    auto sender3 = scheduleAfter(queue, std::chrono::milliseconds(150)) |
                   then([&] {
                     std::scoped_lock lock{order_mutex};
                     execution_order.push_back(3);
                     return 3;
                   });

    auto sender2 = scheduleAfter(queue, std::chrono::milliseconds(100)) |
                   then([&] {
                     std::scoped_lock lock{order_mutex};
                     execution_order.push_back(2);
                     return 2;
                   });

    auto sender1 = scheduleAfter(queue, std::chrono::milliseconds(50)) |
                   then([&] {
                     std::scoped_lock lock{order_mutex};
                     execution_order.push_back(1);
                     return 1;
                   });

    auto result1 = syncWait(std::move(sender1));
    auto result2 = syncWait(std::move(sender2));
    auto result3 = syncWait(std::move(sender3));

    queue.stop();
    worker.join();

    expectTrue(result1.has_value());
    expectTrue(result2.has_value());
    expectTrue(result3.has_value());

    // Verify execution order matches schedule times (not submission order)
    expectEq(execution_order.size(), 3);
    expectEq(execution_order[0], 1);
    expectEq(execution_order[1], 2);
    expectEq(execution_order[2], 3);
  };

  "TimerQueue - chained operations"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    auto sender = scheduleAfter(queue, std::chrono::milliseconds(50)) |
                  then([] { return 10; }) | then([](int x) { return x * 2; }) |
                  then([](int x) { return x + 5; });

    auto result = syncWait(std::move(sender));

    queue.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5
  };

  "TimerQueue - letValue composition"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    auto sender =
        scheduleAfter(queue, std::chrono::milliseconds(50)) |
        then([] { return 5; }) |
        letValue([](int x) { return just(x * 3, x + 2); }) |
        then([](int a, int b) { return a + b; });

    auto result = syncWait(std::move(sender));

    queue.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 22);  // (5 * 3) + (5 + 2) = 15 + 7 = 22
  };

  "TimerQueue - immediate execution for past time"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    // Schedule a task for a time in the past
    auto when = TimerQueue::Clock::now() - std::chrono::seconds(1);
    auto start = std::chrono::steady_clock::now();

    auto sender = scheduleAt(queue, when) | then([] { return 42; });

    auto result = syncWait(std::move(sender));
    auto elapsed = std::chrono::steady_clock::now() - start;

    queue.stop();
    worker.join();

    expectTrue(result.has_value());
    expectEq(std::get<0>(*result), 42);

    // Should execute immediately (within 50ms tolerance)
    expectTrue(elapsed < std::chrono::milliseconds(50));
  };

  "TimerQueue - concurrent scheduling"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    std::atomic<int> counter{0};

    // Schedule multiple tasks concurrently from different threads
    std::vector<std::thread> schedulers;
    for (int i = 0; i < 10; ++i) {
      schedulers.emplace_back([&queue, &counter, i] {
        auto sender = scheduleAfter(
                          queue, std::chrono::milliseconds(50 + i * 10)) |
                      then([&counter] {
                        ++counter;
                        return 1;
                      });
        auto result = syncWait(std::move(sender));
      });
    }

    for (auto& t : schedulers) {
      t.join();
    }

    queue.stop();
    worker.join();

    expectEq(counter.load(), 10);
  };

  "TimerQueue - stop drains pending tasks"_test = [] {
    TimerQueue queue;
    std::thread worker([&queue] { queue.run(); });

    std::atomic<int> executed_count{0};

    // Schedule tasks with various delays
    auto sender1 = scheduleAfter(queue, std::chrono::milliseconds(50)) |
                   then([&executed_count] {
                     ++executed_count;
                     return 1;
                   });

    auto sender2 = scheduleAfter(queue, std::chrono::milliseconds(100)) |
                   then([&executed_count] {
                     ++executed_count;
                     return 2;
                   });

    auto sender3 = scheduleAfter(queue, std::chrono::milliseconds(150)) |
                   then([&executed_count] {
                     ++executed_count;
                     return 3;
                   });

    // Start all senders
    std::thread t1([s = std::move(sender1)]() mutable {
      auto result = syncWait(std::move(s));
    });
    std::thread t2([s = std::move(sender2)]() mutable {
      auto result = syncWait(std::move(s));
    });
    std::thread t3([s = std::move(sender3)]() mutable {
      auto result = syncWait(std::move(s));
    });

    t1.join();
    t2.join();
    t3.join();

    queue.stop();
    worker.join();

    // All tasks should have executed
    expectEq(executed_count.load(), 3);
  };

  return 0;
}
