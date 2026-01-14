#include "probabilistic/cuckoo_filter.h"

#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty filter"_test = [] {
    CuckooFilter<256> filter;
    expectEq(filter.size(), 0uz);
    expectTrue(filter.empty());
    expectFalse(filter.contains(42));
    expectFalse(filter.contains(0));
    expectFalse(filter.contains(12345));
  };

  "insert and lookup"_test = [] {
    CuckooFilter<256> filter;

    expectTrue(filter.insert(100));
    expectTrue(filter.insert(200));
    expectTrue(filter.insert(300));

    expectEq(filter.size(), 3uz);
    expectFalse(filter.empty());
    expectTrue(filter.contains(100));
    expectTrue(filter.contains(200));
    expectTrue(filter.contains(300));
  };

  "deletion works"_test = [] {
    CuckooFilter<256> filter;

    expectTrue(filter.insert(42));
    expectTrue(filter.insert(84));
    expectTrue(filter.insert(126));
    expectEq(filter.size(), 3uz);

    // Delete middle element
    expectTrue(filter.erase(84));
    expectEq(filter.size(), 2uz);
    expectTrue(filter.contains(42));
    expectFalse(filter.contains(84));
    expectTrue(filter.contains(126));

    // Delete non-existent returns false
    expectFalse(filter.erase(84));
    expectFalse(filter.erase(999));

    // Delete remaining
    expectTrue(filter.erase(42));
    expectTrue(filter.erase(126));
    expectEq(filter.size(), 0uz);
    expectTrue(filter.empty());
  };

  "load factor"_test = [] {
    CuckooFilter<64, 4> filter;  // 64 buckets * 4 slots = 256 capacity

    expectNear(filter.loadFactor(), 0.0, 1e-9);

    for (std::uint64_t i = 0; i < 128; ++i) {
      filter.insert(i);
    }

    // 128 / 256 = 0.5
    expectNear(filter.loadFactor(), 0.5, 0.01);
  };

  "filter reports full when capacity exceeded"_test = [] {
    // Small filter: 16 buckets * 4 slots = 64 capacity
    CuckooFilter<16, 4> filter;

    std::size_t inserted = 0;
    std::size_t failed = 0;

    // Try to insert more than capacity
    for (std::uint64_t i = 0; i < 200; ++i) {
      if (filter.insert(i)) {
        ++inserted;
      } else {
        ++failed;
      }
    }

    // Should have some failures (filter saturates around 95%)
    expectGT(failed, 0uz);
    // Should have inserted most items before saturation
    expectGT(inserted, 50uz);
    // Load factor should be high
    expectGT(filter.loadFactor(), 0.8);
  };

  "many insertions and lookups"_test = [] {
    CuckooFilter<1024, 4, 16> filter;  // Larger filter with 16-bit fingerprints

    std::vector<std::uint64_t> items;
    items.reserve(1000);

    // Insert 1000 items
    for (std::uint64_t i = 0; i < 1000; ++i) {
      std::uint64_t item = i * 1000000007ULL;  // Prime multiplier for variety
      if (filter.insert(item)) {
        items.push_back(item);
      }
    }

    // All inserted items should be found
    for (auto item : items) {
      expectTrue(filter.contains(item));
    }
  };

  "false positive rate is reasonable"_test = [] {
    // Use larger fingerprint for lower FPR
    CuckooFilter<512, 4, 16> filter;

    // Insert items in one range
    for (std::uint64_t i = 0; i < 500; ++i) {
      filter.insert(i);
    }

    // Check for false positives in a different range
    std::size_t false_positives = 0;
    std::size_t tests = 10000;

    for (std::uint64_t i = 1000000; i < 1000000 + tests; ++i) {
      if (filter.contains(i)) {
        ++false_positives;
      }
    }

    // Expected FPR with 16-bit fingerprints and bucket size 4:
    // FPR ≈ 2 * BucketSize / 2^FingerprintBits = 8 / 65536 ≈ 0.012%
    // With 10k tests, expected ~1-2 false positives
    // Allow up to 100 (1%) to account for variance
    double fpr = static_cast<double>(false_positives) / static_cast<double>(tests);
    expectLT(fpr, 0.01);  // Less than 1% false positive rate
  };

  "clear resets filter"_test = [] {
    CuckooFilter<128> filter;

    for (std::uint64_t i = 0; i < 100; ++i) {
      filter.insert(i);
    }
    expectGT(filter.size(), 0uz);

    filter.clear();
    expectEq(filter.size(), 0uz);
    expectTrue(filter.empty());
    expectNear(filter.loadFactor(), 0.0, 1e-9);

    // Items should no longer be found
    for (std::uint64_t i = 0; i < 100; ++i) {
      expectFalse(filter.contains(i));
    }

    // Can insert again after clear
    expectTrue(filter.insert(42));
    expectTrue(filter.contains(42));
  };

  "capacity is correct"_test = [] {
    constexpr std::size_t kBuckets = 64;
    constexpr std::size_t kSlots = 4;
    CuckooFilter<kBuckets, kSlots> filter;

    expectEq(filter.capacity(), kBuckets * kSlots);
  };

  "different fingerprint sizes"_test = [] {
    // 8-bit fingerprints
    CuckooFilter<64, 4, 8> filter8;
    expectTrue(filter8.insert(1));
    expectTrue(filter8.contains(1));

    // 16-bit fingerprints
    CuckooFilter<64, 4, 16> filter16;
    expectTrue(filter16.insert(1));
    expectTrue(filter16.contains(1));

    // 32-bit fingerprints
    CuckooFilter<64, 4, 32> filter32;
    expectTrue(filter32.insert(1));
    expectTrue(filter32.contains(1));
  };

  "insert same item multiple times"_test = [] {
    CuckooFilter<64> filter;

    // First insert should succeed
    expectTrue(filter.insert(42));
    expectEq(filter.size(), 1uz);

    // Inserting same item again also succeeds (adds another fingerprint)
    // This is expected behavior - cuckoo filters don't track uniqueness
    expectTrue(filter.insert(42));
    expectEq(filter.size(), 2uz);

    // Item should still be found
    expectTrue(filter.contains(42));

    // Deleting once removes one copy
    expectTrue(filter.erase(42));
    expectEq(filter.size(), 1uz);
    expectTrue(filter.contains(42));

    // Delete second copy
    expectTrue(filter.erase(42));
    expectEq(filter.size(), 0uz);
    expectFalse(filter.contains(42));
  };

  "stress test with evictions"_test = [] {
    // Fill filter close to capacity to trigger evictions
    CuckooFilter<128, 4, 12> filter;

    std::vector<std::uint64_t> inserted;

    // Insert items - track which ones succeeded
    for (std::uint64_t i = 0; i < 400; ++i) {
      if (filter.insert(i)) {
        inserted.push_back(i);
        // Immediately verify it can be found
        expectTrue(filter.contains(i));
      }
    }

    // Should have inserted a significant number
    expectGT(inserted.size(), 300uz);

    // All inserted items should still be found
    for (auto item : inserted) {
      expectTrue(filter.contains(item));
    }

    // Delete first half
    std::size_t half = inserted.size() / 2;
    for (std::size_t i = 0; i < half; ++i) {
      expectTrue(filter.erase(inserted[i]));
    }

    // Verify deleted items are gone, remaining are present
    for (std::size_t i = 0; i < inserted.size(); ++i) {
      if (i < half) {
        expectFalse(filter.contains(inserted[i]));
      } else {
        expectTrue(filter.contains(inserted[i]));
      }
    }
  };

  return TestRegistry::result();
}
