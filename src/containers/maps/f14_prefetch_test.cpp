#include "containers/maps/f14_prefetch.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    F14Prefetch<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
  };

  "insert and find"_test = [] {
    F14Prefetch<int, std::string> map;

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
    F14Prefetch<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));
    expectFalse(map.insertOrAssign(1, 200));

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    F14Prefetch<std::string, int> map;
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
    F14Prefetch<std::string, int> map;

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
    F14Prefetch<int, int> map;
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
    F14Prefetch<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.insert(3, 3);

    expectEq(3uz, map.size());
    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
  };

  "high load factor - 85.7% (12/14)"_test = [] {
    // F14 uses 14 slots per chunk, 12/14 load factor
    F14Prefetch<int, int> map{56};  // 4 chunks × 14 = 56

    for (int i = 0; i < 47; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(56uz, map.capacity());
    expectEq(47uz, map.size());

    for (int i = 0; i < 47; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  "chunk-based probing with prefetch"_test = [] {
    // Prefetching happens during chunk probing
    F14Prefetch<int, int> map;

    for (int i = 0; i < 100; ++i) {
      map.insert(i, i * 2);
    }

    expectEq(100uz, map.size());

    for (int i = 0; i < 100; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 2, *map.find(i));
    }

    for (int i = 200; i < 220; ++i) {
      expectFalse(map.contains(i));
    }
  };

  "tombstone handling"_test = [] {
    F14Prefetch<int, int> map;

    for (int i = 0; i < 30; ++i) {
      map.insert(i, i);
    }

    for (int i = 0; i < 30; i += 2) {
      map.erase(i);
    }

    expectEq(15uz, map.size());

    for (int i = 1; i < 30; i += 2) {
      expectTrue(map.contains(i));
      expectEq(i, *map.find(i));
    }

    for (int i = 0; i < 30; i += 2) {
      expectFalse(map.contains(i));
    }

    map.insert(100, 1000);
    expectTrue(map.contains(100));
    expectEq(1000, *map.find(100));
  };

  "resize recalculates overflow counts"_test = [] {
    F14Prefetch<int, int> map{56};

    for (int i = 0; i < 40; ++i) {
      map.insert(i, i);
    }

    for (int i = 0; i < 15; ++i) {
      map.erase(i);
    }

    std::size_t old_capacity = map.capacity();

    for (int i = 100; i < 140; ++i) {
      map.insert(i, i);
    }

    expectGT(map.capacity(), old_capacity);

    for (int i = 15; i < 40; ++i) {
      expectTrue(map.contains(i));
    }
    for (int i = 100; i < 140; ++i) {
      expectTrue(map.contains(i));
    }
  };

  "copy constructor"_test = [] {
    F14Prefetch<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    F14Prefetch<int, std::string> copy{original};

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectEq(*original.find(2), *copy.find(2));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
    expectTrue(copy.contains(3));
  };

  "move constructor"_test = [] {
    F14Prefetch<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    F14Prefetch<int, int> moved{std::move(original)};

    expectEq(2uz, moved.size());
    expectEq(100, *moved.find(1));
    expectEq(200, *moved.find(2));
    expectEq(0uz, original.size());
  };

  "copy assignment"_test = [] {
    F14Prefetch<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    F14Prefetch<int, std::string> copy;
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
    F14Prefetch<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    F14Prefetch<int, int> target;
    target.insert(99, 99);

    target = std::move(original);

    expectEq(2uz, target.size());
    expectEq(100, *target.find(1));
    expectEq(200, *target.find(2));
    expectFalse(target.contains(99));
    expectEq(0uz, original.size());
  };

  "swap"_test = [] {
    F14Prefetch<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    F14Prefetch<int, int> b;
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
    F14Prefetch<int, int> map;
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
    F14Prefetch<int, int> map;
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
    F14Prefetch<int, int> map;

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
    F14Prefetch<std::string, int> map;

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
    F14Prefetch<int, int> map;

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

  "capacity is multiple of 14"_test = [] {
    // 14 slots per chunk
    F14Prefetch<int, int> map1{10};
    expectEq(56uz, map1.capacity());  // 4 chunks × 14

    F14Prefetch<int, int> map2{60};
    expectEq(112uz, map2.capacity());  // 8 chunks × 14

    F14Prefetch<int, int> map3{120};
    expectEq(224uz, map3.capacity());  // 16 chunks × 14
  };

  return TestRegistry::result();
}
