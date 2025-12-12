#include "matrix3/inline_coordinate_list.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "Empty construction"_test = [] {
    constexpr InlineCoordinateList<int, 3, 3> mat{};
    static_assert(mat.rows() == 3);
    static_assert(mat.cols() == 3);
    static_assert(mat.size() == 0);
    static_assert(mat[0, 0] == 0);
    static_assert(mat[1, 2] == 0);
    static_assert(mat[2, 2] == 0);
  };

  "Basic insert and lookup"_test = [] {
    constexpr auto mat = [] consteval {
      InlineCoordinateList<int, 3, 3, 10> m{};  // Explicit capacity
      m.insert(0, 1, 42);
      m.insert(2, 0, -7);
      m.insert(1, 1, 99);
      return m;
    }();

    static_assert(mat.size() == 3);
    static_assert(mat[0, 1] == 42);
    static_assert(mat[2, 0] == -7);
    static_assert(mat[1, 1] == 99);
    // Missing elements return zero
    static_assert(mat[0, 0] == 0);
    static_assert(mat[0, 2] == 0);
    static_assert(mat[2, 2] == 0);
  };

  "Missing element returns zero"_test = [] {
    InlineCoordinateList<double, 5, 5> mat{};
    mat.insert(1, 2, 3.14);
    mat.insert(3, 4, 2.71);

    expectEq(mat[1, 2], 3.14);
    expectEq(mat[3, 4], 2.71);
    expectEq(mat[0, 0], 0.0);
    expectEq(mat[2, 2], 0.0);
    expectEq(mat[4, 4], 0.0);
  };

  "Last insert wins"_test = [] {
    constexpr auto mat = [] consteval {
      InlineCoordinateList<int, 3, 3, 10> m{};  // Explicit capacity
      m.insert(1, 1, 10);
      m.insert(1, 1, 20);
      m.insert(1, 1, 30);
      return m;
    }();

    // Reverse linear search finds most recent value
    static_assert(mat[1, 1] == 30);
    // Size includes all inserts (no deduplication)
    static_assert(mat.size() == 3);
  };

  "Capacity limit"_test = [] {
    InlineCoordinateList<int, 3, 3, 2> mat{};  // Capacity of only 2

    expectTrue(mat.insert(0, 0, 1));
    expectTrue(mat.insert(0, 1, 2));
    expectFalse(mat.insert(0, 2, 3));  // Should fail - capacity exceeded

    expectEq(mat.size(), 2);
    expectEq(mat.capacity(), 2);
    expectEq(mat[0, 0], 1);
    expectEq(mat[0, 1], 2);
    expectEq(mat[0, 2], 0);  // Failed insert, returns zero
  };

  "Triplet construction"_test = [] {
    using Triplet = InlineCoordinateList<int, 4, 4>::Triplet;
    std::array triplets = {
        Triplet{0, 0, 1},
        Triplet{1, 1, 2},
        Triplet{2, 2, 3},
        Triplet{3, 3, 4},
    };

    InlineCoordinateList<int, 4, 4> mat{triplets};

    expectEq(mat.size(), 4);
    expectEq(mat[0, 0], 1);
    expectEq(mat[1, 1], 2);
    expectEq(mat[2, 2], 3);
    expectEq(mat[3, 3], 4);
    expectEq(mat[0, 1], 0);
    expectEq(mat[1, 0], 0);
  };

  "Triplet insert"_test = [] {
    InlineCoordinateList<int, 3, 3> mat{};
    using Triplet = decltype(mat)::Triplet;

    mat.insert(Triplet{0, 1, 5});
    mat.insert(Triplet{2, 0, 7});

    expectEq(mat.size(), 2);
    expectEq(mat[0, 1], 5);
    expectEq(mat[2, 0], 7);
  };

  "Sparse pattern"_test = [] {
    // Test a truly sparse matrix where most elements are zero
    InlineCoordinateList<int, 100, 100, 5> mat{};

    mat.insert(0, 0, 1);
    mat.insert(50, 50, 2);
    mat.insert(99, 99, 3);
    mat.insert(25, 75, 4);
    mat.insert(75, 25, 5);

    expectEq(mat.size(), 5);
    expectEq(mat[0, 0], 1);
    expectEq(mat[50, 50], 2);
    expectEq(mat[99, 99], 3);
    expectEq(mat[25, 75], 4);
    expectEq(mat[75, 25], 5);

    // Verify most elements are still zero
    expectEq(mat[0, 1], 0);
    expectEq(mat[1, 0], 0);
    expectEq(mat[10, 10], 0);
    expectEq(mat[50, 51], 0);
  };

  "constexpr with different types"_test = [] {
    // Test with double
    constexpr auto mat_d = [] consteval {
      InlineCoordinateList<double, 2, 2, 10> m{};  // Explicit capacity
      m.insert(0, 0, 1.5);
      m.insert(1, 1, 2.5);
      return m;
    }();
    static_assert(mat_d[0, 0] == 1.5);
    static_assert(mat_d[1, 1] == 2.5);

    // Test with int64_t
    constexpr auto mat_i64 = [] consteval {
      InlineCoordinateList<int64_t, 2, 2, 10> m{};  // Explicit capacity
      m.insert(0, 1, 1000000000000LL);
      return m;
    }();
    static_assert(mat_i64[0, 1] == 1000000000000LL);
  };

  "Iteration over empty matrix"_test = [] {
    constexpr InlineCoordinateList<int, 3, 3> mat{};
    static_assert(mat.begin() == mat.end());

    // Runtime check
    InlineCoordinateList<int, 3, 3> mat_rt{};
    int count = 0;
    for (auto entry : mat_rt) {
      (void)entry;  // Suppress unused variable warning
      ++count;
    }
    expectEq(count, 0);
  };

  "Iteration over single entry"_test = [] {
    constexpr auto mat = [] consteval {
      InlineCoordinateList<int, 3, 3, 10> m{};
      m.insert(1, 2, 42);
      return m;
    }();

    // constexpr iteration
    constexpr auto first = *mat.begin();
    static_assert(first.row == 1);
    static_assert(first.col == 2);
    static_assert(first.value == 42);

    // Runtime iteration
    InlineCoordinateList<int, 3, 3> mat_rt{};
    mat_rt.insert(1, 2, 42);

    int count = 0;
    for (auto entry : mat_rt) {
      expectEq(entry.row, 1);
      expectEq(entry.col, 2);
      expectEq(entry.value, 42);
      ++count;
    }
    expectEq(count, 1);
  };

  "Iteration over multiple entries"_test = [] {
    InlineCoordinateList<int, 5, 5> mat{};
    mat.insert(0, 1, 10);
    mat.insert(1, 2, 20);
    mat.insert(2, 3, 30);
    mat.insert(3, 4, 40);

    // Collect entries
    std::array<InlineCoordinateList<int, 5, 5>::Entry, 4> entries{};
    std::size_t idx = 0;
    for (auto entry : mat) {
      entries[idx++] = entry;
    }

    // Verify entries (in insertion order)
    expectEq(entries[0].row, 0);
    expectEq(entries[0].col, 1);
    expectEq(entries[0].value, 10);

    expectEq(entries[1].row, 1);
    expectEq(entries[1].col, 2);
    expectEq(entries[1].value, 20);

    expectEq(entries[2].row, 2);
    expectEq(entries[2].col, 3);
    expectEq(entries[2].value, 30);

    expectEq(entries[3].row, 3);
    expectEq(entries[3].col, 4);
    expectEq(entries[3].value, 40);

    expectEq(idx, 4);
  };

  "Iteration includes duplicates"_test = [] {
    // When the same (row, col) is inserted multiple times,
    // iteration includes all entries (even though lookup returns last)
    InlineCoordinateList<int, 3, 3, 10> mat{};  // Explicit capacity
    mat.insert(1, 1, 10);
    mat.insert(1, 1, 20);
    mat.insert(1, 1, 30);

    int count = 0;
    std::array<int, 3> values{};
    for (auto entry : mat) {
      expectEq(entry.row, 1);
      expectEq(entry.col, 1);
      values[count] = entry.value;
      ++count;
    }

    expectEq(count, 3);
    expectEq(values[0], 10);
    expectEq(values[1], 20);
    expectEq(values[2], 30);

    // But lookup returns the last
    expectEq(mat[1, 1], 30);
  };

  "constexpr iteration"_test = [] {
    constexpr auto mat = [] consteval {
      InlineCoordinateList<int, 3, 3, 10> m{};
      m.insert(0, 0, 1);
      m.insert(1, 1, 2);
      m.insert(2, 2, 3);
      return m;
    }();

    // Count entries at compile time
    constexpr auto count = [&mat] consteval {
      int n = 0;
      for (auto it = mat.begin(); it != mat.end(); ++it) {
        ++n;
      }
      return n;
    }();
    static_assert(count == 3);

    // Sum values at compile time
    constexpr auto sum = [&mat] consteval {
      int s = 0;
      for (auto entry : mat) {
        s += entry.value;
      }
      return s;
    }();
    static_assert(sum == 6);
  };

  "Range-based for with different types"_test = [] {
    InlineCoordinateList<double, 4, 4> mat{};
    mat.insert(0, 1, 1.5);
    mat.insert(1, 2, 2.5);
    mat.insert(2, 3, 3.5);

    double sum = 0.0;
    for (auto entry : mat) {
      sum += entry.value;
    }
    expectEq(sum, 7.5);
  };

  return TestRegistry::result();
}
