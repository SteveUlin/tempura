#include "meta/meta.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "MinimalArray"_test = [] {
    MinimalArray arr{1, 2, 3};
    expectEq(arr.size(), 3);
    expectEq(arr[0], 1);
    expectEq(arr[1], 2);
    expectEq(arr[2], 3);

    expectEq(arr.begin(), &arr.data[0]);
    expectEq(arr.end(), &arr.data[3]);
  };

  "MinimalArray Equality"_test = [] {
    MinimalArray arr1{1, 2, 3};
    MinimalArray arr2{1, 2, 3};
    MinimalArray arr3{4, 5, 6};

    expectEq(arr1 == arr2, true);
    expectEq(arr1 == arr3, false);
    expectEq(arr1 != arr3, true);
  };

  "MinimalArray Constexpr"_test = [] {
    constexpr MinimalArray arr{1, 2, 3};
    static_assert(arr.size() == 3);
    static_assert(arr[0] == 1);
    static_assert(arr[1] == 2);
    static_assert(arr[2] == 3);
  };

  "MinimalVector"_test = [] {
    MinimalVector<int> vec{1, 2, 3};
    expectEq(vec.size(), 3);
    expectEq(vec[0], 1);
    expectEq(vec[1], 2);
    expectEq(vec[2], 3);

    vec.push_back(4);
    expectEq(vec.size(), 4);
    expectEq(vec[3], 4);

    vec.emplace_back(5);
    expectEq(vec.size(), 5);
    expectEq(vec[4], 5);
  };

  "MinimalVector Equality"_test = [] {
    MinimalVector<int> vec1{1, 2, 3};
    MinimalVector<int> vec2{1, 2, 3};
    MinimalVector<int> vec3{4, 5, 6};

    expectEq(vec1 == vec2, true);
    expectEq(vec1 == vec3, false);
    expectEq(vec1 != vec3, true);
  };

  "MinimalVector Constexpr"_test = [] {
    constexpr MinimalVector<int> vec{1, 2, 3};
    static_assert(vec.size() == 3);
    static_assert(vec[0] == 1);
    static_assert(vec[1] == 2);
    static_assert(vec[2] == 3);
  };

  "MinimalVector clear"_test = [] {
    MinimalVector<int> vec{1, 2, 3};
    vec.clear();
    expectEq(vec.size(), 0);
  };

  "Heap Sort"_test = [] {
    MinimalVector<int> vec{5, 3, 8, 1, 4};
    heapSort(vec.begin(), vec.end(), [](int a, int b) { return a < b; });
    expectRangeEq(vec, MinimalVector<int>{1, 3, 4, 5, 8});
  };

  "constexpr Heap Sort"_test = [] {
    constexpr MinimalVector<int> vec{5, 3, 8, 1, 4};
    constexpr auto sorted = [](auto vector) {
      heapSort(vector.begin(), vector.end(),
               [](int a, int b) { return a < b; });
      return vector;
    }(vec);

    static_assert(sorted == MinimalVector<int>{1, 3, 4, 5, 8});
  };

  "constexpr sort fn"_test = [] {
    constexpr MinimalArray<int, 5> vec{5, 3, 8, 1, 4};

    static_assert(sort(vec) == MinimalArray{1, 3, 4, 5, 8});
  };

  return 0;
}
