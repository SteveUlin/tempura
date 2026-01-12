#include "containers/maps/swiss_table_graveyard.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    SwissTableGraveyard<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
    expectEq(0uz, map.tombstoneCount());
  };

  "insert and find"_test = [] {
    SwissTableGraveyard<int, std::string> map;

    auto [ptr1, inserted1] = map.insert(1, "one");
    expectTrue(inserted1);
    expectEq(std::string{"one"}, *ptr1);
    expectEq(1uz, map.size());

    auto [ptr2, inserted2] = map.insert(2, "two");
    expectTrue(inserted2);
    expectEq(std::string{"two"}, *ptr2);
    expectEq(2uz, map.size());

    auto [ptr3, inserted3] = map.insert(1, "ONE");
    expectFalse(inserted3);
    expectEq(std::string{"one"}, *ptr3);
    expectEq(2uz, map.size());
  };

  "insertOrAssign"_test = [] {
    SwissTableGraveyard<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));
    expectFalse(map.insertOrAssign(1, 200));

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    SwissTableGraveyard<std::string, int> map;
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
    SwissTableGraveyard<std::string, int> map;

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
    SwissTableGraveyard<int, int> map;
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

  "tombstone creation on delete"_test = [] {
    SwissTableGraveyard<int, int> map;

    for (int i = 0; i < 10; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(0uz, map.tombstoneCount());

    map.erase(5);
    expectEq(1uz, map.tombstoneCount());

    map.erase(3);
    map.erase(7);
    expectEq(3uz, map.tombstoneCount());
  };

  "primitive tombstones after rehash"_test = [] {
    SwissTableGraveyard<int, int> map{16};

    // Insert elements
    for (int i = 0; i < 8; ++i) {
      map.insert(i, i);
    }

    // Delete some, creating tombstones
    for (int i = 0; i < 4; ++i) {
      map.erase(i);
    }

    expectEq(4uz, map.size());
    expectEq(4uz, map.tombstoneCount());

    // Fill up to trigger rehash (>= 87.5% load factor)
    for (int i = 10; i < 18; ++i) {
      map.insert(i, i);
    }

    // After rehash, tombstones should be placed at primitive positions
    // Target: (capacity - size) / 2 tombstones
    // Should have some tombstones from primitive placement
    expectTrue(map.tombstoneCount() > 0uz);
  };

  "clear"_test = [] {
    SwissTableGraveyard<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.insert(3, 3);
    map.erase(2);

    expectEq(2uz, map.size());
    expectEq(1uz, map.tombstoneCount());

    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
    expectEq(0uz, map.tombstoneCount());
  };

  "high load factor with tombstones"_test = [] {
    SwissTableGraveyard<int, int> map{16};

    for (int i = 0; i < 13; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(16uz, map.capacity());
    expectEq(13uz, map.size());

    for (int i = 0; i < 13; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "tombstone ratio triggers same-capacity rehash"_test = [] {
    SwissTableGraveyard<int, int> map{32};

    // Fill significantly
    for (int i = 0; i < 20; ++i) {
      map.insert(i, i);
    }

    // Delete many elements to create excessive tombstones
    for (int i = 0; i < 15; ++i) {
      map.erase(i);
    }

    std::size_t tombstones_before = map.tombstoneCount();
    expectEq(15uz, tombstones_before);

    // Free slots = 32 - 5 = 27
    // Target max tombstones = 27 * 0.6 = 16.2
    // We have 15 tombstones, which is just below threshold

    // Delete two more to trigger rehash
    // After 1st delete: tombstones=16, free=28, 16*10 = 160 vs 28*6 = 168 (no rehash)
    // After 2nd delete: tombstones=17, free=29, 17*10 = 170 vs 29*6 = 174 (no rehash)
    // After 3rd delete: tombstones=18, free=30, 18*10 = 180 vs 30*6 = 180 (no rehash, equal)
    // Need tombstones > 0.6*free, so one more:
    // After 4th delete: tombstones=19, free=31, 19*10 = 190 vs 31*6 = 186 (rehash!)
    map.erase(15);
    map.erase(16);
    map.erase(17);
    map.erase(18);

    // After rehash, primitive tombstones should be placed
    // Capacity should remain the same
    expectEq(32uz, map.capacity());
    // Tombstones = (capacity - size) / 2 = (32 - 1) / 2 = 15
    expectTrue(map.tombstoneCount() > 0uz);
    expectEq(1uz, map.size());

    // The one remaining element should still be accessible
    expectTrue(map.contains(19));
    expectEq(19, *map.find(19));
  };

  "tombstone reuse"_test = [] {
    SwissTableGraveyard<int, int> map{64};  // Use explicit capacity to control behavior

    for (int i = 0; i < 30; ++i) {
      map.insert(i, i);
    }

    // Delete every other element
    for (int i = 0; i < 30; i += 2) {
      map.erase(i);
    }

    expectEq(15uz, map.size());
    std::size_t tombstones_after_delete = map.tombstoneCount();
    expectEq(15uz, tombstones_after_delete);

    // Remaining elements should still be findable
    for (int i = 1; i < 30; i += 2) {
      expectTrue(map.contains(i));
      expectEq(i, *map.find(i));
    }

    // Reinsert - should reuse tombstones (no longer creates synthetic ones)
    for (int i = 100; i < 110; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(25uz, map.size());

    // All new elements should be accessible
    for (int i = 100; i < 110; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "copy constructor"_test = [] {
    SwissTableGraveyard<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");
    original.erase(2);

    SwissTableGraveyard<int, std::string> copy{original};

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectFalse(copy.contains(2));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
    expectTrue(copy.contains(3));
  };

  "move constructor"_test = [] {
    SwissTableGraveyard<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);
    original.erase(1);

    std::size_t original_tombstones = original.tombstoneCount();

    SwissTableGraveyard<int, int> moved{std::move(original)};

    expectEq(1uz, moved.size());
    expectEq(200, *moved.find(2));
    expectEq(original_tombstones, moved.tombstoneCount());
    expectEq(0uz, original.size());
  };

  "iterator basics"_test = [] {
    SwissTableGraveyard<int, int> map;
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

  "iterator after erase"_test = [] {
    SwissTableGraveyard<int, int> map;

    for (int i = 0; i < 20; ++i) {
      map.insert(i, i * 10);
    }

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
    SwissTableGraveyard<std::string, int> map;

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
    expectEq(1uz, map.tombstoneCount());
  };

  "large scale"_test = [] {
    SwissTableGraveyard<int, int> map;

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

  "capacity is power of 2"_test = [] {
    SwissTableGraveyard<int, int> map1{10};
    expectEq(16uz, map1.capacity());

    SwissTableGraveyard<int, int> map2{20};
    expectEq(32uz, map2.capacity());

    SwissTableGraveyard<int, int> map3{33};
    expectEq(64uz, map3.capacity());
  };

  return TestRegistry::result();
}
