#include "probabilistic/reservoir_sampling.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty reservoir"_test = [] {
    ReservoirSampling<int, 5> rs;
    expectEq(rs.count(), 0uz);
    expectEq(rs.size(), 0uz);
    expectFalse(rs.full());
  };

  "fewer items than reservoir size"_test = [] {
    ReservoirSampling<int, 10> rs;
    rs.add(1);
    rs.add(2);
    rs.add(3);

    expectEq(rs.count(), 3uz);
    expectEq(rs.size(), 3uz);
    expectFalse(rs.full());

    // All items should be in reservoir
    const auto& sample = rs.sample();
    expectEq(sample[0], 1);
    expectEq(sample[1], 2);
    expectEq(sample[2], 3);
  };

  "reservoir fills correctly"_test = [] {
    ReservoirSampling<int, 5> rs{42};  // Fixed seed for reproducibility
    for (int i = 1; i <= 5; ++i) {
      rs.add(i);
    }

    expectEq(rs.count(), 5uz);
    expectEq(rs.size(), 5uz);
    expectTrue(rs.full());

    // Reservoir should contain exactly 1,2,3,4,5
    std::vector<int> items{rs.begin(), rs.end()};
    std::sort(items.begin(), items.end());
    std::vector<int> expected{1, 2, 3, 4, 5};
    expectRangeEq(items, expected);
  };

  "add returns true when item enters reservoir"_test = [] {
    ReservoirSampling<int, 3> rs{12345};

    // First K items always enter
    expectTrue(rs.add(1));
    expectTrue(rs.add(2));
    expectTrue(rs.add(3));

    // After that, some may or may not enter
    int entered = 0;
    for (int i = 4; i <= 100; ++i) {
      if (rs.add(i)) {
        ++entered;
      }
    }
    // With k=3 and n=100, expected replacements ≈ 3×ln(100/3) ≈ 10.5
    // Some should have entered, but not all
    expectGT(entered, 0);
    expectLT(entered, 97);
  };

  "statistical uniformity - each position equally likely"_test = [] {
    // Test that each original item has roughly equal probability of being in
    // the final reservoir. We run many trials and count how often each item
    // from a stream of N items appears in the reservoir of size K.
    constexpr std::size_t kReservoirSize = 5;
    constexpr int kStreamSize = 50;
    constexpr int kTrials = 10000;

    // position_counts[i] = how many times item i appeared in final reservoir
    std::array<int, kStreamSize> position_counts{};

    for (int trial = 0; trial < kTrials; ++trial) {
      ReservoirSampling<int, kReservoirSize> rs;
      for (int i = 0; i < kStreamSize; ++i) {
        rs.add(i);
      }

      // Count which items are in the reservoir
      for (std::size_t j = 0; j < rs.size(); ++j) {
        int item = rs[j];
        ++position_counts[static_cast<std::size_t>(item)];
      }
    }

    // Each item should appear in the reservoir with probability k/n = 5/50 = 0.1
    // Expected count per item = 10000 × 0.1 = 1000
    // Standard deviation = √(n × p × (1-p)) = √(10000 × 0.1 × 0.9) ≈ 30
    // We use tolerance of 200 (≈6.7 standard deviations) to avoid flaky tests
    double expected = static_cast<double>(kTrials * kReservoirSize) / kStreamSize;

    for (int i = 0; i < kStreamSize; ++i) {
      double observed = static_cast<double>(position_counts[static_cast<std::size_t>(i)]);
      expectNear(observed, expected, 200.0);
    }
  };

  "clear and reuse"_test = [] {
    ReservoirSampling<int, 3> rs{42};
    rs.add(1);
    rs.add(2);
    rs.add(3);
    rs.add(4);
    rs.add(5);

    expectEq(rs.count(), 5uz);
    expectTrue(rs.full());

    rs.clear();

    expectEq(rs.count(), 0uz);
    expectEq(rs.size(), 0uz);
    expectFalse(rs.full());

    // Can add new items after clear
    rs.add(100);
    rs.add(200);
    expectEq(rs.count(), 2uz);
    expectEq(rs.size(), 2uz);
    expectEq(rs[0], 100);
    expectEq(rs[1], 200);
  };

  "string type"_test = [] {
    ReservoirSampling<std::string, 3> rs{42};
    rs.add("apple");
    rs.add("banana");
    rs.add("cherry");

    expectEq(rs.count(), 3uz);
    expectEq(rs.size(), 3uz);

    // All strings should be present
    std::vector<std::string> items{rs.begin(), rs.end()};
    std::sort(items.begin(), items.end());
    std::vector<std::string> expected{"apple", "banana", "cherry"};
    expectRangeEq(items, expected);
  };

  "move semantics"_test = [] {
    ReservoirSampling<std::string, 2> rs{42};

    std::string s1 = "hello";
    std::string s2 = "world";
    rs.add(std::move(s1));
    rs.add(std::move(s2));

    expectEq(rs.size(), 2uz);
    // After move, original strings should be empty or in valid but unspecified state
    // The reservoir should have the values
    std::vector<std::string> items{rs.begin(), rs.end()};
    std::sort(items.begin(), items.end());
    expectTrue(items[0] == "hello" || items[0] == "world");
    expectTrue(items[1] == "hello" || items[1] == "world");
  };

  "reproducibility with same seed"_test = [] {
    auto run = [](std::uint64_t seed) {
      ReservoirSampling<int, 5> rs{seed};
      for (int i = 0; i < 1000; ++i) {
        rs.add(i);
      }
      return std::vector<int>{rs.begin(), rs.end()};
    };

    auto result1 = run(12345);
    auto result2 = run(12345);
    auto result3 = run(54321);  // Different seed

    expectRangeEq(result1, result2);
    expectNeq(result1 == result3, true);  // Very likely different
  };

  "single element reservoir"_test = [] {
    // Edge case: reservoir size of 1
    ReservoirSampling<int, 1> rs{42};

    for (int i = 0; i < 100; ++i) {
      rs.add(i);
    }

    expectEq(rs.count(), 100uz);
    expectEq(rs.size(), 1uz);
    expectTrue(rs.full());

    // The single element should be some value from 0-99
    int value = rs[0];
    expectGE(value, 0);
    expectLT(value, 100);
  };

  "large stream"_test = [] {
    ReservoirSampling<int, 10> rs{42};

    for (int i = 0; i < 100000; ++i) {
      rs.add(i);
    }

    expectEq(rs.count(), 100000uz);
    expectEq(rs.size(), 10uz);

    // All elements should be valid (from the stream)
    for (std::size_t i = 0; i < rs.size(); ++i) {
      expectGE(rs[i], 0);
      expectLT(rs[i], 100000);
    }
  };

  "range-based for loop"_test = [] {
    ReservoirSampling<int, 5> rs{42};
    rs.add(10);
    rs.add(20);
    rs.add(30);

    int sum = 0;
    int count = 0;
    for (int x : rs) {
      sum += x;
      ++count;
    }

    expectEq(count, 3);
    expectEq(sum, 60);
  };

  return TestRegistry::result();
}
