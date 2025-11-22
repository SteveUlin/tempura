#include "task/env.h"
#include "unit.h"

#include <system_error>

using namespace tempura;

// Test receivers defined at namespace scope (local classes can't have templates)

struct ReceiverWithEnv {
  InplaceStopSource* source;

  auto get_env() const noexcept { return withStopToken(source->get_token()); }

  void setValue() noexcept {}
  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {}
  void setStopped() noexcept {}
};

struct ReceiverWithoutEnv {
  void setValue() noexcept {}
  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {}
  void setStopped() noexcept {}
};

struct TestReceiver {
  InplaceStopSource* source;
  bool* stop_possible_result;

  auto get_env() const noexcept { return withStopToken(source->get_token()); }

  void setValue(bool value) noexcept { *stop_possible_result = value; }
  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {}
  void setStopped() noexcept {}
};

struct ReceiverWithScheduler {
  EventLoop* loop;
  EventLoopScheduler* retrieved;

  auto get_env() const noexcept {
    return withScheduler(EventLoopScheduler{*loop});
  }

  void setValue() noexcept {
    // Sender queries our environment
    auto env = tempura::get_env(*this);
    auto sched = tempura::get_scheduler(env);
    *retrieved = sched;
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {}
  void setStopped() noexcept {}
};

auto main() -> int {
  // ═══════════════════════════════════════════════════════════════════════════
  // EmptyEnv tests
  // ═══════════════════════════════════════════════════════════════════════════

  "EmptyEnv - provides NeverStopToken"_test = [] {
    EmptyEnv env;
    auto token = get_stop_token(env);
    static_assert(std::same_as<decltype(token), NeverStopToken>);
    expectFalse(token.stop_requested());
    expectFalse(token.stop_possible());
  };

  "EmptyEnv - provides InlineScheduler"_test = [] {
    EmptyEnv env;
    auto sched = get_scheduler(env);
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
  };

  "EmptyEnv - is constexpr"_test = [] {
    constexpr EmptyEnv env;
    constexpr auto token = get_stop_token(env);
    constexpr auto sched = get_scheduler(env);
    static_assert(!token.stop_requested());
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // withStopToken tests
  // ═══════════════════════════════════════════════════════════════════════════

  "withStopToken - stores and returns stop token"_test = [] {
    InplaceStopSource source;
    auto token = source.get_token();
    auto env = withStopToken(token);

    auto retrieved = get_stop_token(env);
    expectTrue(retrieved.stop_possible());
    expectFalse(retrieved.stop_requested());

    source.request_stop();
    expectTrue(retrieved.stop_requested());
  };

  "withStopToken - is constexpr with NeverStopToken"_test = [] {
    constexpr auto env = withStopToken(NeverStopToken{});
    constexpr auto token = get_stop_token(env);
    static_assert(!token.stop_requested());
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // withScheduler tests
  // ═══════════════════════════════════════════════════════════════════════════

  "withScheduler - stores and returns scheduler"_test = [] {
    InlineScheduler sched;
    auto env = withScheduler(sched);

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), InlineScheduler>);
  };

  "withScheduler - works with EventLoopScheduler"_test = [] {
    EventLoop loop;
    EventLoopScheduler sched{loop};
    auto env = withScheduler(sched);

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), EventLoopScheduler>);
  };

  "withScheduler - works with ThreadPoolScheduler"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler sched{pool};
    auto env = withScheduler(sched);

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), ThreadPoolScheduler>);
  };

  "withScheduler - is constexpr with InlineScheduler"_test = [] {
    constexpr auto env = withScheduler(InlineScheduler{});
    constexpr auto sched = get_scheduler(env);
    (void)sched;  // Suppress unused variable warning
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Composed environment tests (stop token + scheduler)
  // ═══════════════════════════════════════════════════════════════════════════

  "Composed - stores both token and scheduler"_test = [] {
    InplaceStopSource source;
    auto token = source.get_token();
    InlineScheduler sched;
    auto env = withScheduler(withStopToken(token), sched);

    auto retrieved_token = get_stop_token(env);
    auto retrieved_sched = get_scheduler(env);

    expectTrue(retrieved_token.stop_possible());
    expectFalse(retrieved_token.stop_requested());
    static_assert(std::same_as<decltype(retrieved_sched), InlineScheduler>);

    source.request_stop();
    expectTrue(retrieved_token.stop_requested());
  };

  "Composed - works with different schedulers"_test = [] {
    EventLoop loop;
    EventLoopScheduler sched{loop};
    InplaceStopSource source;
    auto token = source.get_token();

    auto env = withScheduler(withStopToken(token), sched);

    auto retrieved_sched = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved_sched), EventLoopScheduler>);
  };

  "Composed - is constexpr"_test = [] {
    constexpr auto env =
        withScheduler(withStopToken(NeverStopToken{}), InlineScheduler{});
    constexpr auto token = get_stop_token(env);
    constexpr auto sched = get_scheduler(env);
    static_assert(!token.stop_requested());
    (void)sched;  // Suppress unused variable warning
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // get_env CPO tests
  // ═══════════════════════════════════════════════════════════════════════════

  "get_env - returns receiver's environment"_test = [] {
    InplaceStopSource source;
    ReceiverWithEnv receiver{&source};

    auto env = get_env(receiver);
    auto token = get_stop_token(env);
    expectTrue(token.stop_possible());
  };

  "get_env - returns EmptyEnv for receivers without get_env"_test = [] {
    ReceiverWithoutEnv receiver;
    auto env = get_env(receiver);
    static_assert(std::same_as<decltype(env), EmptyEnv>);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // get_stop_token CPO tests
  // ═══════════════════════════════════════════════════════════════════════════

  "get_stop_token - queries environment for stop token"_test = [] {
    InplaceStopSource source;
    auto token = source.get_token();
    auto env = withStopToken(token);

    auto retrieved = get_stop_token(env);
    expectTrue(retrieved.stop_possible());
    expectFalse(retrieved.stop_requested());

    source.request_stop();
    expectTrue(retrieved.stop_requested());
  };

  "get_stop_token - returns NeverStopToken for EmptyEnv"_test = [] {
    EmptyEnv env;
    auto token = get_stop_token(env);
    static_assert(std::same_as<decltype(token), NeverStopToken>);
    expectFalse(token.stop_possible());
  };

  "get_stop_token - returns NeverStopToken for scheduler-only env"_test = [] {
    auto env = withScheduler(InlineScheduler{});
    auto token = get_stop_token(env);
    static_assert(std::same_as<decltype(token), NeverStopToken>);
    expectFalse(token.stop_possible());
  };

  "get_stop_token - works with combined environment"_test = [] {
    InplaceStopSource source;
    auto env = withScheduler(withStopToken(source.get_token()), InlineScheduler{});

    auto token = get_stop_token(env);
    expectTrue(token.stop_possible());
    expectFalse(token.stop_requested());

    source.request_stop();
    expectTrue(token.stop_requested());
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // get_scheduler CPO tests
  // ═══════════════════════════════════════════════════════════════════════════

  "get_scheduler - queries environment for scheduler"_test = [] {
    InlineScheduler sched;
    auto env = withScheduler(sched);

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), InlineScheduler>);
  };

  "get_scheduler - returns InlineScheduler for EmptyEnv"_test = [] {
    EmptyEnv env;
    auto sched = get_scheduler(env);
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
  };

  "get_scheduler - returns InlineScheduler for token-only env"_test = [] {
    auto env = withStopToken(NeverStopToken{});
    auto sched = get_scheduler(env);
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
  };

  "get_scheduler - works with combined environment"_test = [] {
    EventLoop loop;
    EventLoopScheduler sched{loop};
    auto env = withScheduler(withStopToken(NeverStopToken{}), sched);

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), EventLoopScheduler>);
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Advanced composition tests
  // ═══════════════════════════════════════════════════════════════════════════

  "Composition - withScheduler(withStopToken(...))"_test = [] {
    InplaceStopSource source;
    EventLoop loop;
    auto env = withScheduler(withStopToken(source.get_token()),
                             EventLoopScheduler{loop});

    auto token = get_stop_token(env);
    auto sched = get_scheduler(env);

    expectTrue(token.stop_possible());
    expectFalse(token.stop_requested());
    static_assert(std::same_as<decltype(sched), EventLoopScheduler>);

    source.request_stop();
    expectTrue(token.stop_requested());
  };

  "Composition - withStopToken(withScheduler(...))"_test = [] {
    InplaceStopSource source;
    ThreadPool pool{2};
    auto env = withStopToken(withScheduler(ThreadPoolScheduler{pool}),
                             source.get_token());

    auto token = get_stop_token(env);
    auto sched = get_scheduler(env);

    expectTrue(token.stop_possible());
    static_assert(std::same_as<decltype(sched), ThreadPoolScheduler>);
  };

  "Composition - order doesn't matter for queries"_test = [] {
    InplaceStopSource source;
    InlineScheduler sched;

    // Build in different orders
    auto env1 = withScheduler(withStopToken(source.get_token()), sched);
    auto env2 = withStopToken(withScheduler(sched), source.get_token());

    // Both should provide the same values
    auto token1 = get_stop_token(env1);
    auto token2 = get_stop_token(env2);
    auto sched1 = get_scheduler(env1);
    auto sched2 = get_scheduler(env2);

    expectTrue(token1.stop_possible());
    expectTrue(token2.stop_possible());
    static_assert(std::same_as<decltype(sched1), InlineScheduler>);
    static_assert(std::same_as<decltype(sched2), InlineScheduler>);
  };

  "Composition - EmptyEnv as base provides defaults"_test = [] {
    // Query EmptyEnv directly
    auto token = get_stop_token(EmptyEnv{});
    auto sched = get_scheduler(EmptyEnv{});

    static_assert(std::same_as<decltype(token), NeverStopToken>);
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
    expectFalse(token.stop_possible());
  };

  "Composition - override works (last wins)"_test = [] {
    InplaceStopSource source1;
    InplaceStopSource source2;

    // Add stop token twice - second one overrides
    auto env = withStopToken(withStopToken(source1.get_token()),
                             source2.get_token());

    auto token = get_stop_token(env);
    expectTrue(token.stop_possible());

    // Should get source2's token (the override)
    source1.request_stop();
    expectFalse(token.stop_requested());  // source2 not stopped

    source2.request_stop();
    expectTrue(token.stop_requested());  // source2 stopped
  };

  "Composition - is constexpr"_test = [] {
    constexpr auto env =
        withScheduler(withStopToken(NeverStopToken{}), InlineScheduler{});
    constexpr auto token = get_stop_token(env);
    constexpr auto sched = get_scheduler(env);

    static_assert(!token.stop_requested());
    // Check the expression type, not the variable type (constexpr vars are const)
    static_assert(std::same_as<decltype(get_scheduler(env)), InlineScheduler>);
    (void)sched;  // Suppress unused warning
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // Integration tests - environment propagation
  // ═══════════════════════════════════════════════════════════════════════════

  "Integration - receiver provides environment to sender"_test = [] {
    InplaceStopSource source;
    bool result = false;
    TestReceiver receiver{&source, &result};

    // Sender queries receiver's environment
    auto env = get_env(receiver);
    auto token = get_stop_token(env);
    receiver.setValue(token.stop_possible());

    expectTrue(result);
  };

  "Integration - scheduler query through environment chain"_test = [] {
    EventLoop loop;
    EventLoopScheduler result{loop};
    ReceiverWithScheduler receiver{&loop, &result};

    receiver.setValue();
    // If this compiles and runs, scheduler was propagated correctly
  };

  return TestRegistry::result();
}
