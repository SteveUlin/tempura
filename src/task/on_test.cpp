#include "task/task.h"
#include "unit.h"

using namespace tempura;

// Test sender that checks if scheduler is available in environment
struct SchedulerCheckSender {
  using ValueTypes = std::tuple<bool>;
  using ErrorTypes = std::tuple<>;

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

  return TestRegistry::result();
}
