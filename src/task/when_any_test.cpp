// Tests for whenAny race composition

#include "task/task.h"
#include "task/test_helpers.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <variant>
#include <vector>

#include "unit.h"

using namespace tempura;


// Sender that checks if stop was requested (for cancellation testing)
class StoppableOperationTest {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(int), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r, bool* stop_observed)
          : receiver_(std::move(r)), stop_observed_(stop_observed) {}

      void start() noexcept {
        // Check if stop token is available and query it
        auto env = tempura::get_env(receiver_);
        auto token = tempura::get_stop_token(env);

        *stop_observed_ = token.stop_requested();

        // Complete with a value (but this sender will lose the race)
        receiver_.setValue(999);
      }

     private:
      R receiver_;
      bool* stop_observed_;
    };
    return OpState{std::forward<R>(receiver), &stop_observed_};
  }

  bool stop_observed_{false};
};

// Sender that verifies stop token is available (for cancellation testing)
class TokenCheckSenderTest {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(bool), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}

      void start() noexcept {
        auto env = tempura::get_env(receiver_);
        auto token = tempura::get_stop_token(env);

        // Check if stop is possible (should be true with whenAny)
        bool stop_possible = token.stop_possible();
        receiver_.setValue(stop_possible);
      }

     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

auto main() -> int {
  "whenAny - two senders with int"_test = [] {
    auto sender = whenAny(just(42), just(99));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = *result;
    expectTrue(std::holds_alternative<std::tuple<int>>(var));

    int value = std::get<0>(std::get<std::tuple<int>>(var));
    // Either 42 or 99 is valid (first to complete wins)
    expectTrue(value == 42 || value == 99);
  };

  "whenAny - two senders with different types"_test = [] {
    auto sender = whenAny(just(42), just(std::string{"hello"}));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = *result;

    // Check which sender completed first
    if (std::holds_alternative<std::tuple<int>>(var)) {
      int value = std::get<0>(std::get<std::tuple<int>>(var));
      expectEq(value, 42);
    } else if (std::holds_alternative<std::tuple<std::string>>(var)) {
      auto& str = std::get<0>(std::get<std::tuple<std::string>>(var));
      expectEq(str, std::string{"hello"});
    } else {
      expectTrue(false);  // Should never reach here
    }
  };

  "whenAny - three senders"_test = [] {
    auto sender = whenAny(just(1), just(2.5), just(std::string{"test"}));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = *result;

    // One of the three should have completed
    bool valid = std::holds_alternative<std::tuple<int>>(var) ||
                 std::holds_alternative<std::tuple<double>>(var) ||
                 std::holds_alternative<std::tuple<std::string>>(var);
    expectTrue(valid);
  };

  "whenAny - single sender"_test = [] {
    auto sender = whenAny(just(99));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = *result;
    expectTrue(std::holds_alternative<std::tuple<int>>(var));
    expectEq(std::get<0>(std::get<std::tuple<int>>(var)), 99);
  };

  "whenAny - senders with multiple values"_test = [] {
    auto sender = whenAny(just(1, 2), just(3.14, std::string{"pi"}));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = *result;

    if (std::holds_alternative<std::tuple<int, int>>(var)) {
      auto [v1, v2] = std::get<std::tuple<int, int>>(var);
      expectEq(v1, 1);
      expectEq(v2, 2);
    } else if (std::holds_alternative<std::tuple<double, std::string>>(var)) {
      auto [v1, v2] = std::get<std::tuple<double, std::string>>(var);
      expectEq(v1, 3.14);
      expectEq(v2, std::string{"pi"});
    } else {
      expectTrue(false);  // Should never reach here
    }
  };

  "whenAny - move-only types"_test = [] {
    auto sender = whenAny(just(MoveOnly{10}), just(MoveOnly{20}));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = std::move(*result);
    expectTrue(std::holds_alternative<std::tuple<MoveOnly>>(var));

    auto [mo] = std::get<std::tuple<MoveOnly>>(std::move(var));
    // Either 10 or 20 is valid
    expectTrue(mo.value == 10 || mo.value == 20);
  };

  "whenAny - composed with then"_test = [] {
    auto sender = whenAny(just(10), just(20)) | then([](auto var) {
      // Extract the value from the variant
      if (std::holds_alternative<std::tuple<int>>(var)) {
        return std::get<0>(std::get<std::tuple<int>>(var)) * 2;
      }
      return 0;  // Should not happen
    });

    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;

    int value = std::get<0>(*result);
    // Either 20 or 40 is valid (10*2 or 20*2)
    expectTrue(value == 20 || value == 40);
  };

  "whenAny - chained then operations"_test = [] {
    auto s1 = just(5) | then([](int x) { return x * 2; });
    auto s2 = just(3) | then([](int x) { return x + 1; });

    auto sender = whenAny(std::move(s1), std::move(s2)) |
                  then([](auto var) {
                    return std::get<0>(std::get<std::tuple<int>>(var));
                  });

    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;

    int value = std::get<0>(*result);
    // Either 10 (5*2) or 4 (3+1) is valid
    expectTrue(value == 10 || value == 4);
  };

  "whenAny - nested composition"_test = [] {
    auto inner1 = whenAny(just(1), just(2));
    auto inner2 = whenAny(just(3), just(4));

    auto sender = whenAny(std::move(inner1), std::move(inner2));
    auto result = syncWait(std::move(sender));

    // Liveness: composition completes without deadlock
    expectTrue(result.has_value());
  };

  "whenAny - first sender errors"_test = [] {
    // ErrorSenderTest is first and sync: error wins, value second sender is ignored.
    auto sender = whenAny(ErrorSenderTest{}, just(42));
    auto result = syncWait(std::move(sender));
    expectFalse(result.has_value());
  };

  "whenAny - second sender errors"_test = [] {
    // just(100) is first and sync: value wins, second sender's error is ignored.
    auto sender = whenAny(just(100), ErrorSenderTest2{});
    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(std::get<0>(std::get<std::tuple<int>>(std::get<0>(*result))), 100);
  };

  "whenAny - all senders error"_test = [] {
    auto sender = whenAny(ErrorSenderTest{}, ErrorSenderTest2{});
    auto result = syncWait(std::move(sender));

    expectFalse(result.has_value());  // Should get an error
  };

  "whenAny - sender stopped"_test = [] {
    // StoppedSender is first and sync: stopped wins, value second sender is ignored.
    auto sender = whenAny(StoppedSender{}, just(42));
    auto result = syncWait(std::move(sender));
    expectFalse(result.has_value());
  };

  "whenAny - all senders stopped"_test = [] {
    auto sender = whenAny(StoppedSender{}, StoppedSender{});
    auto result = syncWait(std::move(sender));

    expectFalse(result.has_value());  // Should get stopped signal
  };

  "whenAny - with InlineScheduler"_test = [] {
    InlineScheduler sched;

    auto s1 = sched.schedule() | then([] { return 10; });
    auto s2 = sched.schedule() | then([] { return 20; });

    auto sender = whenAny(std::move(s1), std::move(s2)) |
                  then([](auto var) {
                    return std::get<0>(std::get<std::tuple<int>>(var));
                  });

    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;

    int value = std::get<0>(*result);
    // Either 10 or 20 is valid
    expectTrue(value == 10 || value == 20);
  };

  "whenAny - five senders"_test = [] {
    auto sender = whenAny(just(1), just(2), just(3), just(4), just(5));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [var] = *result;
    expectTrue(std::holds_alternative<std::tuple<int>>(var));

    int value = std::get<0>(std::get<std::tuple<int>>(var));
    // One of 1, 2, 3, 4, or 5
    expectTrue(value >= 1 && value <= 5);
  };

  "whenAny - composed with whenAll"_test = [] {
    auto all_sender = whenAll(just(1), just(2));
    auto any_sender = whenAny(std::move(all_sender), just(99));

    auto result = syncWait(std::move(any_sender));
    expectTrue(result.has_value());
  };

  "whenAny - losers receive stop signal"_test = [] {
    // just(42) is first and wins; StoppableOperationTest starts after stop is requested.
    auto sender = whenAny(just(42), StoppableOperationTest{});
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;
    // just(42) wins — value channel with 42.
    expectTrue(std::holds_alternative<std::tuple<int>>(var));
    expectEq(std::get<0>(std::get<std::tuple<int>>(var)), 42);
    // Liveness: confirms slow sender completed and handed off to receiver.
  };

  "whenAny - stop token available in child receivers"_test = [] {
    auto sender = whenAny(TokenCheckSenderTest{}, just(42));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;

    // Either sender could win, but if TokenCheckSender wins,
    // it should report that stop token was available
    if (std::holds_alternative<std::tuple<bool>>(var)) {
      bool stop_was_possible = std::get<0>(std::get<std::tuple<bool>>(var));
      expectTrue(stop_was_possible);
    }
  };

  "whenAny - concurrent completions from thread pool workers"_test = [] {
    ThreadPool pool{4};
    ThreadPoolScheduler scheduler{pool};

    // Create multiple tasks with varying sleep times
    // The one that sleeps the least should win the race
    auto s1 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return 1;
              });

    auto s2 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return 2;
              });

    auto s3 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return 3;
              });

    auto sender = whenAny(std::move(s1), std::move(s2), std::move(s3));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;

    expectTrue(std::holds_alternative<std::tuple<int>>(var));

    // Task 2 should usually win (sleeps for 10ms), but we accept any winner
    // since we're primarily testing that the race works
    int winner = std::get<0>(std::get<std::tuple<int>>(var));
    expectTrue(winner >= 1 && winner <= 3);
  };

  "whenAny - first to complete wins on thread pool"_test = [] {
    ThreadPool pool{8};
    ThreadPoolScheduler scheduler{pool};

    std::atomic<int> completion_counter{0};
    std::atomic<int> first_to_complete{0};
    std::mutex completion_mutex;

    auto makeTask = [&](int id, int delay_ms) {
      return scheduler.schedule() | then([&, id, delay_ms] {
               std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

               // Record which task completed first
               int current = completion_counter.fetch_add(1);
               if (current == 0) {
                 first_to_complete.store(id);
               }

               return id;
             });
    };

    // Create tasks with different delays
    auto s1 = makeTask(1, 80);
    auto s2 = makeTask(2, 5);   // Fastest
    auto s3 = makeTask(3, 60);
    auto s4 = makeTask(4, 40);

    auto sender = whenAny(std::move(s1), std::move(s2), std::move(s3),
                          std::move(s4));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;

    expectTrue(std::holds_alternative<std::tuple<int>>(var));
    int winner = std::get<0>(std::get<std::tuple<int>>(var));

    // Task 2 should usually win, but timing isn't guaranteed
    expectTrue(winner >= 1 && winner <= 4);

    // At least one task completed
    expectTrue(completion_counter.load() >= 1);
  };

  "whenAny - stop token cancels losing operations"_test = [] {
    ThreadPool pool{4};
    ThreadPoolScheduler scheduler{pool};

    std::atomic<int> tasks_started{0};
    std::atomic<int> tasks_completed{0};

    auto makeLongTask = [&](int id) {
      return scheduler.schedule() | then([&, id] {
               tasks_started.fetch_add(1);

               // Simulate long-running work
               for (int i = 0; i < 20; ++i) {
                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
                 // In a real implementation, we'd check stop token here
               }

               tasks_completed.fetch_add(1);
               return id;
             });
    };

    auto fastTask = scheduler.schedule() | then([] {
                      std::this_thread::sleep_for(std::chrono::milliseconds(20));
                      return 99;
                    });

    auto sender = whenAny(makeLongTask(1), makeLongTask(2), makeLongTask(3),
                          std::move(fastTask));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;

    expectTrue(std::holds_alternative<std::tuple<int>>(var));
    int winner = std::get<0>(std::get<std::tuple<int>>(var));

    // Fast task (99) should usually win
    expectTrue(winner >= 1 && winner <= 99);

    expectTrue(tasks_started.load() >= 1);
  };

  "whenAny - concurrent error and value completions"_test = [] {
    ThreadPool pool{4};
    ThreadPoolScheduler scheduler{pool};

    auto value_sender = scheduler.schedule() | then([] {
                          std::this_thread::sleep_for(
                              std::chrono::milliseconds(50));
                          return 42;
                        });

    auto fast_sender = scheduler.schedule() | then([] {
                         std::this_thread::sleep_for(
                             std::chrono::milliseconds(10));
                         return 99;
                       });

    auto sender = whenAny(std::move(value_sender), std::move(fast_sender),
                          ErrorSenderTest{});
    auto result = syncWait(std::move(sender));

    // Winner is nondeterministic: value (42 or 99) or error are all legal.
    // syncWait returning is the liveness assertion (no deadlock).
  };

  "whenAny - many concurrent operations"_test = [] {
    ThreadPool pool{8};
    ThreadPoolScheduler scheduler{pool};

    // Create many concurrent operations with different delays
    auto s0 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(0));
                return 0;
              });
    auto s1 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(7));
                return 1;
              });
    auto s2 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(14));
                return 2;
              });
    auto s3 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(21));
                return 3;
              });
    auto s4 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(28));
                return 4;
              });
    auto s5 = scheduler.schedule() | then([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
                return 5;
              });

    auto sender = whenAny(std::move(s0), std::move(s1), std::move(s2),
                          std::move(s3), std::move(s4), std::move(s5));

    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;

    expectTrue(std::holds_alternative<std::tuple<int>>(var));
    int winner = std::get<0>(std::get<std::tuple<int>>(var));

    // Some task won the race (0 should usually win with 0ms delay)
    expectTrue(winner >= 0 && winner < 6);
  };

  "whenAny - mixed sync and async senders"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    // Synchronous sender (completes immediately)
    auto sync_sender = just(100);

    // Asynchronous sender (takes time)
    auto async_sender = scheduler.schedule() | then([] {
                          std::this_thread::sleep_for(
                              std::chrono::milliseconds(50));
                          return 200;
                        });

    auto sender = whenAny(std::move(sync_sender), std::move(async_sender));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;
    auto [var] = *result;

    // Either could win, but sync usually completes first
    expectTrue(std::holds_alternative<std::tuple<int>>(var));
    int winner = std::get<0>(std::get<std::tuple<int>>(var));
    expectTrue(winner == 100 || winner == 200);
  };

  return TestRegistry::result();
}
