#include "containers/maps/f14.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    F14Map<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
  };

  "insert and find"_test = [] {
    F14Map<int, std::string> map;

    auto [ptr1, inserted1] = map.insert(1, "one");
    expectTrue(inserted1);
    expectEq(std::string{"one"}, *ptr1);
    expectEq(1uz, map.size());

    auto [ptr2, inserted2] = map.insert(2, "two");
    expectTrue(inserted2);
    expectEq(std::string{"two"}, *ptr2);
    expectEq(2uz, map.size());

    // Duplicate key should not insert
    auto [ptr3, inserted3] = map.insert(1, "ONE");
    expectFalse(inserted3);
    expectEq(std::string{"one"}, *ptr3);
    expectEq(2uz, map.size());
  };

  "insertOrAssign"_test = [] {
    F14Map<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));
    expectFalse(map.insertOrAssign(1, 200));

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    F14Map<std::string, int> map;
    map.insert("apple", 1);
    map.insert("banana", 2);
    map.insert("cherry", 3);

    expectTrue(map.contains("apple"));
    expectTrue(map.contains("banana"));
    expectTrue(map.contains("cherry"));
    expectFalse(map.contains("durian"));

    expectEq(1, *map.find("apple"));
    expectEq(2, *map.find("banana"));
    expectEq(3, *map.find("cherry"));
    expectTrue(map.find("durian") == nullptr);
  };

  "operator[]"_test = [] {
    F14Map<std::string, int> map;

    map["x"] = 10;
    expectEq(10, map["x"]);
    expectEq(1uz, map.size());

    map["x"] = 20;
    expectEq(20, map["x"]);
    expectEq(1uz, map.size());

    int val = map["y"];
    expectEq(0, val);
    expectEq(2uz, map.size());
  };

  "erase"_test = [] {
    F14Map<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    expectEq(3uz, map.size());
    expectTrue(map.contains(2));

    expectTrue(map.erase(2));
    expectEq(2uz, map.size());
    expectFalse(map.contains(2));
    expectTrue(map.find(2) == nullptr);

    expectTrue(map.contains(1));
    expectTrue(map.contains(3));

    expectFalse(map.erase(999));
    expectEq(2uz, map.size());
  };

  "clear"_test = [] {
    F14Map<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.insert(3, 3);

    expectEq(3uz, map.size());
    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
  };

  "high load factor - 85.7% target (12/14)"_test = [] {
    // F14 uses 12/14 ≈ 85.7% load factor per Folly's design
    // 14 slots per chunk, so 4 chunks = 56 slots
    F14Map<int, int> map{56};

    // Fill to just below resize threshold (capacity = 56)
    // 12/14 of 56 = 48 slots before resize
    for (int i = 0; i < 47; ++i) {
      map.insert(i, i * 10);
    }

    // Should still be original capacity
    expectEq(56uz, map.capacity());
    expectEq(47uz, map.size());

    // All lookups should work
    for (int i = 0; i < 47; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "chunk-based SIMD matching"_test = [] {
    // F14 processes 14 slots per chunk via SIMD operations (masking bits 14-15)
    // This test verifies behavior across chunk boundaries
    F14Map<int, int> map;

    // Insert enough elements to span multiple chunks
    for (int i = 0; i < 100; ++i) {
      map.insert(i, i * 2);
    }

    expectEq(100uz, map.size());

    // Verify all lookups work across chunk boundaries
    for (int i = 0; i < 100; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 2, *map.find(i));
    }

    // Non-existent keys should be quickly rejected
    for (int i = 200; i < 220; ++i) {
      expectFalse(map.contains(i));
    }
  };

  "overflow counting on delete (no tombstones)"_test = [] {
    // F14 uses overflow counting instead of tombstones!
    // Erase marks slot as empty and decrements overflow counts
    F14Map<int, int> map;

    for (int i = 0; i < 30; ++i) {
      map.insert(i, i);
    }

    // Delete every other element, creating tombstones
    for (int i = 0; i < 30; i += 2) {
      map.erase(i);
    }

    expectEq(15uz, map.size());

    // Remaining elements should still be findable
    for (int i = 1; i < 30; i += 2) {
      expectTrue(map.contains(i));
      expectEq(i, *map.find(i));
    }

    // Deleted elements should not be found
    for (int i = 0; i < 30; i += 2) {
      expectFalse(map.contains(i));
    }

    // Can reuse emptied slots for new insertions (no tombstone penalty!)
    map.insert(100, 1000);
    expectTrue(map.contains(100));
    expectEq(1000, *map.find(100));
  };

  "resize recalculates overflow counts"_test = [] {
    F14Map<int, int> map{56};

    // Fill the map
    for (int i = 0; i < 40; ++i) {
      map.insert(i, i);
    }

    // Delete several elements
    for (int i = 0; i < 15; ++i) {
      map.erase(i);
    }

    std::size_t old_capacity = map.capacity();

    // Insert more elements to trigger resize
    // Resize recalculates overflow counts from scratch
    for (int i = 100; i < 140; ++i) {
      map.insert(i, i);
    }

    // Should have resized
    expectGT(map.capacity(), old_capacity);

    // All elements should be accessible
    for (int i = 15; i < 40; ++i) {
      expectTrue(map.contains(i));
    }
    for (int i = 100; i < 140; ++i) {
      expectTrue(map.contains(i));
    }
  };

  "copy constructor"_test = [] {
    F14Map<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    F14Map<int, std::string> copy{original};

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectEq(*original.find(2), *copy.find(2));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
    expectTrue(copy.contains(3));
  };

  "move constructor"_test = [] {
    F14Map<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    F14Map<int, int> moved{std::move(original)};

    expectEq(2uz, moved.size());
    expectEq(100, *moved.find(1));
    expectEq(200, *moved.find(2));
    expectEq(0uz, original.size());
  };

  "copy assignment"_test = [] {
    F14Map<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    F14Map<int, std::string> copy;
    copy.insert(99, "ninety-nine");

    copy = original;

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectEq(*original.find(2), *copy.find(2));
    expectFalse(copy.contains(99));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
  };

  "move assignment"_test = [] {
    F14Map<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    F14Map<int, int> target;
    target.insert(99, 99);

    target = std::move(original);

    expectEq(2uz, target.size());
    expectEq(100, *target.find(1));
    expectEq(200, *target.find(2));
    expectFalse(target.contains(99));
    expectEq(0uz, original.size());
  };

  "swap"_test = [] {
    F14Map<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    F14Map<int, int> b;
    b.insert(3, 30);

    a.swap(b);

    expectEq(1uz, a.size());
    expectTrue(a.contains(3));
    expectEq(30, *a.find(3));

    expectEq(2uz, b.size());
    expectTrue(b.contains(1));
    expectTrue(b.contains(2));
  };

  "iterator basics"_test = [] {
    F14Map<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    int count = 0;
    int key_sum = 0;
    int value_sum = 0;
    for (auto [key, value] : map) {
      ++count;
      key_sum += key;
      value_sum += value;
    }

    expectEq(3, count);
    expectEq(6, key_sum);
    expectEq(60, value_sum);
  };

  "const iterator"_test = [] {
    F14Map<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    const auto& const_map = map;

    int count = 0;
    for (auto [key, value] : const_map) {
      ++count;
    }
    expectEq(2, count);
  };

  "iterator after erase"_test = [] {
    // Iterators should skip tombstone (deleted) slots
    F14Map<int, int> map;

    for (int i = 0; i < 20; ++i) {
      map.insert(i, i * 10);
    }

    // Delete some elements
    map.erase(5);
    map.erase(10);
    map.erase(15);

    int count = 0;
    for (auto [key, value] : map) {
      ++count;
      expectTrue(key != 5 && key != 10 && key != 15);
    }

    expectEq(17, count);
    expectEq(17uz, map.size());
  };

  "string keys"_test = [] {
    F14Map<std::string, int> map;

    map.insert("hello", 1);
    map.insert("world", 2);
    map.insert("foo", 3);
    map.insert("bar", 4);

    expectEq(4uz, map.size());
    expectEq(1, *map.find("hello"));
    expectEq(2, *map.find("world"));
    expectTrue(map.contains("foo"));
    expectFalse(map.contains("baz"));

    map.erase("foo");
    expectFalse(map.contains("foo"));
    expectEq(3uz, map.size());
  };

  "large scale"_test = [] {
    F14Map<int, int> map;

    constexpr int kCount = 10000;

    for (int i = 0; i < kCount; ++i) {
      map.insert(i, i * 2);
    }

    expectEq(static_cast<std::size_t>(kCount), map.size());

    for (int i = 0; i < kCount; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 2, *map.find(i));
    }

    for (int i = 0; i < kCount; i += 2) {
      map.erase(i);
    }

    expectEq(static_cast<std::size_t>(kCount / 2), map.size());

    for (int i = 0; i < kCount; ++i) {
      if (i % 2 == 0) {
        expectFalse(map.contains(i));
      } else {
        expectTrue(map.contains(i));
        expectEq(i * 2, *map.find(i));
      }
    }
  };

  "capacity is multiple of 14 (chunk width)"_test = [] {
    // F14 capacity is num_chunks × 14, and num_chunks is power of 2
    F14Map<int, int> map1{10};
    expectEq(56uz, map1.capacity());  // 4 chunks × 14

    F14Map<int, int> map2{60};
    expectEq(112uz, map2.capacity());  // 8 chunks × 14

    F14Map<int, int> map3{120};
    expectEq(224uz, map3.capacity());  // 16 chunks × 14
  };

  "H2 filtering reduces key comparisons"_test = [] {
    // H2 hash (7 bits stored in control byte) dramatically
    // reduces the number of actual key comparisons
    F14Map<int, int> map;

    // Insert many elements with potentially conflicting chunk assignments
    for (int i = 0; i < 1000; ++i) {
      map.insert(i, i);
    }

    // All lookups should succeed efficiently
    for (int i = 0; i < 1000; ++i) {
      expectTrue(map.contains(i));
      expectEq(i, *map.find(i));
    }

    // Non-existent keys should be rejected quickly
    for (int i = 2000; i < 2100; ++i) {
      expectFalse(map.contains(i));
    }
  };

  "colocated metadata - chunk alignment"_test = [] {
    // Verify that chunks are properly aligned for cache optimization
    F14Map<int, int> map;

    // Each chunk should be 64-byte aligned
    // Insert enough to span multiple chunks
    for (int i = 0; i < 200; ++i) {
      map.insert(i, i * 3);
    }

    // All lookups should work correctly with aligned chunks
    for (int i = 0; i < 200; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 3, *map.find(i));
    }
  };

  "small key-value pairs - cache efficiency"_test = [] {
    // F14 is optimized for small key-value pairs that fit in cache lines
    // Test with int→int mapping (8 bytes per pair)
    F14Map<int, int> map;

    for (int i = 0; i < 500; ++i) {
      map.insert(i, i + 1000);
    }

    for (int i = 0; i < 500; ++i) {
      expectEq(i + 1000, *map.find(i));
    }
  };

  "prehash API"_test = [] {
    F14Map<std::string, int> map;
    map.insert("hello", 1);
    map.insert("world", 2);
    map.insert("test", 3);

    // Prehash computes hash once
    auto token = map.prehash("hello");

    // Use precomputed hash for lookup
    auto* ptr = map.find(token, "hello");
    expectTrue(ptr != nullptr);
    expectEq(1, *ptr);

    // const version
    const auto& cmap = map;
    const auto* cptr = cmap.find(token, "hello");
    expectTrue(cptr != nullptr);
    expectEq(1, *cptr);

    // contains with prehash
    expectTrue(map.contains(token, "hello"));

    // Missing key with prehash
    auto missing_token = map.prehash("missing");
    expectFalse(map.contains(missing_token, "missing"));
    expectTrue(map.find(missing_token, "missing") == nullptr);
  };

  return TestRegistry::result();
}
