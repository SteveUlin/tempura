// Tests for repeat algorithms

#include "task/task.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  // =========================================================================
  // repeatN tests
  // =========================================================================

  "repeatN - zero iterations completes immediately"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(0);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 0);
  };

  "repeatN - single iteration"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(1);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 1);
  };

  "repeatN - multiple iterations"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(5);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 5);
  };

  "repeatN - accumulates values across iterations"_test = [] {
    int sum = 0;
    int iteration = 0;
    auto sender = just() | then([&] {
      ++iteration;
      sum += iteration;
      return sum;
    }) | repeatN(10);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(sum, 55);  // 1+2+3+...+10
  };

  "repeatN - free function form"_test = [] {
    int count = 0;
    auto effect = just() | then([&] { return ++count; });
    auto sender = repeatN(std::move(effect), 3);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 3);
  };

  // =========================================================================
  // repeatEffectUntil tests
  // =========================================================================

  "repeatEffectUntil - stops when predicate returns true"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) |
                  repeatEffectUntil([&] { return count >= 5; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 5);
  };

  "repeatEffectUntil - predicate checked after each iteration"_test = [] {
    int count = 0;
    bool should_stop = false;
    auto sender = just() | then([&] {
      ++count;
      if (count == 3) {
        should_stop = true;
      }
      return count;
    }) | repeatEffectUntil([&] { return should_stop; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 3);
  };

  "repeatEffectUntil - immediate stop if predicate starts true"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) |
                  repeatEffectUntil([] { return true; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 1);  // Runs once, then predicate checked
  };

  "repeatEffectUntil - free function form"_test = [] {
    int count = 0;
    auto effect = just() | then([&] { return ++count; });
    auto sender = repeatEffectUntil(std::move(effect), [&] { return count >= 2; });

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 2);
  };

  // =========================================================================
  // repeatEffect tests (would run forever, so we can't directly test)
  // =========================================================================

  "repeatEffect - compiles with pipe syntax"_test = [] {
    // Just verify it compiles - can't actually run it without stop token
    [[maybe_unused]] auto sender = just() | then([] { return 0; }) | repeatEffect();
    expectTrue(true);
  };

  // =========================================================================
  // Edge cases and error handling
  // =========================================================================

  "repeatN - large iteration count"_test = [] {
    int count = 0;
    auto sender = just() | then([&] { return ++count; }) | repeatN(1000);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(count, 1000);
  };

  "repeatN - with state mutation"_test = [] {
    std::vector<int> values;
    auto sender = just() | then([&] {
      values.push_back(static_cast<int>(values.size()));
      return values.size();
    }) | repeatN(5);

    auto result = syncWait(std::move(sender));
    expectTrue(result.has_value());
    expectEq(values.size(), 5u);
    expectEq(values[0], 0);
    expectEq(values[4], 4);
  };

  return TestRegistry::result();
}
