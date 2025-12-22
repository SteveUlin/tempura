#include "cache/clock_cache.h"
#include "cache/fifo_cache.h"
#include "cache/lfu_cache.h"
#include "cache/lifo_cache.h"
#include "cache/lru_cache.h"
#include "cache/lru_k_cache.h"
#include "cache/mfu_cache.h"
#include "cache/mru_cache.h"
#include "cache/random_cache.h"

#include "unit.h"

using namespace tempura;

// ============================================================================
// LruCache tests
// ============================================================================

auto main() -> int {
  "LruCache: insert and get"_test = [] {
    LruCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    auto v1 = cache.get(1);
    expectTrue(v1.has_value());
    expectEq(*v1, std::string{"one"});

    auto v2 = cache.get(2);
    expectTrue(v2.has_value());
    expectEq(*v2, std::string{"two"});
  };

  "LruCache: get missing key returns nullopt"_test = [] {
    LruCache<int, int> cache{3};
    expectFalse(cache.get(42).has_value());
  };

  "LruCache: insert same key returns false"_test = [] {
    LruCache<int, int> cache{3};
    expectTrue(cache.insert(1, 100));
    expectFalse(cache.insert(1, 200));

    auto v = cache.get(1);
    expectTrue(v.has_value());
    expectEq(*v, 200);
  };

  "LruCache: LRU eviction"_test = [] {
    LruCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);
    // Cache: [3, 2, 1] (3 is MRU, 1 is LRU)

    cache.insert(4, 40);  // Evicts key 1
    // Cache: [4, 3, 2]

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "LruCache: touch updates LRU order"_test = [] {
    LruCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);
    // Cache: [3, 2, 1]

    cache.get(1);  // Touch key 1, moves to MRU
    // Cache: [1, 3, 2]

    cache.insert(4, 40);  // Evicts key 2 (now LRU)
    // Cache: [4, 1, 3]

    expectTrue(cache.get(1).has_value());
    expectFalse(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "LruCache: erase removes entry"_test = [] {
    LruCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);

    expectTrue(cache.erase(1));
    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
  };

  "LruCache: erase non-existent returns false"_test = [] {
    LruCache<int, int> cache{3};
    expectFalse(cache.erase(42));
  };

  "LruCache: erase frees capacity"_test = [] {
    LruCache<int, int> cache{2};
    cache.insert(1, 10);
    cache.insert(2, 20);
    // Cache full: [2, 1]

    cache.erase(1);
    // Cache: [2], has room

    cache.insert(3, 30);  // No eviction needed
    cache.insert(4, 40);  // Evicts key 2

    expectFalse(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  // ============================================================================
  // FifoCache tests
  // ============================================================================

  "FifoCache: insert and get"_test = [] {
    FifoCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "FifoCache: FIFO eviction ignores access pattern"_test = [] {
    FifoCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    cache.get(1);  // Access key 1 - but FIFO doesn't care

    cache.insert(4, 40);  // Still evicts key 1 (oldest inserted)

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "FifoCache: update doesn't change eviction order"_test = [] {
    FifoCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    cache.insert(1, 100);  // Update key 1 - still oldest

    cache.insert(4, 40);  // Evicts key 1

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
  };

  // ============================================================================
  // RandomCache tests
  // ============================================================================

  "RandomCache: insert and get"_test = [] {
    RandomCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "RandomCache: eviction removes exactly one entry"_test = [] {
    // Use seeded RNG for reproducibility
    RandomCache<int, int, std::mt19937> cache{3, std::mt19937{42}};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    expectEq(cache.size(), std::size_t{3});

    cache.insert(4, 40);  // Evicts one random entry

    expectEq(cache.size(), std::size_t{3});
    expectTrue(cache.get(4).has_value());

    // Exactly one of 1, 2, 3 should be evicted
    int present = 0;
    if (cache.get(1).has_value()) ++present;
    if (cache.get(2).has_value()) ++present;
    if (cache.get(3).has_value()) ++present;
    expectEq(present, 2);
  };

  // ============================================================================
  // ClockCache tests
  // ============================================================================

  "ClockCache: insert and get"_test = [] {
    ClockCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "ClockCache: second chance prevents immediate eviction"_test = [] {
    ClockCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    // Access all entries - sets reference bits
    cache.get(1);
    cache.get(2);
    cache.get(3);

    // Insert new entry - must give second chances
    cache.insert(4, 40);

    expectEq(cache.size(), std::size_t{3});
    expectTrue(cache.get(4).has_value());
  };

  "ClockCache: unreferenced entries evicted first"_test = [] {
    ClockCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    // Only access key 2 and 3
    cache.get(2);
    cache.get(3);

    cache.insert(4, 40);  // Key 1 has ref bit clear, evicted first

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  // ============================================================================
  // LfuCache tests
  // ============================================================================

  "LfuCache: insert and get"_test = [] {
    LfuCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "LfuCache: evicts least frequently used"_test = [] {
    LfuCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    // Access key 1 and 2 multiple times
    cache.get(1);
    cache.get(1);
    cache.get(2);

    cache.insert(4, 40);  // Evicts key 3 (accessed only once)

    expectTrue(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectFalse(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "LfuCache: ties broken by LRU within frequency"_test = [] {
    LfuCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);
    // All have frequency 1

    cache.insert(4, 40);  // Evicts key 1 (oldest at frequency 1)

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "LfuCache: frequency survives updates"_test = [] {
    LfuCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.get(1);
    cache.get(1);  // freq=3

    cache.insert(2, 20);
    cache.insert(3, 30);  // freq=1 each

    cache.insert(1, 100);  // Update value, freq becomes 4

    cache.insert(4, 40);  // Evicts 2 (lowest freq, oldest)

    expectTrue(cache.get(1).has_value());
    expectEq(*cache.get(1), 100);
  };

  // ============================================================================
  // LruKCache tests
  // ============================================================================

  "LruKCache: insert and get"_test = [] {
    LruKCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "LruKCache: entries with fewer than K accesses evicted first"_test = [] {
    LruKCache<int, int, 2> cache{3};
    cache.insert(1, 10);
    cache.get(1);  // Key 1 now has 2 accesses (insert + get)

    cache.insert(2, 20);
    cache.insert(3, 30);  // Keys 2, 3 have only 1 access each

    cache.insert(4, 40);  // Evicts 2 or 3 (< K accesses)

    expectTrue(cache.get(1).has_value());
    expectTrue(cache.get(4).has_value());
    // Either 2 or 3 evicted, but not 1 (has K accesses)
  };

  "LruKCache: with sufficient accesses, oldest Kth access evicted"_test = [] {
    LruKCache<int, int, 2> cache{2};
    cache.insert(1, 10);
    cache.get(1);  // Key 1: accesses at t=1, t=2

    cache.insert(2, 20);
    cache.get(2);  // Key 2: accesses at t=3, t=4

    // Both have K=2 accesses. Key 1's 2nd-most-recent (Kth) is older
    cache.insert(3, 30);  // Evicts key 1

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
  };

  // ============================================================================
  // MruCache tests (the "bad" opposite of LRU)
  // ============================================================================

  "MruCache: insert and get"_test = [] {
    MruCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "MruCache: evicts most recently used"_test = [] {
    MruCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);
    // Cache: [3, 2, 1] (3 is MRU)

    cache.insert(4, 40);  // Evicts key 3 (MRU), not key 1 (LRU)

    expectTrue(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectFalse(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "MruCache: access makes entry vulnerable"_test = [] {
    MruCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    cache.get(1);  // Touch key 1, makes it MRU (vulnerable)

    cache.insert(4, 40);  // Evicts key 1 (now MRU)

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  // ============================================================================
  // LifoCache tests (the "bad" opposite of FIFO)
  // ============================================================================

  "LifoCache: insert and get"_test = [] {
    LifoCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "LifoCache: evicts newest entry"_test = [] {
    LifoCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    cache.insert(4, 40);  // Evicts key 3 (newest), not key 1 (oldest)

    expectTrue(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectFalse(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "LifoCache: access doesn't change eviction order"_test = [] {
    LifoCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    cache.get(1);  // Access oldest - doesn't matter for LIFO

    cache.insert(4, 40);  // Still evicts key 3 (newest)

    expectTrue(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectFalse(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  // ============================================================================
  // MfuCache tests (the "bad" opposite of LFU)
  // ============================================================================

  "MfuCache: insert and get"_test = [] {
    MfuCache<int, std::string> cache{3};
    expectTrue(cache.insert(1, "one"));
    expectTrue(cache.insert(2, "two"));

    expectEq(*cache.get(1), std::string{"one"});
    expectEq(*cache.get(2), std::string{"two"});
  };

  "MfuCache: evicts most frequently used"_test = [] {
    MfuCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.insert(2, 20);
    cache.insert(3, 30);

    // Access key 1 multiple times (make it "hot")
    cache.get(1);
    cache.get(1);
    cache.get(1);

    cache.insert(4, 40);  // Evicts key 1 (most frequent)

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  "MfuCache: protects cold entries"_test = [] {
    MfuCache<int, int> cache{3};
    cache.insert(1, 10);
    cache.get(1);
    cache.get(1);  // freq=3

    cache.insert(2, 20);  // freq=1
    cache.insert(3, 30);  // freq=1

    cache.insert(4, 40);  // Evicts key 1 (highest freq)

    expectFalse(cache.get(1).has_value());
    expectTrue(cache.get(2).has_value());
    expectTrue(cache.get(3).has_value());
    expectTrue(cache.get(4).has_value());
  };

  // ============================================================================
  // Common functionality tests (using LruCache as representative)
  // ============================================================================

  "size and capacity"_test = [] {
    LruCache<int, int> cache{5};
    expectEq(cache.capacity(), std::size_t{5});
    expectEq(cache.size(), std::size_t{0});

    cache.insert(1, 10);
    cache.insert(2, 20);
    expectEq(cache.size(), std::size_t{2});

    cache.erase(1);
    expectEq(cache.size(), std::size_t{1});
  };

  return TestRegistry::result();
}
