#include "matrix2/storage/permutation.h"

#include "matrix2/storage/dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Permutation - Default Constructor"_test = [] {
    constexpr matrix::Permutation<4> m{};
    constexpr matrix::Dense d{
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
    constexpr matrix::Dense d{
        {0, 0, 0, 1},
        {0, 0, 1, 0},
        {0, 1, 0, 0},
        {1, 0, 0, 0},
    };
    static_assert(m == d);
  };

  "Permutation - init list assignment"_test = [] {
    constexpr matrix::Permutation<4> m = {3, 2, 1, 0};
    constexpr matrix::Permutation<4> n = m;
    constexpr matrix::Dense d{
        {0, 0, 0, 1},
        {0, 0, 1, 0},
        {0, 1, 0, 0},
        {1, 0, 0, 0},
    };
    static_assert(n == d);
  };

  "Permutation - Swap"_test = [] {
    constexpr matrix::Permutation<4> m = [] {
      matrix::Permutation<4> m{};
      m.swap(0, 1);
      m.swap(2, 3);
      return m;
    }();

    constexpr matrix::Dense d{
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
}
