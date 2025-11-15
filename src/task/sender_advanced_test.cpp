// Tests for advanced sender operations: letValue, letError, uponError

#include "task/task.h"
#include "task/test_helpers.h"

#include <string>
#include <tuple>
#include <utility>

#include "unit.h"

using namespace tempura;

auto main() -> int {
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

  // ==========================================================================
  // uponError - error transformation
  // ==========================================================================

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

  return 0;
}
