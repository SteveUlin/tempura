// Copyright (C) 2025 tempura project
//
// Tests for async task execution system

#include "task/task.h"

#include <optional>
#include <string>
#include <tuple>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Print Receiver"_test = [] -> void {
    JustSender{99}.connect(PrintReceiver<int>{}).start();
  };

  "Value Receiver"_test = [] -> void {
    std::optional<std::tuple<int, std::string>> value;

    JustSender{7, std::string{"test"}}
        .connect(ValueReceiver<int, std::string>{value})
        .start();

    if (!expectTrue(value.has_value())) return;
    expectEq(std::make_tuple(7, std::string{"test"}), *value);
  };

  "syncWait with single value"_test = [] -> void {
    auto result = syncWait(JustSender{42});

    if (!expectTrue(result.has_value())) return;
    expectEq(std::make_tuple(42), *result);
  };

  "syncWait with multiple values"_test = [] -> void {
    auto [num, str, pi] =
        syncWait(JustSender{100, std::string{"hello"}, 3.14}).value();

    expectEq(num, 100);
    expectEq(str, std::string{"hello"});
    expectEq(pi, 3.14);
  };

  "Receiver error channel"_test = [] -> void {
    std::optional<std::tuple<int>> value;
    ValueReceiver<int> recv{value};

    // Simulate error completion
    recv.set_error(std::make_error_code(std::errc::invalid_argument));

    // Optional should be empty after error
    expectFalse(value.has_value());
  };

  "Receiver stopped channel"_test = [] -> void {
    std::optional<std::tuple<int, std::string>> value;
    ValueReceiver<int, std::string> recv{value};

    // Simulate cancellation
    recv.set_stopped();

    // Optional should be empty after stopped
    expectFalse(value.has_value());
  };

  "Receiver channels are mutually exclusive"_test = [] -> void {
    // Test that set_value works, but then error clears it
    std::optional<std::tuple<int>> value;
    ValueReceiver<int> recv{value};

    recv.set_value(42);
    if (!expectTrue(value.has_value())) return;
    expectEq(std::make_tuple(42), *value);

    // Error should clear the value
    recv.set_error(std::make_error_code(std::errc::io_error));
    expectFalse(value.has_value());
  };

  return 0;
}
