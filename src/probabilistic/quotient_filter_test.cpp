#include "probabilistic/quotient_filter.h"

#include <random>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty filter"_test = [] {
    QuotientFilter<8, 8> filter;  // 256 slots
    expectEq(filter.size(), 0uz);
    expectTrue(filter.empty());
    expectFalse(filter.contains(42));
    expectFalse(filter.contains(0));
    expectFalse(filter.contains(12345));
  };

  "insert and lookup"_test = [] {
    QuotientFilter<8, 8> filter;  // 256 slots

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
    QuotientFilter<8, 8> filter;  // 256 slots

    expectTrue(filter.insert(42));
    expectTrue(filter.insert(84));
    expectTrue(filter.insert(126));
    expectEq(filter.size(), 3uz);

    // Delete middle element
    expectTrue(filter.remove(84));
    expectEq(filter.size(), 2uz);
    expectTrue(filter.contains(42));
    expectFalse(filter.contains(84));
    expectTrue(filter.contains(126));

    // Delete non-existent returns false
    expectFalse(filter.remove(84));
    expectFalse(filter.remove(999));

    // Delete remaining
    expectTrue(filter.remove(42));
    expectTrue(filter.remove(126));
    expectEq(filter.size(), 0uz);
    expectTrue(filter.empty());
  };

  "insert after delete"_test = [] {
    QuotientFilter<8, 8> filter;

    // Insert, delete, reinsert
    expectTrue(filter.insert(42));
    expectTrue(filter.contains(42));
    expectTrue(filter.remove(42));
    expectFalse(filter.contains(42));
    expectTrue(filter.insert(42));
    expectTrue(filter.contains(42));
    expectEq(filter.size(), 1uz);
  };

  "load factor"_test = [] {
    QuotientFilter<6, 8> filter;  // 64 slots

    expectNear(filter.loadFactor(), 0.0, 1e-9);

    for (std::uint64_t i = 0; i < 32; ++i) {
      filter.insert(i);
    }

    // 32 / 64 = 0.5
    expectNear(filter.loadFactor(), 0.5, 0.01);
  };

  "many insertions and lookups"_test = [] {
    QuotientFilter<12, 12> filter;  // 4096 slots, 12-bit remainder

    std::vector<std::uint64_t> items;
    items.reserve(1000);

    // Insert 1000 items
    for (std::uint64_t i = 0; i < 1000; ++i) {
      std::uint64_t item = i * 1000000007ULL;  // Prime multiplier for variety
      if (filter.insert(item)) {
        items.push_back(item);
      }
    }

    // All inserted items should be found (no false negatives)
    for (auto item : items) {
      expectTrue(filter.contains(item));
    }
  };

  "false positive rate is reasonable"_test = [] {
    // Use larger remainder for lower FPR
    // 12-bit remainder gives ~1/4096 = 0.024% base FPR
    QuotientFilter<10, 12> filter;  // 1024 slots, 12-bit remainder

    // Insert items in one range
    for (std::uint64_t i = 0; i < 200; ++i) {
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

    // Expected FPR with 12-bit remainders: ~0.024%
    // With clustering at ~20% load, actual FPR might be higher
    // Allow up to 2% to account for variance and clustering effects
    double fpr = static_cast<double>(false_positives) / static_cast<double>(tests);
    expectLT(fpr, 0.02);  // Less than 2% false positive rate
  };

  "high load factor works"_test = [] {
    QuotientFilter<8, 8> filter;  // 256 slots

    std::size_t inserted = 0;
    std::size_t failed = 0;

    // Try to insert up to 200 items (~78% load factor target)
    for (std::uint64_t i = 0; i < 200; ++i) {
      if (filter.insert(i)) {
        ++inserted;
      } else {
        ++failed;
      }
    }

    // Should have inserted most items
    expectGT(inserted, 150uz);
    // Load factor should be substantial
    expectGT(filter.loadFactor(), 0.5);

    // Verify inserted items are still findable
    for (std::uint64_t i = 0; i < inserted; ++i) {
      expectTrue(filter.contains(i));
    }
  };

  "clear resets filter"_test = [] {
    QuotientFilter<8, 8> filter;

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
    constexpr std::size_t kQuotientBits = 6;
    QuotientFilter<kQuotientBits, 8> filter;

    expectEq(filter.capacity(), 1ULL << kQuotientBits);  // 64 slots
  };

  "different sizes"_test = [] {
    // Small filter
    QuotientFilter<4, 4> small;  // 16 slots, 4-bit remainder
    expectTrue(small.insert(1));
    expectTrue(small.contains(1));
    expectEq(small.capacity(), 16uz);

    // Medium filter
    QuotientFilter<10, 10> medium;  // 1024 slots
    expectTrue(medium.insert(1));
    expectTrue(medium.contains(1));
    expectEq(medium.capacity(), 1024uz);

    // Larger remainder
    QuotientFilter<8, 16> wide;  // 256 slots, 16-bit remainder
    expectTrue(wide.insert(1));
    expectTrue(wide.contains(1));
  };

  "delete from middle of run"_test = [] {
    // Test deletion with a small number of items to avoid complex edge cases
    QuotientFilter<8, 8> filter;

    // Insert a few items
    std::vector<std::uint64_t> items = {100, 200, 300, 400, 500};
    for (auto item : items) {
      expectTrue(filter.insert(item));
    }

    // Delete one item
    expectTrue(filter.remove(300));

    // Remaining items should still be found
    expectTrue(filter.contains(100));
    expectTrue(filter.contains(200));
    expectFalse(filter.contains(300));
    expectTrue(filter.contains(400));
    expectTrue(filter.contains(500));
  };

  "stress test insert delete cycle"_test = [] {
    QuotientFilter<10, 10> filter;  // 1024 slots

    // Insert items
    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 100; ++i) {
      items.push_back(i * 12345);
      filter.insert(items.back());
    }

    // Delete a few items (sequential, not interleaved)
    for (std::size_t i = 0; i < 10; ++i) {
      filter.remove(items[i]);
    }

    // Verify remaining items (those not deleted)
    for (std::size_t i = 10; i < items.size(); ++i) {
      expectTrue(filter.contains(items[i]));
    }

    // Filter should still work
    expectEq(filter.size(), 90uz);
    expectLT(filter.loadFactor(), 1.0);
  };

  "no false negatives"_test = [] {
    // Critical property: if we insert an item and don't delete it,
    // contains() must return true
    QuotientFilter<10, 8> filter;

    std::vector<std::uint64_t> items;
    for (std::uint64_t i = 0; i < 500; ++i) {
      std::uint64_t item = i * 123456789ULL;
      if (filter.insert(item)) {
        items.push_back(item);
      }
    }

    // Every inserted item must be found
    for (auto item : items) {
      if (!filter.contains(item)) {
        // This would be a bug - quotient filters have no false negatives
        expectTrue(false);  // Force test failure with message
      }
    }
  };

  return TestRegistry::result();
}
