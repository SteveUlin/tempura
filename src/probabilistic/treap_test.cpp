#include "probabilistic/treap.h"

#include <cmath>
#include <string>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty treap"_test = [] {
    Treap<int> treap;
    expectEq(treap.size(), 0uz);
    expectTrue(treap.empty());
    expectFalse(treap.contains(42));
    expectEq(treap.find(42), treap.end());
    expectEq(treap.begin(), treap.end());
  };

  "insert and find"_test = [] {
    Treap<int> treap;

    expectTrue(treap.insert(5));
    expectTrue(treap.insert(10));
    expectTrue(treap.insert(3));
    expectTrue(treap.insert(7));

    expectEq(treap.size(), 4uz);
    expectTrue(treap.contains(5));
    expectTrue(treap.contains(10));
    expectTrue(treap.contains(3));
    expectTrue(treap.contains(7));
    expectFalse(treap.contains(1));
    expectFalse(treap.contains(100));

    // Find returns valid iterator
    auto it = treap.find(7);
    expectNeq(it, treap.end());
    expectEq(*it, 7);
  };

  "duplicate insert returns false"_test = [] {
    Treap<int> treap;

    expectTrue(treap.insert(5));
    expectFalse(treap.insert(5));  // Duplicate
    expectEq(treap.size(), 1uz);

    // Original still there
    expectTrue(treap.contains(5));
  };

  "erase"_test = [] {
    Treap<int> treap;
    treap.insert(1);
    treap.insert(2);
    treap.insert(3);
    treap.insert(4);
    treap.insert(5);

    expectTrue(treap.erase(2));
    expectEq(treap.size(), 4uz);
    expectFalse(treap.contains(2));
    expectTrue(treap.contains(1));
    expectTrue(treap.contains(3));

    expectFalse(treap.erase(2));   // Already removed
    expectFalse(treap.erase(100)); // Never existed

    // Erase root, first, last
    expectTrue(treap.erase(1));
    expectTrue(treap.erase(5));
    expectEq(treap.size(), 2uz);
  };

  "iteration order"_test = [] {
    Treap<int> treap;
    treap.insert(5);
    treap.insert(1);
    treap.insert(9);
    treap.insert(3);
    treap.insert(7);

    std::vector<int> result;
    for (int x : treap) {
      result.push_back(x);
    }

    // In-order traversal should be sorted
    std::vector<int> expected{1, 3, 5, 7, 9};
    expectRangeEq(result, expected);
  };

  "initializer list"_test = [] {
    Treap<int> treap{5, 1, 9, 3, 7};

    std::vector<int> result;
    for (int x : treap) {
      result.push_back(x);
    }

    std::vector<int> expected{1, 3, 5, 7, 9};
    expectRangeEq(result, expected);
  };

  "lower and upper bound"_test = [] {
    Treap<int> treap{10, 20, 30, 40, 50};

    // lowerBound: first element >= key
    expectEq(*treap.lowerBound(20), 20);
    expectEq(*treap.lowerBound(25), 30);
    expectEq(*treap.lowerBound(10), 10);
    expectEq(*treap.lowerBound(5), 10);
    expectEq(treap.lowerBound(55), treap.end());

    // upperBound: first element > key
    expectEq(*treap.upperBound(20), 30);
    expectEq(*treap.upperBound(25), 30);
    expectEq(*treap.upperBound(10), 20);
    expectEq(*treap.upperBound(5), 10);
    expectEq(treap.upperBound(50), treap.end());
  };

  "clear"_test = [] {
    Treap<int> treap{1, 2, 3, 4, 5};
    expectEq(treap.size(), 5uz);

    treap.clear();
    expectEq(treap.size(), 0uz);
    expectTrue(treap.empty());
    expectEq(treap.begin(), treap.end());

    // Can insert again after clear
    treap.insert(42);
    expectEq(treap.size(), 1uz);
    expectTrue(treap.contains(42));
  };

  "move constructor"_test = [] {
    Treap<int> treap1{1, 2, 3};
    Treap<int> treap2{std::move(treap1)};

    expectEq(treap2.size(), 3uz);
    expectTrue(treap2.contains(1));
    expectTrue(treap2.contains(2));
    expectTrue(treap2.contains(3));

    // Moved-from treap should be empty but valid
    expectTrue(treap1.empty());
    expectEq(treap1.begin(), treap1.end());
  };

  "move assignment"_test = [] {
    Treap<int> treap1{1, 2, 3};
    Treap<int> treap2{10, 20};

    treap2 = std::move(treap1);

    expectEq(treap2.size(), 3uz);
    expectTrue(treap2.contains(1));
    expectFalse(treap2.contains(10));

    expectTrue(treap1.empty());
  };

  "many elements - height is O(log n)"_test = [] {
    Treap<int> treap;

    // Insert 1000 elements in worst-case order for a naive BST (sorted)
    for (int i = 0; i < 1000; ++i) {
      treap.insert(i);
    }

    // All elements should be present
    for (int i = 0; i < 1000; ++i) {
      expectTrue(treap.contains(i));
    }

    // Should be in sorted order
    int prev = -1;
    for (int x : treap) {
      expectGT(x, prev);
      prev = x;
    }

    // Random priorities should give O(log n) height
    // For n=1000, log2(1000) ~ 10, so height should be reasonable
    // Allow up to 50 for statistical variance (still way better than 1000)
    std::size_t h = treap.height();
    expectLT(h, 50uz);

    // Erase all even numbers
    for (int i = 0; i < 1000; i += 2) {
      expectTrue(treap.erase(i));
    }
    expectEq(treap.size(), 500uz);
  };

  "string keys"_test = [] {
    Treap<std::string> treap;
    treap.insert("banana");
    treap.insert("apple");
    treap.insert("cherry");
    treap.insert("date");

    expectTrue(treap.contains("apple"));
    expectTrue(treap.contains("banana"));
    expectTrue(treap.contains("cherry"));
    expectTrue(treap.contains("date"));
    expectFalse(treap.contains("elderberry"));

    // Check sorted order
    std::vector<std::string> result;
    for (const auto& s : treap) {
      result.push_back(s);
    }
    std::vector<std::string> expected{"apple", "banana", "cherry", "date"};
    expectRangeEq(result, expected);

    // Erase and verify
    expectTrue(treap.erase("banana"));
    expectFalse(treap.contains("banana"));
    expectEq(treap.size(), 3uz);
  };

  "single element operations"_test = [] {
    Treap<int> treap;

    treap.insert(42);
    expectEq(treap.size(), 1uz);
    expectTrue(treap.contains(42));
    expectEq(*treap.begin(), 42);
    expectEq(*treap.find(42), 42);

    treap.erase(42);
    expectTrue(treap.empty());
    expectEq(treap.begin(), treap.end());
  };

  "iterator decrement"_test = [] {
    Treap<int> treap{1, 2, 3, 4, 5};

    // Start at 5 (rightmost)
    auto it = treap.find(5);
    expectEq(*it, 5);

    --it;
    expectEq(*it, 4);
    --it;
    expectEq(*it, 3);
    --it;
    expectEq(*it, 2);
    --it;
    expectEq(*it, 1);
  };

  return TestRegistry::result();
}
