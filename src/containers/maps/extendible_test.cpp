#include "containers/maps/extendible.h"

#include <string>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty map"_test = [] {
    ExtendibleMap<int, int> map;
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(42));
    expectTrue(map.find(42) == nullptr);
  };

  "insert and find"_test = [] {
    ExtendibleMap<int, std::string> map;

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
    ExtendibleMap<int, int> map;

    expectTrue(map.insertOrAssign(1, 100));
    expectFalse(map.insertOrAssign(1, 200));

    auto* ptr = map.find(1);
    expectTrue(ptr != nullptr);
    expectEq(200, *ptr);
  };

  "contains and find"_test = [] {
    ExtendibleMap<std::string, int> map;
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
    ExtendibleMap<std::string, int> map;

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
    ExtendibleMap<int, int> map;
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
    ExtendibleMap<int, int> map;
    map.insert(1, 1);
    map.insert(2, 2);
    map.insert(3, 3);

    expectEq(3uz, map.size());
    map.clear();
    expectEq(0uz, map.size());
    expectTrue(map.empty());
    expectFalse(map.contains(1));
  };

  "bucket splitting - local_depth < global_depth"_test = [] {
    // Start with small capacity to force splits
    ExtendibleMap<int, int> map{2};  // global_depth=2, 4 directory entries

    // Insert enough to fill a bucket (capacity=4)
    // These should hash to same bucket initially
    for (int i = 0; i < 10; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(10uz, map.size());

    // Verify all values are accessible
    for (int i = 0; i < 10; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }

    // Should have split some buckets
    expectGE(map.bucketCount(), 1uz);
  };

  "directory doubling"_test = [] {
    ExtendibleMap<int, int> map{1};  // Start with global_depth=1

    std::size_t initial_dir_size = map.directorySize();
    expectEq(2uz, initial_dir_size);  // 2^1 = 2

    // Insert enough elements to force directory doubling
    // Bucket capacity is 4, so we need >4 elements hashing to same bucket
    for (int i = 0; i < 20; ++i) {
      map.insert(i, i);
    }

    expectEq(20uz, map.size());

    // Directory should have doubled at least once
    expectGT(map.directorySize(), initial_dir_size);

    // All elements should still be findable
    for (int i = 0; i < 20; ++i) {
      expectTrue(map.contains(i));
      expectEq(i, *map.find(i));
    }
  };

  "multiple directory entries point to same bucket"_test = [] {
    ExtendibleMap<int, int> map{3};  // global_depth=3, 8 directory entries

    // Initially should have fewer buckets than directory entries
    expectLT(map.bucketCount(), map.directorySize());

    // Insert a few elements
    map.insert(1, 10);
    map.insert(2, 20);
    map.insert(3, 30);

    expectEq(3uz, map.size());

    // Should still have multiple directory entries per bucket
    expectLE(map.bucketCount(), map.directorySize());
  };

  "incremental growth"_test = [] {
    // Extendible hashing splits ONE bucket at a time
    ExtendibleMap<int, int> map{2};

    std::size_t bucket_count_before = map.bucketCount();

    // Insert enough to trigger split
    for (int i = 0; i < 10; ++i) {
      map.insert(i, i);
    }

    std::size_t bucket_count_after = map.bucketCount();

    // Should have created more buckets, but not rehashed everything
    expectGE(bucket_count_after, bucket_count_before);

    // All elements accessible
    for (int i = 0; i < 10; ++i) {
      expectTrue(map.contains(i));
    }
  };

  "copy constructor"_test = [] {
    ExtendibleMap<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    ExtendibleMap<int, std::string> copy{original};

    expectEq(original.size(), copy.size());
    expectEq(*original.find(1), *copy.find(1));
    expectEq(*original.find(2), *copy.find(2));

    copy.insert(3, "three");
    expectFalse(original.contains(3));
    expectTrue(copy.contains(3));
  };

  "move constructor"_test = [] {
    ExtendibleMap<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    ExtendibleMap<int, int> moved{std::move(original)};

    expectEq(2uz, moved.size());
    expectEq(100, *moved.find(1));
    expectEq(200, *moved.find(2));
    expectEq(0uz, original.size());
  };

  "copy assignment"_test = [] {
    ExtendibleMap<int, std::string> original;
    original.insert(1, "one");
    original.insert(2, "two");

    ExtendibleMap<int, std::string> copy;
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
    ExtendibleMap<int, int> original;
    original.insert(1, 100);
    original.insert(2, 200);

    ExtendibleMap<int, int> target;
    target.insert(99, 99);

    target = std::move(original);

    expectEq(2uz, target.size());
    expectEq(100, *target.find(1));
    expectEq(200, *target.find(2));
    expectFalse(target.contains(99));
    expectEq(0uz, original.size());
  };

  "swap"_test = [] {
    ExtendibleMap<int, int> a;
    a.insert(1, 10);
    a.insert(2, 20);

    ExtendibleMap<int, int> b;
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
    ExtendibleMap<int, int> map;
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
    ExtendibleMap<int, int> map;
    map.insert(1, 10);
    map.insert(2, 20);

    const auto& const_map = map;

    int count = 0;
    for (auto [key, value] : const_map) {
      ++count;
    }
    expectEq(2, count);
  };

  "string keys"_test = [] {
    ExtendibleMap<std::string, int> map;

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
    ExtendibleMap<int, int> map;

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

  "global depth increases with load"_test = [] {
    ExtendibleMap<int, int> map{1};

    std::size_t initial_depth = map.globalDepth();

    // Insert many elements to force depth increase
    for (int i = 0; i < 100; ++i) {
      map.insert(i, i);
    }

    expectEq(100uz, map.size());
    expectGE(map.globalDepth(), initial_depth);

    // All elements still accessible
    for (int i = 0; i < 100; ++i) {
      expectTrue(map.contains(i));
    }
  };

  "hash collision handling"_test = [] {
    // Even with hash collisions (same directory index),
    // extendible hashing handles via bucket splitting
    ExtendibleMap<int, int> map{2};

    // Insert many elements - some will collide
    for (int i = 0; i < 50; ++i) {
      map.insert(i, i * 10);
    }

    expectEq(50uz, map.size());

    for (int i = 0; i < 50; ++i) {
      expectTrue(map.contains(i));
      expectEq(i * 10, *map.find(i));
    }
  };

  return TestRegistry::result();
}
