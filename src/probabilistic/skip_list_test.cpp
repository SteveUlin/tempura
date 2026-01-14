#include "probabilistic/skip_list.h"

#include <algorithm>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty skip list"_test = [] {
    SkipList<int> list;
    expectEq(list.size(), 0uz);
    expectTrue(list.empty());
    expectFalse(list.contains(42));
    expectEq(list.find(42), list.end());
  };

  "insert and find"_test = [] {
    SkipList<int> list;

    expectTrue(list.insert(5));
    expectTrue(list.insert(10));
    expectTrue(list.insert(3));
    expectTrue(list.insert(7));

    expectEq(list.size(), 4uz);
    expectTrue(list.contains(5));
    expectTrue(list.contains(10));
    expectTrue(list.contains(3));
    expectTrue(list.contains(7));
    expectFalse(list.contains(1));
    expectFalse(list.contains(100));
  };

  "duplicate insert returns false"_test = [] {
    SkipList<int> list;

    expectTrue(list.insert(5));
    expectFalse(list.insert(5));  // Duplicate
    expectEq(list.size(), 1uz);
  };

  "erase"_test = [] {
    SkipList<int> list;
    list.insert(1);
    list.insert(2);
    list.insert(3);

    expectTrue(list.erase(2));
    expectEq(list.size(), 2uz);
    expectFalse(list.contains(2));
    expectTrue(list.contains(1));
    expectTrue(list.contains(3));

    expectFalse(list.erase(2));  // Already removed
    expectFalse(list.erase(100));  // Never existed
  };

  "iteration order"_test = [] {
    SkipList<int> list;
    list.insert(5);
    list.insert(1);
    list.insert(9);
    list.insert(3);
    list.insert(7);

    std::vector<int> result;
    for (int x : list) {
      result.push_back(x);
    }

    std::vector<int> expected{1, 3, 5, 7, 9};
    expectRangeEq(result, expected);
  };

  "initializer list"_test = [] {
    SkipList<int> list{5, 1, 9, 3, 7};

    std::vector<int> result;
    for (int x : list) {
      result.push_back(x);
    }

    std::vector<int> expected{1, 3, 5, 7, 9};
    expectRangeEq(result, expected);
  };

  "lower and upper bound"_test = [] {
    SkipList<int> list{10, 20, 30, 40, 50};

    // lowerBound: first element >= key
    expectEq(*list.lowerBound(20), 20);
    expectEq(*list.lowerBound(25), 30);
    expectEq(*list.lowerBound(10), 10);
    expectEq(*list.lowerBound(5), 10);
    expectEq(list.lowerBound(55), list.end());

    // upperBound: first element > key
    expectEq(*list.upperBound(20), 30);
    expectEq(*list.upperBound(25), 30);
    expectEq(*list.upperBound(10), 20);
    expectEq(*list.upperBound(5), 10);
    expectEq(list.upperBound(50), list.end());
  };

  "clear"_test = [] {
    SkipList<int> list{1, 2, 3, 4, 5};
    expectEq(list.size(), 5uz);

    list.clear();
    expectEq(list.size(), 0uz);
    expectTrue(list.empty());
    expectEq(list.begin(), list.end());

    // Can insert again after clear
    list.insert(42);
    expectEq(list.size(), 1uz);
    expectTrue(list.contains(42));
  };

  "move constructor"_test = [] {
    SkipList<int> list1{1, 2, 3};
    SkipList<int> list2{std::move(list1)};

    expectEq(list2.size(), 3uz);
    expectTrue(list2.contains(1));
    expectTrue(list2.contains(2));
    expectTrue(list2.contains(3));

    // Moved-from list should be empty but valid
    expectTrue(list1.empty());
  };

  "move assignment"_test = [] {
    SkipList<int> list1{1, 2, 3};
    SkipList<int> list2{10, 20};

    list2 = std::move(list1);

    expectEq(list2.size(), 3uz);
    expectTrue(list2.contains(1));
    expectFalse(list2.contains(10));

    expectTrue(list1.empty());
  };

  "many elements"_test = [] {
    SkipList<int> list;

    // Insert 1000 elements in random-ish order
    for (int i = 0; i < 1000; ++i) {
      list.insert((i * 37) % 1000);
    }

    // All elements should be present
    for (int i = 0; i < 1000; ++i) {
      expectTrue(list.contains(i));
    }

    // Should be in sorted order
    int prev = -1;
    for (int x : list) {
      expectGT(x, prev);
      prev = x;
    }
  };

  "string keys"_test = [] {
    SkipList<std::string> list;
    list.insert("banana");
    list.insert("apple");
    list.insert("cherry");

    expectTrue(list.contains("apple"));
    expectTrue(list.contains("banana"));
    expectTrue(list.contains("cherry"));
    expectFalse(list.contains("date"));

    // Check sorted order
    std::vector<std::string> result;
    for (const auto& s : list) {
      result.push_back(s);
    }
    std::vector<std::string> expected{"apple", "banana", "cherry"};
    expectRangeEq(result, expected);
  };

  return TestRegistry::result();
}
