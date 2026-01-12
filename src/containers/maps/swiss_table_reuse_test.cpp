#include "containers/maps/swiss_table_reuse.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    SwissTableReuse<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
  };

  "insert and find"_test = [] {
    SwissTableReuse<int, std::string> map;

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
    SwissTableReuse<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));
    expectFalse(map.insertOrAssign(1, 200));

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    SwissTableReuse<std::string, int> map;
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
    SwissTableReuse<std::string, int> map;

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
    SwissTableReuse<int, int> map;
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

  "tombstone reuse"_test = [] {
    SwissTableReuse<int, int> map;

    // Insert keys that will hash to similar locations
    map.insert(1, 100);
    map.insert(2, 200);
    map.insert(3, 300);

    expectEq(3uz, map.size());

    // Erase middle element, creating a tombstone
    map.erase(2);
    expectEq(2uz, map.size());

    // Insert new key - should reuse tombstone if encountered during probe
    map.insert(4, 400);
    expectEq(3uz, map.size());

    // Verify all keys are accessible
    expectTrue(map.contains(1));
    expectFalse(map.contains(2));  // Erased
    expectTrue(map.contains(3));
    expectTrue(map.contains(4));

    expectEq(100, *map.find(1));
    expectEq(300, *map.find(3));
    expectEq(400, *map.find(4));
  };

  "clear"_test = [] {
    SwissTableReuse<int, int> map;
    map.insert(1, 100);
    map.insert(2, 200);
    expectEq(2uz, map.size());

    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
    expectFalse(map.contains(2));
  };

  "iteration"_test = [] {
    SwissTableReuse<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    int sum = 0;
    for (const auto& [key, value] : map) {
      sum += value;
    }
    expectEq(60, sum);
  };

  "large insertion"_test = [] {
    SwissTableReuse<int, int> map;
    for (int i = 0; i < 1000; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(1000uz, map.size());

    for (int i = 0; i < 1000; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "erase and reinsert pattern"_test = [] {
    SwissTableReuse<int, int> map;

    // Insert initial set
    for (int i = 0; i < 100; ++i) {
      map.insert(i, i);
    }
    expectEq(100uz, map.size());

    // Erase every other element, creating tombstones
    for (int i = 0; i < 100; i += 2) {
      map.erase(i);
    }
    expectEq(50uz, map.size());

    // Reinsert new elements - should reuse tombstones
    for (int i = 1000; i < 1050; ++i) {
      map.insert(i, i);
    }
    expectEq(100uz, map.size());

    // Verify odd numbers from 0-99 and 1000-1049 exist
    for (int i = 1; i < 100; i += 2) {
      expectTrue(map.contains(i));
    }
    for (int i = 1000; i < 1050; ++i) {
      expectTrue(map.contains(i));
    }
  };

  return TestRegistry::result();
}
