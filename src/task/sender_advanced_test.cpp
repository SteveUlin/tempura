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
    // Use a sender that actually has error signatures
    auto sender = CustomErrorSender1{} | letError([](std::tuple<std::string, int> err) {
                    return just(999);  // Recovery value
                  });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 999);  // Error was recovered
    }
  };

  "letError - chained error recovery"_test = [] {
    // Chain letError on a sender with error signatures
    auto sender = CustomErrorSender1{} |
                  letError([](std::tuple<std::string, int> err) {
                    return CustomErrorSender2{};  // Returns another error sender
                  }) |
                  letError([](std::tuple<double, std::string> err) {
                    return just(200);  // Final recovery
                  });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 200);  // Recovered from second error
    }
  };

  "letError - mixing with then and letValue"_test = [] {
    // Start with error sender, recover, then process with then and letValue
    auto sender = CustomErrorSender1{} |
                  letError([](std::tuple<std::string, int> err) { return just(10); }) |
                  then([](int x) { return x * 2; }) |
                  letValue([](int x) { return just(x + 5); });

    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 25);  // (10 * 2) + 5 = 25
    }
  };

  "letError - variadic error types"_test = [] {
    // letError should unpack the error tuple using std::apply
    auto sender = CustomErrorSender1{} |
                  letError([](std::tuple<std::string, int> err) {
                    // Verify we received both error arguments in the tuple
                    auto [msg, code] = err;
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
    // uponError should unpack error tuple and convert to value
    auto sender = CustomErrorSender2{} |
                  uponError([](std::tuple<double, std::string> err) {
                    // Verify we received both error arguments in the tuple
                    auto [val, msg] = err;
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
