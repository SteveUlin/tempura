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

  "JustSender - multiple values"_test = [] {
    auto [num, str, pi] = syncWait(JustSender{100, std::string{"hello"}, 3.14}).value();
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

    recv.SetValue(42);
    expectTrue(value.has_value());

    recv.SetError(std::make_error_code(std::errc::invalid_argument));
    expectFalse(value.has_value());
  };

  "PrintReceiver - compiles and runs"_test = [] {
    JustSender{99}.connect(PrintReceiver<int>{}).start();
  };

  // ==========================================================================
  // Then / Pipe Operators
  // ==========================================================================

  "Then - basic transformation"_test = [] {
    auto sender = Then(JustSender{21}, [](int x) { return x * 2; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 42);
    }
  };

  "Then - chained"_test = [] {
    auto sender = Then(Then(JustSender{10}, [](int x) { return x + 5; }),
                       [](int x) { return x * 2; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 30);  // (10 + 5) * 2
    }
  };

  "Pipe operator - chained transformations"_test = [] {
    auto sender = JustSender{2} |
                  Then([](int x) { return x + 3; }) |
                  Then([](int x) { return x * 4; });
    auto result = syncWait(std::move(sender));
    if (expectTrue(result.has_value())) {
      expectEq(std::get<0>(*result), 20);  // (2 + 3) * 4
    }
  };

  return 0;
}
