// Tests for bulk sender - execute function for each index

#include "task/task.h"
#include "task/test_helpers.h"

#include <array>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // ==========================================================================
  // Basic bulk functionality
  // ==========================================================================

  "bulk - basic counter"_test = [] {
    int sum = 0;
    auto result = syncWait(
        just(10) | bulk(5, [&sum](std::size_t i, int value) {
          sum += static_cast<int>(i) + value;
        }));

    if (!expectTrue(result.has_value())) return;
    // Original value is forwarded
    expectEq(std::get<0>(*result), 10);
    // sum = (0+10) + (1+10) + (2+10) + (3+10) + (4+10) = 60
    expectEq(sum, 60);
  };

  "bulk - zero iterations"_test = [] {
    int call_count = 0;
    auto result = syncWait(
        just(42) | bulk(0, [&call_count](std::size_t, int) {
          ++call_count;
        }));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 42);
    expectEq(call_count, 0);
  };

  "bulk - single iteration"_test = [] {
    int captured_index = -1;
    int captured_value = -1;

    auto result = syncWait(
        just(100) | bulk(1, [&](std::size_t i, int v) {
          captured_index = static_cast<int>(i);
          captured_value = v;
        }));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 100);
    expectEq(captured_index, 0);
    expectEq(captured_value, 100);
  };

  "bulk - modifies values by reference"_test = [] {
    std::array<int, 4> arr = {1, 2, 3, 4};

    auto result = syncWait(
        just(std::ref(arr)) | bulk(4, [](std::size_t i, std::reference_wrapper<std::array<int, 4>> a) {
          a.get()[i] *= 2;
        }));

    if (!expectTrue(result.has_value())) return;

    // Array should be modified
    expectEq(arr[0], 2);
    expectEq(arr[1], 4);
    expectEq(arr[2], 6);
    expectEq(arr[3], 8);
  };

  "bulk - multiple values from sender"_test = [] {
    std::vector<std::pair<int, int>> calls;

    auto result = syncWait(
        just(10, 20) | bulk(3, [&calls](std::size_t i, int a, int b) {
          calls.push_back({a + static_cast<int>(i), b + static_cast<int>(i)});
        }));

    if (!expectTrue(result.has_value())) return;

    auto [v1, v2] = *result;
    expectEq(v1, 10);
    expectEq(v2, 20);

    expectEq(calls.size(), 3u);
    expectEq(calls[0].first, 10);
    expectEq(calls[0].second, 20);
    expectEq(calls[1].first, 11);
    expectEq(calls[1].second, 21);
    expectEq(calls[2].first, 12);
    expectEq(calls[2].second, 22);
  };

  // ==========================================================================
  // Composition
  // ==========================================================================

  "bulk - chained with then"_test = [] {
    int bulk_sum = 0;

    auto result = syncWait(
        just(5)
        | then([](int x) { return x * 2; })
        | bulk(3, [&bulk_sum](std::size_t i, int v) {
            bulk_sum += v + static_cast<int>(i);
          })
        | then([](int x) { return x + 100; }));

    if (!expectTrue(result.has_value())) return;
    // Original value was 5, then doubled to 10
    // bulk sum = (10+0) + (10+1) + (10+2) = 33
    // then adds 100 to 10 = 110
    expectEq(std::get<0>(*result), 110);
    expectEq(bulk_sum, 33);
  };

  "bulk - multiple bulk in sequence"_test = [] {
    int sum1 = 0;
    int sum2 = 0;

    auto result = syncWait(
        just(1)
        | bulk(2, [&sum1](std::size_t i, int v) { sum1 += v + static_cast<int>(i); })
        | bulk(3, [&sum2](std::size_t i, int v) { sum2 += v + static_cast<int>(i); }));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 1);
    // sum1 = (1+0) + (1+1) = 3
    expectEq(sum1, 3);
    // sum2 = (1+0) + (1+1) + (1+2) = 6
    expectEq(sum2, 6);
  };

  // ==========================================================================
  // Three-argument form
  // ==========================================================================

  "bulk - three argument form"_test = [] {
    int sum = 0;
    auto result = syncWait(bulk(just(7), 4, [&sum](std::size_t i, int v) {
      sum += static_cast<int>(i) * v;
    }));

    if (!expectTrue(result.has_value())) return;
    expectEq(std::get<0>(*result), 7);
    // sum = 0*7 + 1*7 + 2*7 + 3*7 = 42
    expectEq(sum, 42);
  };

  // ==========================================================================
  // Error and stopped propagation
  // ==========================================================================

  "bulk - stopped propagates through"_test = [] {
    int call_count = 0;
    auto result = syncWait(
        StoppedSender{}
        | bulk(5, [&call_count](std::size_t, int) {
            ++call_count;
          }));

    expectFalse(result.has_value());
    expectEq(call_count, 0);  // Bulk function not called on stopped
  };

  // ==========================================================================
  // Different shape types
  // ==========================================================================

  "bulk - int shape"_test = [] {
    int count = 0;
    auto result = syncWait(
        just(0) | bulk(3, [&count](int i, int) { count += i; }));

    if (!expectTrue(result.has_value())) return;
    // count = 0 + 1 + 2 = 3
    expectEq(count, 3);
  };

  "bulk - size_t shape"_test = [] {
    std::size_t count = 0;
    auto result = syncWait(
        just(0) | bulk(std::size_t{4}, [&count](std::size_t i, int) { count += i; }));

    if (!expectTrue(result.has_value())) return;
    // count = 0 + 1 + 2 + 3 = 6
    expectEq(count, std::size_t{6});
  };

  return TestRegistry::result();
}
