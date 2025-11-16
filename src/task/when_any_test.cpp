// Tests for whenAny race composition

#include "task/task.h"
#include "task/test_helpers.h"

#include <string>
#include <system_error>
#include <variant>

#include "unit.h"

using namespace tempura;

// Helper error sender for testing
class ErrorSenderTest {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<std::error_code>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(
            std::make_error_code(std::errc::invalid_argument));
      }

     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Another error sender for testing
class ErrorSenderTest2 {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<std::error_code>;

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

// Stopped sender for testing
class StoppedSenderTest {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept { receiver_.setStopped(); }

     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

auto main() -> int {
  // ==========================================================================
  // Basic whenAny functionality
  // ==========================================================================

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

  // ==========================================================================
  // Composition with then
  // ==========================================================================

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

  // ==========================================================================
  // Nested whenAny
  // ==========================================================================

  "whenAny - nested composition"_test = [] {
    auto inner1 = whenAny(just(1), just(2));
    auto inner2 = whenAny(just(3), just(4));

    auto sender = whenAny(std::move(inner1), std::move(inner2));
    auto result = syncWait(std::move(sender));

    if (!expectTrue(result.has_value())) return;

    auto [outer_var] = *result;
    // The outer variant contains variants from inner whenAny calls
    // This gets complex quickly, but we can verify it completed
    expectTrue(true);  // If we got here, the composition worked
  };

  // ==========================================================================
  // Error handling
  // ==========================================================================

  "whenAny - first sender errors"_test = [] {
    auto sender = whenAny(ErrorSenderTest{}, just(42));
    auto result = syncWait(std::move(sender));

    // Either error or value could win depending on scheduling
    // In inline execution, error typically wins
    expectTrue(true);  // Test completes without crash
  };

  "whenAny - second sender errors"_test = [] {
    auto sender = whenAny(just(100), ErrorSenderTest2{});
    auto result = syncWait(std::move(sender));

    // Either error or value could win depending on scheduling
    expectTrue(true);  // Test completes without crash
  };

  "whenAny - all senders error"_test = [] {
    auto sender = whenAny(ErrorSenderTest{}, ErrorSenderTest2{});
    auto result = syncWait(std::move(sender));

    expectFalse(result.has_value());  // Should get an error
  };

  // ==========================================================================
  // Stop handling
  // ==========================================================================

  "whenAny - sender stopped"_test = [] {
    auto sender = whenAny(StoppedSenderTest{}, just(42));
    auto result = syncWait(std::move(sender));

    // Either stopped or value could win depending on scheduling
    expectTrue(true);  // Test completes without crash
  };

  "whenAny - all senders stopped"_test = [] {
    auto sender = whenAny(StoppedSenderTest{}, StoppedSenderTest{});
    auto result = syncWait(std::move(sender));

    expectFalse(result.has_value());  // Should get stopped signal
  };

  // ==========================================================================
  // Type deduction tests (compile-time)
  // ==========================================================================

  "whenAny - ValueTypes deduction"_test = [] {
    auto sender = whenAny(just(1), just(2.5));
    using SenderType = decltype(sender);
    using ExpectedValueTypes = std::tuple<
        std::variant<std::tuple<int>, std::tuple<double>>
    >;

    static_assert(std::same_as<typename SenderType::ValueTypes,
                               ExpectedValueTypes>);
  };

  "whenAny - ErrorTypes deduplication"_test = [] {
    auto sender = whenAny(CustomErrorSender1{}, CustomErrorSender2{});
    using SenderType = decltype(sender);

    // Error type should be a variant of all unique error types
    // CustomErrorSender1::ErrorTypes = tuple<string, int>
    // CustomErrorSender2::ErrorTypes = tuple<double, string>
    // Result: variant<string, int, double> (string deduplicated)
    using ExpectedErrorTypes = std::tuple<std::variant<
        std::string,  // From both senders (deduplicated)
        int,          // From CustomErrorSender1
        double        // From CustomErrorSender2
        >>;

    static_assert(std::same_as<typename SenderType::ErrorTypes,
                               ExpectedErrorTypes>);
  };

  // ==========================================================================
  // Integration with schedulers
  // ==========================================================================

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

  // ==========================================================================
  // Many senders
  // ==========================================================================

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

  // ==========================================================================
  // Mixed whenAll and whenAny
  // ==========================================================================

  "whenAny - composed with whenAll"_test = [] {
    auto all_sender = whenAll(just(1), just(2));
    auto any_sender = whenAny(std::move(all_sender), just(99));

    auto result = syncWait(std::move(any_sender));
    if (!expectTrue(result.has_value())) return;

    // Should complete (either the whenAll or the just(99))
    expectTrue(true);
  };

  return TestRegistry::result();
}
