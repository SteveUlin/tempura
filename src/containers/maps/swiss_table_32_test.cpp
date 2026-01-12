#include "containers/maps/swiss_table_32.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    SwissTable32<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
  };

  "insert and find"_test = [] {
    SwissTable32<int, std::string> map;

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
    SwissTable32<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));
    expectFalse(map.insertOrAssign(1, 200));

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    SwissTable32<std::string, int> map;
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
    SwissTable32<std::string, int> map;

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
    SwissTable32<int, int> map;
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
    SwissTable32<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.insert(3, 3);

    expectEq(3uz, map.size());
    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
  };

  "high load factor - 87.5% target"_test = [] {
    // Swiss Tables can operate efficiently at 87.5% load factor
    // due to SIMD batch matching reducing probe lengths
    SwissTable32<int, int> map{32};

    // Fill to just below resize threshold
    for (int i = 0; i < 27; ++i) {  // 27/32 ≈ 84%
      map.insert(i, i * 10);
    }

    // Should still be original capacity
    expectEq(32uz, map.capacity());
    expectEq(27uz, map.size());

    // All lookups should work
    for (int i = 0; i < 27; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "SIMD group matching"_test = [] {
    // Swiss Table processes 32 slots at a time via AVX2 group operations
    // This test verifies behavior across group boundaries
    SwissTable32<int, int> map;

    // Insert enough elements to span multiple groups
    for (int i = 0; i < 100; ++i) {
      map.insert(i, i * 2);
    }

    expectEq(100uz, map.size());

    // Verify all lookups work across group boundaries
    for (int i = 0; i < 100; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 2, *map.find(i));
    }

    // Non-existent keys should be quickly rejected
    for (int i = 200; i < 220; ++i) {
      expectFalse(map.contains(i));
    }
  };

  "tombstone handling (deleted slots)"_test = [] {
    // Swiss Tables use tombstones (DELETED control bytes) instead of
    // backward-shift deletion, preserving SIMD matching efficiency
    SwissTable32<int, int> map;

    for (int i = 0; i < 60; ++i) {
      map.insert(i, i);
    }

    // Delete every other element, creating tombstones
    for (int i = 0; i < 60; i += 2) {
      map.erase(i);
    }

    expectEq(30uz, map.size());

    // Remaining elements should still be findable
    for (int i = 1; i < 60; i += 2) {
      expectTrue(map.contains(i));
      expectEq(i, *map.find(i));
    }

    // Deleted elements should not be found
    for (int i = 0; i < 60; i += 2) {
      expectFalse(map.contains(i));
    }

    // Can reuse tombstone slots for new insertions
    map.insert(200, 2000);
    expectTrue(map.contains(200));
    expectEq(2000, *map.find(200));
  };

  "resize clears tombstones"_test = [] {
    SwissTable32<int, int> map{32};

    // Fill the map
    for (int i = 0; i < 25; ++i) {
      map.insert(i, i);
    }

    // Delete several elements
    for (int i = 0; i < 12; ++i) {
      map.erase(i);
    }

    std::size_t old_capacity = map.capacity();

    // Insert more elements to trigger resize
    // Resize should clear tombstones
    for (int i = 100; i < 130; ++i) {
      map.insert(i, i);
    }

    // Should have resized
    expectGT(map.capacity(), old_capacity);

    // All elements should be accessible
    for (int i = 12; i < 25; ++i) {
      expectTrue(map.contains(i));
    }
    for (int i = 100; i < 130; ++i) {
      expectTrue(map.contains(i));
    }
  };

  "copy constructor"_test = [] {
    SwissTable32<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    SwissTable32<int, std::string> copy{original};

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectEq(*original.find(2), *copy.find(2));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
    expectTrue(copy.contains(3));
  };

  "move constructor"_test = [] {
    SwissTable32<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    SwissTable32<int, int> moved{std::move(original)};

    expectEq(2uz, moved.size());
    expectEq(100, *moved.find(1));
    expectEq(200, *moved.find(2));
    expectEq(0uz, original.size());
  };

  "copy assignment"_test = [] {
    SwissTable32<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    SwissTable32<int, std::string> copy;
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
    SwissTable32<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    SwissTable32<int, int> target;
    target.insert(99, 99);

    target = std::move(original);

    expectEq(2uz, target.size());
    expectEq(100, *target.find(1));
    expectEq(200, *target.find(2));
    expectFalse(target.contains(99));
    expectEq(0uz, original.size());
  };

  "swap"_test = [] {
    SwissTable32<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    SwissTable32<int, int> b;
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
    SwissTable32<int, int> map;
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
    SwissTable32<int, int> map;
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
    SwissTable32<int, int> map;

    for (int i = 0; i < 40; ++i) {
      map.insert(i, i * 10);
    }

    // Delete some elements
    map.erase(10);
    map.erase(20);
    map.erase(30);

    int count = 0;
    for (auto [key, value] : map) {
      ++count;
      expectTrue(key != 10 && key != 20 && key != 30);
    }

    expectEq(37, count);
    expectEq(37uz, map.size());
  };

  "string keys"_test = [] {
    SwissTable32<std::string, int> map;

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
    SwissTable32<int, int> map;

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

  "capacity is power of 2 and >= 32"_test = [] {
    // Swiss Table capacity must be power of 2 (for bitwise AND masking in ProbeSeq32)
    // and at least Group32::kWidth (32) for AVX2 alignment
    SwissTable32<int, int> map1{10};
    expectEq(32uz, map1.capacity());

    SwissTable32<int, int> map2{40};
    expectEq(64uz, map2.capacity());

    SwissTable32<int, int> map3{65};
    expectEq(128uz, map3.capacity());  // Rounds up to next power of 2
  };

  "H2 filtering reduces key comparisons"_test = [] {
    // The H2 hash (7 bits stored in control byte) should dramatically
    // reduce the number of actual key comparisons needed
    SwissTable32<int, int> map;

    // Insert many elements with potentially conflicting H1 values
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

  return TestRegistry::result();
}
