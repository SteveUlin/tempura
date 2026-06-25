// Tests for repeat algorithms

#include "task/task.h"
#include "task/test_helpers.h"

#include "unit.h"

#include <atomic>
#include <latch>
#include <optional>
#include <thread>

using namespace tempura;

// Receiver that carries a stop token — lets us test repeatEffect cancellation.
template <typename Token>
class StopTokenReceiver {
 public:
  StopTokenReceiver(std::optional<bool>& stopped_out, std::latch& latch,
                    Token token)
      : stopped_out_{stopped_out}, latch_{latch}, token_{token} {}

  void setValue() noexcept {
    stopped_out_ = false;
    latch_.count_down();
  }

  template <typename... E>
  void setError(E&&...) noexcept {
    stopped_out_ = false;
    latch_.count_down();
  }

  void setStopped() noexcept {
    stopped_out_ = true;
    latch_.count_down();
  }

  auto get_env() const noexcept { return withStopToken(token_); }

 private:
  std::optional<bool>& stopped_out_;
  std::latch& latch_;
  Token token_;
};

auto main() -> int {
  "repeatN - zero iterations completes immediately"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(0);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 0);
  };

  "repeatN - single iteration"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(1);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 1);
  };

  "repeatN - multiple iterations"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(5);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 5);
  };

  "repeatN - accumulates values across iterations"_test = [] {
    int sum = 0;
    int iteration = 0;
    auto sender = just() | then([&] {
      ++iteration;
      sum += iteration;
      return sum;
    }) | repeatN(10);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(sum, 55);  // 1+2+3+...+10
  };

  "repeatN - free function form"_test = [] {
    int count = 0;
    auto effect = just() | then([&] { return ++count; });
    auto sender = repeatN(std::move(effect), 3);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 3);
  };

  "repeatEffectUntil - stops when predicate returns true"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) |
                  repeatEffectUntil([&] { return count >= 5; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 5);
  };

  "repeatEffectUntil - predicate checked after each iteration"_test = [] {
    int count = 0;
    bool should_stop = false;
    auto sender = just() | then([&] {
      ++count;
      if (count == 3) {
        should_stop = true;
      }
      return count;
    }) | repeatEffectUntil([&] { return should_stop; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 3);
  };

  "repeatEffectUntil - immediate stop if predicate starts true"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) |
                  repeatEffectUntil([] { return true; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 1);  // Runs once, then predicate checked
  };

  "repeatEffectUntil - free function form"_test = [] {
    int count = 0;
    auto effect = just() | then([&] { return ++count; });
    auto sender = repeatEffectUntil(std::move(effect), [&] { return count >= 2; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 2);
  };

  "repeatEffect - compiles with pipe syntax"_test = [] {
    [[maybe_unused]] auto sender = just() | then([] { return 0; }) | repeatEffect();
  };

  "repeatEffect - stop token terminates loop via stopped channel"_test = [] {
    // repeatEffect polls get_stop_token(get_env(receiver)) each iteration.
    // After kStop iterations the source requests stop; the next onValueComplete
    // must deliver setStopped() instead of reconnecting.
    constexpr int kStop = 5;
    int count = 0;

    InplaceStopSource source;
    auto token = source.get_token();

    auto effect = just() | then([&] {
      ++count;
      if (count >= kStop) source.request_stop();
      return count;
    });

    auto sender = repeatEffect(std::move(effect));

    std::optional<bool> stopped_out;
    std::latch latch{1};
    StopTokenReceiver recv{stopped_out, latch, token};

    auto op = std::move(sender).connect(std::move(recv));
    op.start();
    latch.wait();

    // Loop must have terminated via the stopped channel
    expectTrue(stopped_out.has_value());
    expectTrue(*stopped_out);
    // Must not have run unboundedly — at most kStop + 1 iterations
    expectTrue(count <= kStop + 1);
    expectTrue(count >= kStop);
  };

  "repeatN - runs on async scheduler, each iteration crosses thread boundary"_test = [] {
    // The reconnect-each-iteration path in RepeatNOperationState is only
    // reached when inner_op_->start() dispatches to a real thread.
    // This test verifies that each copy of the OnSender correctly schedules
    // work on the pool and that iteration counts are coherent across threads.
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};
    std::thread::id main_tid = std::this_thread::get_id();

    constexpr int kN = 10;
    std::atomic<int> count{0};
    std::atomic<bool> all_on_pool{true};

    auto effect = on(scheduler, just() | then([&] {
                       ++count;
                       if (std::this_thread::get_id() == main_tid) all_on_pool = false;
                       return 0;
                     }));

    auto result = syncWait(repeatN(std::move(effect), kN));

    expectTrue(result.has_value());
    expectEq(count.load(), kN);
    expectTrue(all_on_pool.load());
  };

  "repeatEffectUntil - runs on async scheduler, predicate checked on main thread"_test = [] {
    // repeatEffectUntil calls the predicate in onValueComplete, which runs
    // on the thread that called setStopped/setValue on the inner receiver —
    // the pool thread for an OnSender. The outer syncWait must still unblock.
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};

    constexpr int kTarget = 7;
    std::atomic<int> count{0};

    auto effect = on(scheduler, just() | then([&] { ++count; return 0; }));

    auto result = syncWait(repeatEffectUntil(std::move(effect),
                                              [&] { return count.load() >= kTarget; }));

    expectTrue(result.has_value());
    expectTrue(count.load() >= kTarget);
  };

  "repeatN - large iteration count"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(1000);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 1000);
  };

  "repeatN - with state mutation"_test = [] {
    std::vector<int> values;
    auto sender = just() | then([&] {
      values.push_back(static_cast<int>(values.size()));
      return values.size();
    }) | repeatN(5);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(values.size(), 5u);
    expectEq(values[0], 0);
    expectEq(values[4], 4);
  };

  "repeatEffectUntil - an effect error terminates the loop and propagates"_test = [] {
    // The predicate never stops the loop; only the error does — so this would
    // spin forever if errors weren't forwarded.
    auto r = syncWait(repeatEffectUntil(ErrorSenderTest{}, [] { return false; }));
    expectFalse(r.has_value());
  };

  "repeatN - an effect error terminates the loop and propagates"_test = [] {
    auto r = syncWait(repeatN(ErrorSenderTest{}, 5));
    expectFalse(r.has_value());
  };

  "repeatN - honors the stop token (terminates early via stopped channel)"_test = [] {
    constexpr int kStop = 3;
    constexpr std::size_t kN = 100;
    int count = 0;

    InplaceStopSource source;
    auto token = source.get_token();
    auto effect = just() | then([&] {
      ++count;
      if (count >= kStop) source.request_stop();
      return count;
    });

    std::optional<bool> stopped_out;
    std::latch latch{1};
    StopTokenReceiver recv{stopped_out, latch, token};
    auto op = repeatN(std::move(effect), kN).connect(std::move(recv));
    op.start();
    latch.wait();

    expectTrue(stopped_out.has_value());
    expectTrue(*stopped_out);         // ended via stopped channel, not after kN iterations
    expectTrue(count <= kStop + 1);
    expectTrue(count >= kStop);
  };

  return TestRegistry::result();
}
