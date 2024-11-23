#include "matrix2/views/permuted.h"

#include "matrix2/storage/inline_dense.h"
#include "matrix2/storage/permutation.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "RowPermuted - Default Constructor"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
    constexpr matrix::RowPermuted r{d};
    static_assert(r == d);
  };

  "RowPermuted - Permutation Constructor"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
    constexpr matrix::RowPermuted r{
        d, matrix::Permutation<(int64_t)4>{3, 2, 1, 0}};

    constexpr matrix::InlineDense expected{
        {0, 0, 0, 1},
        {0, 0, 1, 0},
        {0, 1, 0, 0},
        {1, 0, 0, 0},
    };
    static_assert(r == expected);
  };

  "RowPermuted - Swap"_test = [] {
    static constexpr matrix::InlineDense d{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
    constexpr matrix::RowPermuted r = [] {
      matrix::RowPermuted r{d};
      r.swap(0, 1);
      r.swap(2, 3);
      return r;
    }();
    static_assert(r == matrix::InlineDense{
                           {0, 1, 0, 0},
                           {1, 0, 0, 0},
                           {0, 0, 0, 1},
                           {0, 0, 1, 0},
                       });
  };
}
