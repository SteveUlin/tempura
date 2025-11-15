// Copyright (C) 2025 tempura project
//
// Tests for async task execution system

#include "task/task.h"

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include "unit.h"

using namespace tempura;

// ============================================================================
// Custom Error Sender Types (for testing variadic error types)
// ============================================================================

// Sender with custom error types (string, int)
class CustomErrorSender1 {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<std::string, int>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(std::string{"error message"}, 404);
      }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Sender with different custom error types (double, string)
class CustomErrorSender2 {
 public:
  using ValueTypes = std::tuple<int>;
  using ErrorTypes = std::tuple<double, std::string>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_(std::move(r)) {}
      void start() noexcept {
        receiver_.setError(3.14, std::string{"pi error"});
      }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

// Multi-error type sender (for static testing)
class MultiErrorSender {
 public:
  using ValueTypes = std::tuple<>;
  using ErrorTypes = std::tuple<int, double, std::string>;

  template <typename R>
  auto connect(R&&) && {
    struct OpState { void start() noexcept {} };
    return OpState{};
  }
};

// ============================================================================
// Concept Tests
// ============================================================================

// Verify that valid senders satisfy the Sender concept
static_assert(Sender<JustSender<int>>);
static_assert(Sender<decltype(just(42))>);
static_assert(Sender<decltype(just(42) | then([](int x) { return x * 2; }))>);

// Verify that SenderTo works correctly
static_assert(SenderTo<JustSender<int>, ValueReceiver<int>>);
static_assert(SenderTo<decltype(just(42)), PrintReceiver<int>>);

// Types without ValueTypes should not satisfy Sender
struct NotASender {
  // Missing: using ValueTypes = ...;
};
static_assert(!Sender<NotASender>);

// ============================================================================
// Helper Types
// ============================================================================

struct MoveOnly {
  explicit MoveOnly(int v) : value(v) {}
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) = default;
  int value;
};

auto main() -> int {
  // ==========================================================================
  // JustSender / syncWait
  // ==========================================================================

  "JustSender - single value"_test = [] {
    auto result = syncWait(JustSender{42});
    if (!expectTrue(result.has_value())) return;
    expectEq(std::make_tuple(42), *result);
  };

  "just() helper - single value"_test = [] {
    auto result = syncWait(just(42));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::make_tuple(42), *result);
  };

  "just() helper - multiple values"_test = [] {
    auto [num, str, pi] = syncWait(just(100, std::string{"hello"}, 3.14)).value();
    expectEq(num, 100);
    expectEq(str, std::string{"hello"});
    expectEq(pi, 3.14);
  };

  "JustSender - multiple values"_test = [] {
    auto [num, str, pi] =
        syncWait(JustSender{100, std::string{"hello"}, 3.14}).value();
    expectEq(num, 100);
    expectEq(str, std::string{"hello"});
    expectEq(pi, 3.14);
  };

  "JustSender - move-only types"_test = [] {
    auto result = syncWait(JustSender{MoveOnly{99}});
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result).value, 99);
  };

  // ==========================================================================
  // Receivers
  // ==========================================================================

  "ValueReceiver - basic usage"_test = [] {
    std::optional<std::tuple<int>> value;
    JustSender{42}.connect(ValueReceiver<int>{value}).start();
    if (!expectTrue(value.has_value())) return;
    expectEq(std::get<0>(*value), 42);
  };

  "ValueReceiver - error channel"_test = [] {
    std::optional<std::tuple<int>> value;
    ValueReceiver<int> recv{value};

    recv.setValue(42);
    expectTrue(value.has_value());

    recv.setError(std::make_error_code(std::errc::invalid_argument));
    expectFalse(value.has_value());
  };

  "PrintReceiver - compiles and runs"_test = [] {
    JustSender{99}.connect(PrintReceiver<int>{}).start();
  };

  // ==========================================================================
  // then / Pipe Operators
  // ==========================================================================

  "then - basic transformation"_test = [] {
    auto sender = then(just(21), [](int x) { return x * 2; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 42);
    }
  };

  "then - chained"_test = [] {
    auto sender = then(then(just(10), [](int x) { return x + 5; }),
                       [](int x) { return x * 2; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 30);  // (10 + 5) * 2
    }
  };

  "Pipe operator - chained transformations"_test = [] {
    auto sender = just(2) | then([](int x) { return x + 3; }) |
                  then([](int x) { return x * 4; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 20);  // (2 + 3) * 4
    }
  };

  // ==========================================================================
  // letValue - nested async operations
  // ==========================================================================

  "letValue - basic nested sender"_test = [] {
    auto sender = letValue(just(21), [](int x) { return just(x * 2); });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 42);
    }
  };

  "letValue - chained nested operations"_test = [] {
    auto sender = letValue(just(10), [](int x) {
      return letValue(just(x + 5), [](int y) { return just(y * 2); });
    });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 30);  // (10 + 5) * 2
    }
  };

  "letValue - pipe operator"_test = [] {
    auto sender = just(3) | letValue([](int x) { return just(x * 10); }) |
                  then([](int x) { return x + 7; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 37);  // (3 * 10) + 7
    }
  };

  "letValue - mixing with then"_test = [] {
    auto sender = just(2) | then([](int x) { return x + 1; }) |
                  letValue([](int x) { return just(x * 10); }) |
                  then([](int x) { return x - 5; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 25);  // ((2 + 1) * 10) - 5 = 25
    }
  };

  "letValue - multiple values"_test = [] {
    auto sender = just(5, 10) | letValue([](int a, int b) {
                    return just(a + b, a * b);
                  });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      auto [sum, product] = *result;
      expectEq(sum, 15);
      expectEq(product, 50);
    }
  };

  // ==========================================================================
  // letError - nested error recovery
  // ==========================================================================

  "letError - error recovery with sender"_test = [] {
    // Create a sender that will produce an error (we'll use a helper)
    auto error_sender = just(0) | then([](int) -> int {
                          // In real code, this might fail - for testing,
                          // we'll manually test with uponError
                          return 42;
                        });

    // Test the type deduction works (empty error types by default)
    auto sender = error_sender | letError([]() {
                    return just(999);  // Recovery value
                  });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 42);  // No error occurred
    }
  };

  "letError - chained error recovery"_test = [] {
    auto sender =
        just(42) |
        letError([]() { return just(100); }) |
        letError([]() { return just(200); });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 42);  // Original value, no errors
    }
  };

  "letError - mixing with then and letValue"_test = [] {
    auto sender = just(10) | then([](int x) { return x * 2; }) |
                  letError([]() { return just(999); }) |
                  letValue([](int x) { return just(x + 5); });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5 = 25
    }
  };

  // ==========================================================================
  // Concept validation for new senders
  // ==========================================================================

  "letValue - sender concept"_test = [] {
    auto sender = just(42) | letValue([](int x) { return just(x * 2); });
    static_assert(Sender<decltype(sender)>);
    expectTrue(true);  // If it compiles, we're good
  };

  "letError - sender concept"_test = [] {
    auto sender = just(42) | letError([]() { return just(0); });
    static_assert(Sender<decltype(sender)>);
    expectTrue(true);  // If it compiles, we're good
  };

  // ==========================================================================
  // Variadic Error Types - P2300 Symmetry
  // ==========================================================================

  "ErrorTypes - custom error sender"_test = [] {
    static_assert(Sender<CustomErrorSender1>);
    static_assert(Sender<CustomErrorSender2>);
    static_assert(Sender<MultiErrorSender>);
    expectTrue(true);
  };

  "letError - variadic error types"_test = [] {
    // letError should unpack the error types using std::apply
    auto sender = CustomErrorSender1{} |
                  letError([](std::string msg, int code) {
                    // Verify we received both error arguments
                    expectEq(msg, std::string{"error message"});
                    expectEq(code, 404);
                    return just(999);  // Recovery value
                  });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 999);
    }
  };

  "uponError - variadic error types"_test = [] {
    // uponError should unpack error types and convert to value
    auto sender = CustomErrorSender2{} |
                  uponError([](double val, std::string msg) {
                    // Verify we received both error arguments
                    expectEq(val, 3.14);
                    expectEq(msg, std::string{"pi error"});
                    return 42;  // Convert error to value
                  });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 42);
    }
  };

  "ErrorTypes - symmetry with ValueTypes"_test = [] {
    // Verify error types work just like value types
    using ValueSender = decltype(just(1, 2, 3));
    static_assert(std::same_as<
        typename ValueSender::ValueTypes,
        std::tuple<int, int, int>>);

    // Custom error sender with multiple error types
    static_assert(std::same_as<
        typename MultiErrorSender::ErrorTypes,
        std::tuple<int, double, std::string>>);

    expectTrue(true);
  };

  return 0;
}
