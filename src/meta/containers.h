#pragma once

#include "meta/utility.h"
#include "tags.h"

// Small utility containers to use in meta programming to avoid import times of
// stl containers.

namespace tempura {

template <typename T, SizeT N>
struct MinimalArray {
  constexpr MinimalArray() = default;
  constexpr MinimalArray(const T& t) {
    for (SizeT i = 0; i < N; ++i) {
      data[i] = t;
    }
  }
  template <typename... Ts>
  constexpr MinimalArray(const T& t, Ts... ts)
    requires((requires { T(ts); }) and ...)
      : data{t, ts...} {
    static_assert(sizeof...(ts) + 1 == N);
  }

  constexpr auto begin(this auto&& self) { return &self.data[0]; }

  constexpr auto end(this auto&& self) { return &self.data[N]; }

  constexpr auto operator[](SizeT i) const -> const T& { return data[i]; }

  constexpr auto operator[](SizeT i) -> T& { return data[i]; }

  static constexpr auto size() -> SizeT { return N; }

  constexpr auto operator==(const MinimalArray& other) const {
    for (SizeT i = 0; i < N; ++i) {
      if (data[i] != other.data[i]) return false;
    }
    return true;
  }

  constexpr auto operator!=(const MinimalArray& other) const {
    return not(*this == other);
  }

  T data[N];
};
static_assert(MinimalArray<int, 3>{1, 2, 3}[0] == 1);
constexpr auto arr = [] {
  MinimalArray<int, 3> arr{1, 2, 3};
  arr[0] = 4;
  return arr;
}();
static_assert(arr[0] == 4);

template <typename T, SizeT... Ns>
constexpr auto join(MinimalArray<T, Ns>... arrs)
    -> MinimalArray<T, (Ns + ... + 0)> {
  MinimalArray<T, (Ns + ... + 0)> out;
  SizeT j = 0;
  (([&out, &arrs, &j] {
     for (SizeT i = 0; i < arrs.size(); ++i) {
       out[j++] = arrs[i];
     }
   }()),
   ...);
  return out;
}
static_assert(join(MinimalArray<int, 3>{1, 2, 3}, MinimalArray<int, 2>{4, 5}) ==
              MinimalArray<int, 5>{1, 2, 3, 4, 5});

template <typename T, typename... Ts>
MinimalArray(T, Ts...) -> MinimalArray<T, sizeof...(Ts) + 1>;

template <typename T>
class MinimalVector {
 public:
  constexpr static SizeT kCapacity = 1024;

  constexpr MinimalVector() = default;

  constexpr explicit MinimalVector(SizeT size) : size_(size) {}

  constexpr explicit MinimalVector(const auto&... ts)
    requires((requires { T(ts); }) and ...)
      : data_{ts...}, size_{sizeof...(ts)} {
    static_assert(sizeof...(ts) <= kCapacity);
  }

  constexpr void push_back(const T& t) { data_[size_++] = t; }

  constexpr void emplace_back(T&& t) { data_[size_++] = static_cast<T&&>(t); }

  constexpr auto begin(this auto&& self) { return &self.data_[0]; }

  constexpr auto end(this auto&& self) { return &self.data_[self.size_]; }

  constexpr auto operator[](this auto&& self, SizeT i) -> decltype(auto) {
    return self.data_[i];
  }

  constexpr auto size() const -> SizeT { return size_; }

  constexpr auto capacity() const -> SizeT { return kCapacity; }

  constexpr auto operator==(const MinimalVector& other) const -> bool {
    if (size_ != other.size_) return false;
    for (SizeT i = 0; i < size_; ++i) {
      if (data_[i] != other.data_[i]) return false;
    }
    return true;
  }

  constexpr auto operator!=(const MinimalVector& other) const -> bool {
    return not(*this == other);
  }

  constexpr void clear() { size_ = 0; }

 private:
  union {
    T data_[kCapacity] = {};
  };

  SizeT size_ = 0;
};

// Heap Sort
//
// A heap node has the property that it is greater than or equal to its
// children.

// Sift the element at idx down the heap
template <typename RandomAccessIterator, typename Compare>
constexpr void heapSiftDown(RandomAccessIterator first,
                            RandomAccessIterator last, SizeT idx,
                            Compare comp) {
  const SizeT size = last - first;
  while (true) {
    SizeT left = 2 * idx + 1;
    // No Children
    if (left >= size) break;

    SizeT right = left + 1;
    // Assume left is the largest
    SizeT largest = left;
    if (right < size and comp(first[left], first[right])) {
      largest = right;
    }
    if (comp(first[idx], first[largest])) {
      swap(first[idx], first[largest]);
      idx = largest;
    } else {
      break;
    }
  }
}

// Heap Sort
template <typename RandomAccessIterator, typename Compare>
constexpr void heapSort(RandomAccessIterator first, RandomAccessIterator last,
                        Compare comp) {
  const SizeT size = last - first;
  if (size <= 1) return;

  // Build the heap
  // The parent of the last element is at size / 2 - 1, but we need to
  // offset by one to avoid SizeT underflow
  for (SizeT i = size / 2; i-- > 0;) {
    heapSiftDown(first, last, i, comp);
  }
  // Sort the heap
  for (SizeT i = size - 1; i > 0; --i) {
    swap(first[0], first[i]);
    heapSiftDown(first, first + i, 0, comp);
  }
}

template <typename T, SizeT N>
constexpr auto sort(
    MinimalArray<T, N> range) {
  auto comp = [](const T& a, const T& b) { return a < b; };
  heapSort(range.begin(), range.end(), comp);
  return range;
}

template <typename T>
constexpr auto sort(
    MinimalVector<T> range) {
  auto comp = [](const T& a, const T& b) { return a < b; };
  heapSort(range.begin(), range.end(), comp);
  return range;
}

}  // namespace tempura
