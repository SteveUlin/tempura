// Tests for split sender - multi-shot sender caching

#include "task/task.h"
#include "task/test_helpers.h"

#include <optional>
#include <string>
#include <tuple>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "split - basic single value"_test = [] {
    auto shared = split(just(42));
    auto result = syncWait(shared);
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 42);
  };

  "split - multiple values"_test = [] {
    auto shared = split(just(1, 2.5, std::string{"hello"}));
    auto result = syncWait(shared);
    if (!expectTrue(result.has_value())) return;
    auto [a, b, c] = *result;
    expectEq(a, 1);
    expectEq(b, 2.5);
    expectEq(c, std::string{"hello"});
  };

  "split - can be consumed multiple times"_test = [] {
    auto shared = split(just(100));

    // First consumption
    auto result1 = syncWait(shared);
    if (!expectTrue(result1.has_value())) return;
    expectEq(std::get<0>(*result1), 100);

    // Second consumption (should return cached result)
    auto result2 = syncWait(shared);
    if (!expectTrue(result2.has_value())) return;
    expectEq(std::get<0>(*result2), 100);

    // Third consumption
    auto result3 = syncWait(shared);
    if (!expectTrue(result3.has_value())) return;
    expectEq(std::get<0>(*result3), 100);
  };

  "split - computes only once"_test = [] {
    int call_count = 0;
    auto shared = split(just(0) | then([&call_count](int) {
      ++call_count;
      return 42;
    }));

    syncWait(shared);
    syncWait(shared);
    syncWait(shared);

    // Should only compute once
    expectEq(call_count, 1);
  };

  "split - with then composition"_test = [] {
    auto shared = split(just(10) | then([](int x) { return x * 2; }));

    auto result1 = syncWait(shared);
    if (!expectTrue(result1.has_value())) return;
    expectEq(std::get<0>(*result1), 20);

    auto result2 = syncWait(shared);
    if (!expectTrue(result2.has_value())) return;
    expectEq(std::get<0>(*result2), 20);
  };

  "split - forwards stopped"_test = [] {
    auto shared = split(StoppedSender{});

    auto result = syncWait(shared);
    expectFalse(result.has_value());

    // Second call should also return empty (cached stopped)
    auto result2 = syncWait(shared);
    expectFalse(result2.has_value());
  };

  "split - forwards error to single consumer"_test = [] {
    auto shared = split(CustomErrorSender1{});
    auto result = syncWait(shared);
    // Error channel → optional is empty
    expectFalse(result.has_value());
  };

  "split - cached error fans out to multiple consumers"_test = [] {
    // split's core contract: compute once, broadcast to all waiters.
    // An error must be cached and re-delivered to every subsequent consumer.
    auto shared = split(CustomErrorSender1{});

    auto r1 = syncWait(shared);
    auto r2 = syncWait(shared);
    auto r3 = syncWait(shared);

    expectFalse(r1.has_value());
    expectFalse(r2.has_value());
    expectFalse(r3.has_value());
  };

  "split - copyable sender"_test = [] {
    auto shared = split(just(999));

    // Make copies
    auto copy1 = shared;
    auto copy2 = shared;

    auto result1 = syncWait(copy1);
    auto result2 = syncWait(copy2);
    auto result3 = syncWait(shared);

    if (!expectTrue(result1.has_value())) return;
    if (!expectTrue(result2.has_value())) return;
    if (!expectTrue(result3.has_value())) return;

    expectEq(std::get<0>(*result1), 999);
    expectEq(std::get<0>(*result2), 999);
    expectEq(std::get<0>(*result3), 999);
  };

  "split - deduction guide"_test = [] {
    // Test that SplitSender can be deduced
    SplitSender sender{just(42)};
    auto result = syncWait(sender);
    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 42);
  };

  return TestRegistry::result();
}
