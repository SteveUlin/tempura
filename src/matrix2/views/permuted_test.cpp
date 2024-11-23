#include "matrix2/views/permuted.h"

#include "matrix2/storage/inline_dense.h"
#include "matrix2/storage/permutation.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "RowPermuted - Default Constructor"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::RowPermuted r{d};
    static_assert(r == d);
  };

  "RowPermuted - Permutation Constructor"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::RowPermuted r{
        d, matrix::Permutation<(int64_t)4>{3, 2, 1, 0}};

    constexpr matrix::InlineDense expected{
        {13, 14, 15, 16},
        {9, 10, 11, 12},
        {5, 6, 7, 8},
        {1, 2, 3, 4},
    };
    static_assert(r == expected);
  };

  "RowPermuted - Swap"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::RowPermuted r = [] {
      matrix::RowPermuted r{d};
      r.swap(0, 1);
      r.swap(2, 3);
      return r;
    }();
    constexpr matrix::InlineDense expected{
        {5, 6, 7, 8},
        {1, 2, 3, 4},
        {13, 14, 15, 16},
        {9, 10, 11, 12},
    };
    static_assert(r == expected);
  };

  "ColPermuted - Default Constructor"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::ColPermuted c{d};
    static_assert(c == d);
  };

  "ColPermuted - Swap"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::ColPermuted c = [] {
      matrix::ColPermuted c{d};
      c.swap(0, 1);
      c.swap(2, 3);
      return c;
    }();
    constexpr matrix::InlineDense expected{
        {2, 1, 4, 3},
        {6, 5, 8, 7},
        {10, 9, 12, 11},
        {14, 13, 16, 15},
    };
    static_assert(c == expected);
  };

  "ColPermuted - Permutation Constructor"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::ColPermuted c{
        d, matrix::Permutation<(int64_t)4>{3, 2, 1, 0}};
    constexpr matrix::InlineDense expected{
        {4, 3, 2, 1},
        {8, 7, 6, 5},
        {12, 11, 10, 9},
        {16, 15, 14, 13},
    };
    static_assert(c == expected);
  };

  "ColPermuted - Swap"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 2, 3, 4},
        {5, 6, 7, 8},
        {9, 10, 11, 12},
        {13, 14, 15, 16},
    };
    constexpr matrix::ColPermuted c = [] {
      matrix::ColPermuted c{d};
      c.swap(0, 1);
      c.swap(2, 3);
      return c;
    }();
    constexpr matrix::InlineDense expected{
        {2, 1, 4, 3},
        {6, 5, 8, 7},
        {10, 9, 12, 11},
        {14, 13, 16, 15},
    };
    static_assert(c == expected);
  };
}
