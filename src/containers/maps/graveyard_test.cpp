#include "containers/maps/graveyard.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    GraveyardMap<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
  };

  "insert and find"_test = [] {
    GraveyardMap<int, std::string> map;

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
    expectEq(std::string{"one"}, *ptr3);  // Original value preserved
    expectEq(2uz, map.size());
  };

  "insertOrAssign"_test = [] {
    GraveyardMap<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));   // New insertion
    expectFalse(map.insertOrAssign(1, 200));  // Update existing

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    GraveyardMap<std::string, int> map;
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
    GraveyardMap<std::string, int> map;

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
    GraveyardMap<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    expectEq(3uz, map.size());
    expectTrue(map.contains(2));

    expectTrue(map.erase(2));
    expectEq(2uz, map.size());
    expectFalse(map.contains(2));
    expectTrue(map.find(2) == nullptr);

    // Other elements still accessible
    expectTrue(map.contains(1));
    expectTrue(map.contains(3));

    // Erase non-existent key
    expectFalse(map.erase(999));
    expectEq(2uz, map.size());
  };

  "clear"_test = [] {
    GraveyardMap<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.insert(3, 3);

    expectEq(3uz, map.size());
    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
  };

  "collision handling"_test = [] {
    struct BadHash {
      auto operator()(int key) const -> std::size_t {
        return static_cast<std::size_t>(key % 4);
      }
    };

    GraveyardMap<int, int, BadHash> map;

    map.insert(0, 0);
    map.insert(4, 4);
    map.insert(8, 8);
    map.insert(1, 1);
    map.insert(5, 5);

    expectEq(5uz, map.size());
    expectEq(0, *map.find(0));
    expectEq(4, *map.find(4));
    expectEq(8, *map.find(8));
    expectEq(1, *map.find(1));
    expectEq(5, *map.find(5));
  };

  "tombstone handling"_test = [] {
    struct BadHash {
      auto operator()(int key) const -> std::size_t {
        return 0;  // Everything hashes to slot 0
      }
    };

    GraveyardMap<int, int, BadHash> map;

    map.insert(10, 100);
    map.insert(20, 200);
    map.insert(30, 300);

    map.erase(20);

    // Probe chain should still work
    expectTrue(map.contains(10));
    expectFalse(map.contains(20));
    expectTrue(map.contains(30));
    expectEq(300, *map.find(30));
  };

  "resize triggers"_test = [] {
    GraveyardMap<int, int> map{8};

    for (int i = 0; i < 20; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(20uz, map.size());
    expectGE(map.capacity(), 20uz);

    for (int i = 0; i < 20; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "tombstone maintenance"_test = [] {
    GraveyardMap<int, int> map{16};

    // Insert elements
    for (int i = 0; i < 8; ++i) {
      map.insert(i, i);
    }

    // Delete half to create tombstones
    for (int i = 0; i < 4; ++i) {
      map.erase(i);
    }

    expectEq(4uz, map.size());

    // Re-insert with tombstone reuse
    for (int i = 0; i < 4; ++i) {
      map.insert(i + 100, i);
    }

    expectEq(8uz, map.size());

    // All elements should be accessible
    for (int i = 4; i < 8; ++i) {
      expectTrue(map.contains(i));
    }
    for (int i = 0; i < 4; ++i) {
      expectTrue(map.contains(i + 100));
    }
  };

  "copy constructor"_test = [] {
    GraveyardMap<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    GraveyardMap<int, std::string> copy{original};

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectEq(*original.find(2), *copy.find(2));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
    expectTrue(copy.contains(3));
  };

  "move constructor"_test = [] {
    GraveyardMap<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    GraveyardMap<int, int> moved{std::move(original)};

    expectEq(2uz, moved.size());
    expectEq(100, *moved.find(1));
    expectEq(200, *moved.find(2));

    expectEq(0uz, original.size());
  };

  "copy assignment"_test = [] {
    GraveyardMap<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    GraveyardMap<int, std::string> copy;
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
    GraveyardMap<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    GraveyardMap<int, int> target;
    target.insert(99, 99);

    target = std::move(original);

    expectEq(2uz, target.size());
    expectEq(100, *target.find(1));
    expectEq(200, *target.find(2));
    expectFalse(target.contains(99));

    expectEq(0uz, original.size());
  };

  "swap"_test = [] {
    GraveyardMap<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    GraveyardMap<int, int> b;
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
    GraveyardMap<int, int> map;
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

  "iterator with tombstones"_test = [] {
    GraveyardMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);
    map.insert(4, 40);
    map.erase(2);
    map.erase(4);

    int count = 0;
    for (auto [key, value] : map) {
      ++count;
      expectTrue(key == 1 || key == 3);
    }
    expectEq(2, count);
  };

  "const iterator"_test = [] {
    GraveyardMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    const auto& const_map = map;

    int count = 0;
    for (auto [key, value] : const_map) {
      ++count;
    }
    expectEq(2, count);
  };

  "iterator operator->"_test = [] {
    GraveyardMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    for (auto it = map.begin(); it != map.end(); ++it) {
      if (it->first == 1) {
        it->second = 100;
      }
    }

    expectEq(100, *map.find(1));
    expectEq(20, *map.find(2));
  };

  "string keys"_test = [] {
    GraveyardMap<std::string, int> map;

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
    GraveyardMap<int, int> map;

    constexpr int kCount = 10000;

    for (int i = 0; i < kCount; ++i) {
      map.insert(i, i * 2);
    }

    expectEq(static_cast<std::size_t>(kCount), map.size());

    for (int i = 0; i < kCount; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 2, *map.find(i));
    }

    // Delete half
    for (int i = 0; i < kCount; i += 2) {
      map.erase(i);
    }

    expectEq(static_cast<std::size_t>(kCount / 2), map.size());

    // Verify remaining
    for (int i = 0; i < kCount; ++i) {
      if (i % 2 == 0) {
        expectFalse(map.contains(i));
      } else {
        expectTrue(map.contains(i));
        expectEq(i * 2, *map.find(i));
      }
    }
  };

  "heavy delete and insert pattern"_test = [] {
    GraveyardMap<int, int> map{32};

    // Pattern that stresses tombstone management
    for (int round = 0; round < 5; ++round) {
      // Insert
      for (int i = 0; i < 20; ++i) {
        map.insert(round * 100 + i, i);
      }

      // Delete some
      for (int i = 0; i < 10; ++i) {
        map.erase(round * 100 + i);
      }
    }

    // Map should still work correctly
    expectGE(map.size(), 50uz);

    // Verify we can still insert and find
    map.insert(9999, 9999);
    expectTrue(map.contains(9999));
    expectEq(9999, *map.find(9999));
  };

  return TestRegistry::result();
}
