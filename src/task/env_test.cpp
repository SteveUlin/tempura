#include "task/env.h"
#include "unit.h"

#include <system_error>

using namespace tempura;

// Test receivers defined at namespace scope (local classes can't have templates)

struct ReceiverWithEnv {
  InplaceStopSource* source;

  auto get_env() const noexcept {
    return EnvWithStopToken{source->get_token()};
  }

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

  auto get_env() const noexcept {
    return EnvWithStopToken{source->get_token()};
  }

  void setValue(bool value) noexcept { *stop_possible_result = value; }
  template <typename... ErrorArgs>
  void setError(ErrorArgs&&...) noexcept {}
  void setStopped() noexcept {}
};

struct ReceiverWithScheduler {
  EventLoop* loop;
  EventLoopScheduler* retrieved;

  auto get_env() const noexcept {
    return EnvWithScheduler{EventLoopScheduler{*loop}};
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
    auto token = env.get_stop_token();
    static_assert(std::same_as<decltype(token), NeverStopToken>);
    expectFalse(token.stop_requested());
    expectFalse(token.stop_possible());
  };

  "EmptyEnv - provides InlineScheduler"_test = [] {
    EmptyEnv env;
    auto sched = env.get_scheduler();
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
  };

  "EmptyEnv - is constexpr"_test = [] {
    constexpr EmptyEnv env;
    constexpr auto token = env.get_stop_token();
    constexpr auto sched = env.get_scheduler();
    static_assert(!token.stop_requested());
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // EnvWithStopToken tests
  // ═══════════════════════════════════════════════════════════════════════════

  "EnvWithStopToken - stores and returns stop token"_test = [] {
    InplaceStopSource source;
    auto token = source.get_token();
    EnvWithStopToken env{token};

    auto retrieved = env.get_stop_token();
    expectTrue(retrieved.stop_possible());
    expectFalse(retrieved.stop_requested());

    source.request_stop();
    expectTrue(retrieved.stop_requested());
  };

  "EnvWithStopToken - is constexpr with NeverStopToken"_test = [] {
    constexpr EnvWithStopToken env{NeverStopToken{}};
    constexpr auto token = env.get_stop_token();
    static_assert(!token.stop_requested());
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // EnvWithScheduler tests
  // ═══════════════════════════════════════════════════════════════════════════

  "EnvWithScheduler - stores and returns scheduler"_test = [] {
    InlineScheduler sched;
    EnvWithScheduler env{sched};

    auto retrieved = env.get_scheduler();
    static_assert(std::same_as<decltype(retrieved), InlineScheduler>);
  };

  "EnvWithScheduler - works with EventLoopScheduler"_test = [] {
    EventLoop loop;
    EventLoopScheduler sched{loop};
    EnvWithScheduler env{sched};

    auto retrieved = env.get_scheduler();
    static_assert(std::same_as<decltype(retrieved), EventLoopScheduler>);
  };

  "EnvWithScheduler - works with ThreadPoolScheduler"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler sched{pool};
    EnvWithScheduler env{sched};

    auto retrieved = env.get_scheduler();
    static_assert(std::same_as<decltype(retrieved), ThreadPoolScheduler>);
  };

  "EnvWithScheduler - is constexpr with InlineScheduler"_test = [] {
    constexpr EnvWithScheduler env{InlineScheduler{}};
    constexpr auto sched = env.get_scheduler();
    (void)sched;  // Suppress unused variable warning
  };

  // ═══════════════════════════════════════════════════════════════════════════
  // EnvWithStopTokenAndScheduler tests
  // ═══════════════════════════════════════════════════════════════════════════

  "EnvWithStopTokenAndScheduler - stores both token and scheduler"_test = [] {
    InplaceStopSource source;
    auto token = source.get_token();
    InlineScheduler sched;
    EnvWithStopTokenAndScheduler env{token, sched};

    auto retrieved_token = env.get_stop_token();
    auto retrieved_sched = env.get_scheduler();

    expectTrue(retrieved_token.stop_possible());
    expectFalse(retrieved_token.stop_requested());
    static_assert(std::same_as<decltype(retrieved_sched), InlineScheduler>);

    source.request_stop();
    expectTrue(retrieved_token.stop_requested());
  };

  "EnvWithStopTokenAndScheduler - works with different schedulers"_test = [] {
    EventLoop loop;
    EventLoopScheduler sched{loop};
    InplaceStopSource source;
    auto token = source.get_token();

    EnvWithStopTokenAndScheduler env{token, sched};

    auto retrieved_sched = env.get_scheduler();
    static_assert(std::same_as<decltype(retrieved_sched), EventLoopScheduler>);
  };

  "EnvWithStopTokenAndScheduler - is constexpr"_test = [] {
    constexpr EnvWithStopTokenAndScheduler env{NeverStopToken{},
                                               InlineScheduler{}};
    constexpr auto token = env.get_stop_token();
    constexpr auto sched = env.get_scheduler();
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
    auto token = env.get_stop_token();
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
    EnvWithStopToken env{token};

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

  "get_stop_token - returns NeverStopToken for EnvWithScheduler"_test = [] {
    EnvWithScheduler env{InlineScheduler{}};
    auto token = get_stop_token(env);
    static_assert(std::same_as<decltype(token), NeverStopToken>);
    expectFalse(token.stop_possible());
  };

  "get_stop_token - works with combined environment"_test = [] {
    InplaceStopSource source;
    EnvWithStopTokenAndScheduler env{source.get_token(), InlineScheduler{}};

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
    EnvWithScheduler env{sched};

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), InlineScheduler>);
  };

  "get_scheduler - returns InlineScheduler for EmptyEnv"_test = [] {
    EmptyEnv env;
    auto sched = get_scheduler(env);
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
  };

  "get_scheduler - returns InlineScheduler for EnvWithStopToken"_test = [] {
    EnvWithStopToken env{NeverStopToken{}};
    auto sched = get_scheduler(env);
    static_assert(std::same_as<decltype(sched), InlineScheduler>);
  };

  "get_scheduler - works with combined environment"_test = [] {
    EventLoop loop;
    EventLoopScheduler sched{loop};
    EnvWithStopTokenAndScheduler env{NeverStopToken{}, sched};

    auto retrieved = get_scheduler(env);
    static_assert(std::same_as<decltype(retrieved), EventLoopScheduler>);
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
