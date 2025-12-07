// Tests for let_stopped and upon_stopped sender operations

#include "task/task.h"
#include "task/test_helpers.h"

#include <optional>
#include <string>
#include <system_error>
#include <tuple>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // ==========================================================================
  // uponStopped
  // ==========================================================================

  "uponStopped - converts stopped to value"_test = [] {
    auto sender = StoppedSender{} | uponStopped([] { return 42; });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 42);
  };

  "uponStopped - passes through values"_test = [] {
    auto sender = just(100) | uponStopped([] { return -1; });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 100);
  };

  "uponStopped - chained with then"_test = [] {
    auto sender = StoppedSender{}
                  | uponStopped([] { return 10; })
                  | then([](int x) { return x * 2; });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 20);
  };

  "uponStopped - two-argument form"_test = [] {
    auto sender = uponStopped(StoppedSender{}, [] { return 99; });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 99);
  };

  // ==========================================================================
  // letStopped
  // ==========================================================================

  "letStopped - converts stopped to sender"_test = [] {
    auto sender = StoppedSender{} | letStopped([] { return just(42); });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 42);
  };

  "letStopped - passes through values"_test = [] {
    auto sender = just(100) | letStopped([] { return just(-1); });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 100);
  };

  "letStopped - chain inner sender computation"_test = [] {
    auto sender = StoppedSender{} | letStopped([] {
      return just(5) | then([](int x) { return x * 10; });
    });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 50);
  };

  "letStopped - two-argument form"_test = [] {
    auto sender = letStopped(StoppedSender{}, [] { return just(77); });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 77);
  };

  "letStopped - chained with then"_test = [] {
    auto sender = StoppedSender{}
                  | letStopped([] { return just(10); })
                  | then([](int x) { return x + 5; });
    auto result = syncWait(std::move(sender));
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 15);
  };

  return TestRegistry::result();
}
