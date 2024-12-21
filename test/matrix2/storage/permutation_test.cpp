#include "matrix2/storage/permutation.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Permutation - Default Constructor"_test = [] {
    constexpr matrix::Permutation<4> m{};
    constexpr matrix::InlineDense d{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1},
    };
    static_assert(m == d);
  };

  "Permutation - Copy Constructor"_test = [] {
    constexpr matrix::Permutation<4> m{3, 2, 1, 0};
    constexpr matrix::Permutation<4> n{m};
    static_assert(m == n);
  };

  "Permutation - Assignment Operator"_test = [] {
    constexpr matrix::Permutation<4> m{3, 2, 1, 0};
    constexpr matrix::Permutation<4> n = m;
    static_assert(m == n);
  };

  "Permutation - init list constructor"_test = [] {
    constexpr matrix::Permutation<4> m{3, 2, 1, 0};
    constexpr matrix::InlineDense d{
        {0, 0, 0, 1},
        {0, 0, 1, 0},
        {0, 1, 0, 0},
        {1, 0, 0, 0},
    };
    static_assert(m == d);
  };

  "Permutation - Swap"_test = [] {
    constexpr matrix::Permutation<4> m = [] {
      matrix::Permutation<4> m{};
      m.swap(0, 1);
      m.swap(2, 3);
      return m;
    }();

    constexpr matrix::InlineDense d{
        {0, 1, 0, 0},
        {1, 0, 0, 0},
        {0, 0, 0, 1},
        {0, 0, 1, 0},
    };
    static_assert(m == d);
  };

  "Permutaton - data"_test = [] {
    constexpr matrix::Permutation<4> m{};
    constexpr auto data = m.data();
    static_assert(data == std::array<int64_t, 4>{0, 1, 2, 3});

    constexpr matrix::Permutation<4> n{3, 2, 1, 0};
    static_assert(n.data() == std::array<int64_t, 4>{3, 2, 1, 0});
  };

  "Parity"_test = [] {
    constexpr matrix::Permutation<4> m{};
    static_assert(!m.parity());

    constexpr matrix::Permutation<4> n{1, 0, 2, 3};
    static_assert(n.parity());
  };

  "Permute"_test = [] {
    constexpr matrix::Permutation<4> m{3, 2, 1, 0};
    constexpr auto d = [&] {
      matrix::InlineDense d{
          {1, 2, 3, 4},
          {5, 6, 7, 8},
          {9, 10, 11, 12},
          {13, 14, 15, 16},
      };
      m.permuteRows(d);
      return d;
    }();
    static_assert(d == matrix::InlineDense{
      {13, 14, 15, 16},
      {9, 10, 11, 12},
      {5, 6, 7, 8},
      {1, 2, 3, 4},
    });
  };
}
