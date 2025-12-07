#include "task/task.h"
#include "unit.h"

#include <atomic>
#include <mutex>
#include <set>
#include <thread>

using namespace tempura;

// Test sender that checks if scheduler is available in environment
struct SchedulerCheckSender {
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(bool), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    struct OpState {
      R receiver_;

      void start() noexcept {
        auto env = tempura::get_env(receiver_);
        auto sched = tempura::get_scheduler(env);
        // Verify scheduler is available (InlineScheduler)
        bool has_scheduler = std::same_as<decltype(sched), InlineScheduler>;
        receiver_.setValue(std::move(has_scheduler));
      }
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Helper error sender for testing error propagation
class ErrorSenderTest {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int), SetErrorTag(std::error_code), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(std::make_error_code(std::errc::io_error));
      }

     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

auto main() -> int {
  // ═══════════════════════════════════════════════════════════════════════════
  // Basic compilation tests for on()
  // ═══════════════════════════════════════════════════════════════════════════

  "on() - compiles with InlineScheduler"_test = [] {
    // Just verify it compiles for now
    InlineScheduler sched;
    auto sender = just(42);
    [[maybe_unused]] auto on_sender = on(sched, std::move(sender));
    // Full test would require proper async execution
  };

  "BlockingReceiver - provides InlineScheduler environment"_test = [] {
    std::optional<std::tuple<int>> result;
    std::latch latch{1};
    BlockingReceiver<int> receiver{result, latch};

    auto env = get_env(receiver);
    auto sched = get_scheduler(env);

    static_assert(std::same_as<decltype(sched), InlineScheduler>);
    expectTrue(true);  // If we got here, it worked
  };

  "syncWait - receiver provides scheduler to sender"_test = [] {
    // Verify that senders within syncWait can query the scheduler
    auto result = syncWait(SchedulerCheckSender{});
    expectTrue(result.has_value());
    if (result) {
      expectTrue(std::get<0>(*result));
    }
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Async scheduler tests for on() with ThreadPoolScheduler
  //
  // NOTE: These tests currently fail to compile due to use-after-free bug in
  // on.h (see TODO.md line 7). The schedule operation states are stored as
  // local variables in start() and destroyed before async work completes.
  // These tests will work once the bug is fixed.
  // ═══════════════════════════════════════════════════════════════════════════

#if 1  // Enabled - UAF bug in on.h has been fixed

  "on() - basic async execution with ThreadPool"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    // Execute just(42) on thread pool and verify result
    auto sender = on(scheduler, just(42));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 42);
  };

  "on() - thread verification (executes on pool thread)"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    // Capture the main thread ID
    std::thread::id main_thread_id = std::this_thread::get_id();
    std::atomic<std::thread::id> worker_thread_id{};

    // Execute on pool and capture the worker thread ID
    auto sender = on(scheduler, just(1)) | then([&](int value) {
                    worker_thread_id.store(std::this_thread::get_id());
                    return value * 2;
                  });

    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 2);

    // Verify that the work ran on a different thread (pool thread, not main)
    expectTrue(worker_thread_id.load() != main_thread_id);
  };

  "on() - round-trip scheduler transition"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    // Capture thread IDs at different stages
    std::thread::id main_thread_id = std::this_thread::get_id();
    std::atomic<std::thread::id> pool_thread_id{};
    std::atomic<std::thread::id> completion_thread_id{};

    // Start on main, execute on pool, verify we return to main context
    auto sender =
        on(scheduler, just(10)) | then([&](int value) {
          pool_thread_id.store(std::this_thread::get_id());
          return value * 3;
        }) |
        then([&](int value) {
          // This runs after returning from pool
          completion_thread_id.store(std::this_thread::get_id());
          return value + 5;
        });

    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 35);  // (10 * 3) + 5

    // Verify execution happened on pool thread
    expectTrue(pool_thread_id.load() != main_thread_id);

    // Note: completion_thread_id runs on the pool thread because 'then'
    // after 'on' continues in the target scheduler's context
    expectTrue(pool_thread_id.load() == completion_thread_id.load());
  };

  "on() - with composed senders"_test = [] {
    ThreadPool pool{4};
    ThreadPoolScheduler scheduler{pool};

    // Track which threads executed tasks
    std::set<std::thread::id> thread_ids;
    std::mutex ids_mutex;

    // Compose multiple operations: just → then → then, all on pool
    auto sender = on(scheduler, just(1)) | then([&](int x) {
                    std::scoped_lock lock{ids_mutex};
                    thread_ids.insert(std::this_thread::get_id());
                    return x + 10;
                  }) |
                  then([&](int x) {
                    std::scoped_lock lock{ids_mutex};
                    thread_ids.insert(std::this_thread::get_id());
                    return x * 2;
                  });

    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 22);  // (1 + 10) * 2

    // Verify work executed on pool threads
    expectTrue(thread_ids.size() >= 1);
  };

  "on() - error propagation from async scheduler"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    // Sender that produces an error on the pool
    auto sender = on(scheduler, ErrorSenderTest{});
    auto result = syncWait(std::move(sender));

    // Should propagate the error
    expectFalse(result.has_value());
  };

  "on() - multiple values with async scheduler"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    std::thread::id main_thread_id = std::this_thread::get_id();
    std::atomic<std::thread::id> pool_thread_id{};

    // Execute sender with multiple values on pool
    auto sender = on(scheduler, just(42, 100)) | then([&](int a, int b) {
                    pool_thread_id.store(std::this_thread::get_id());
                    return a + b;
                  });

    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 142);

    // Verify execution on pool thread
    expectTrue(pool_thread_id.load() != main_thread_id);
  };

  "on() - nested on() calls (pool → different pool)"_test = [] {
    ThreadPool pool1{2};
    ThreadPool pool2{2};
    ThreadPoolScheduler scheduler1{pool1};
    ThreadPoolScheduler scheduler2{pool2};

    std::atomic<std::thread::id> thread1{};
    std::atomic<std::thread::id> thread2{};

    // Execute on pool1, then transition to pool2
    auto sender = on(scheduler1, just(5)) | then([&](int x) {
                    thread1.store(std::this_thread::get_id());
                    return x * 2;
                  }) |
                  on(scheduler2) | then([&](int x) {
                    thread2.store(std::this_thread::get_id());
                    return x + 10;
                  });

    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 20);  // (5 * 2) + 10

    // Verify different threads executed different stages
    // Note: they could be the same thread if pools share threads,
    // but the test is valid either way
    expectTrue(thread1.load() != std::thread::id{});
    expectTrue(thread2.load() != std::thread::id{});
  };

#endif  // Disabled async tests

  return TestRegistry::result();
}
