#include "probabilistic/bloom_filter.h"

#include <random>
#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty filter returns false for contains"_test = [] {
    BloomFilter<1024, 3> filter;
    expectFalse(filter.contains(42));
    expectFalse(filter.contains(0));
    expectFalse(filter.contains(-1));
    expectFalse(filter.contains(std::string{"hello"}));
    expectEq(filter.count(), 0uz);
  };

  "added items return true"_test = [] {
    BloomFilter<1024, 5> filter;

    filter.add(10);
    filter.add(20);
    filter.add(30);

    expectTrue(filter.contains(10));
    expectTrue(filter.contains(20));
    expectTrue(filter.contains(30));
  };

  "string items"_test = [] {
    BloomFilter<2048, 4> filter;

    filter.add(std::string{"apple"});
    filter.add(std::string{"banana"});
    filter.add(std::string{"cherry"});

    expectTrue(filter.contains(std::string{"apple"}));
    expectTrue(filter.contains(std::string{"banana"}));
    expectTrue(filter.contains(std::string{"cherry"}));
    // These might be false positives, but very unlikely with this filter size
  };

  "clear resets filter"_test = [] {
    BloomFilter<512, 3> filter;

    filter.add(1);
    filter.add(2);
    filter.add(3);
    expectTrue(filter.contains(1));
    expectGT(filter.count(), 0uz);

    filter.clear();
    expectFalse(filter.contains(1));
    expectFalse(filter.contains(2));
    expectFalse(filter.contains(3));
    expectEq(filter.count(), 0uz);

    // Can add after clear
    filter.add(42);
    expectTrue(filter.contains(42));
  };

  "merge combines two filters"_test = [] {
    BloomFilter<1024, 4> filter1;
    BloomFilter<1024, 4> filter2;

    filter1.add(1);
    filter1.add(2);
    filter2.add(3);
    filter2.add(4);

    // Before merge
    expectTrue(filter1.contains(1));
    expectTrue(filter1.contains(2));
    expectFalse(filter1.contains(3));  // Very unlikely false positive
    expectFalse(filter1.contains(4));

    // After merge
    filter1.merge(filter2);
    expectTrue(filter1.contains(1));
    expectTrue(filter1.contains(2));
    expectTrue(filter1.contains(3));
    expectTrue(filter1.contains(4));
  };

  "false positive rate is reasonable"_test = [] {
    // Use a filter sized for ~1% false positive rate with 100 elements
    // m = 1000, n = 100, k = 7 gives theoretical FP rate ~0.8%
    // We use m = 1024 (power of 2) and k = 7
    BloomFilter<1024, 7> filter;

    // Insert 100 elements (0-99)
    for (int i = 0; i < 100; ++i) {
      filter.add(i);
    }

    // Verify all inserted elements are found (no false negatives)
    for (int i = 0; i < 100; ++i) {
      expectTrue(filter.contains(i));
    }

    // Count false positives among 10000 elements NOT in the set
    // Use a fixed seed for reproducibility
    std::mt19937 rng{12345};
    std::uniform_int_distribution<int> dist{1000, 100000};

    int false_positives = 0;
    constexpr int kNumTests = 10000;
    for (int i = 0; i < kNumTests; ++i) {
      int val = dist(rng);
      if (filter.contains(val)) {
        ++false_positives;
      }
    }

    double fp_rate = static_cast<double>(false_positives) / kNumTests;
    // Theoretical rate is ~0.8%, allow up to 5% for statistical variance
    // This tolerance is very generous to avoid flaky tests
    // With 10k samples and true rate 0.8%, std error = sqrt(0.008*0.992/10000) = 0.0009
    // 5% threshold is ~50 standard errors away, so effectively never fails
    expectLT(fp_rate, 0.05);
  };

  "static properties"_test = [] {
    using Filter = BloomFilter<2048, 5>;
    expectEq(Filter::size(), 2048uz);
    expectEq(Filter::numHashes(), 5uz);
  };

  "estimate false positive rate"_test = [] {
    // For m=1000, n=100, k=7, theoretical FP rate is ~0.82%
    double rate = BloomFilter<1000, 7>::estimateFalsePositiveRate(100);
    expectLT(rate, 0.02);  // Should be well under 2%
    expectGT(rate, 0.001); // Should be above 0.1%
  };

  "duplicate adds are idempotent"_test = [] {
    BloomFilter<512, 3> filter;

    filter.add(42);
    auto count_before = filter.count();

    filter.add(42);
    filter.add(42);
    auto count_after = filter.count();

    expectEq(count_before, count_after);
    expectTrue(filter.contains(42));
  };

  "different types"_test = [] {
    BloomFilter<1024, 4> filter;

    filter.add(42);
    filter.add(3.14);
    filter.add('x');
    filter.add(std::string{"test"});

    expectTrue(filter.contains(42));
    expectTrue(filter.contains(3.14));
    expectTrue(filter.contains('x'));
    expectTrue(filter.contains(std::string{"test"}));
  };

  return TestRegistry::result();
}
