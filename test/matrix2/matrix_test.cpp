#include "matrix2/matrix.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Approx Equal"_test = [] {
    constexpr matrix::InlineDense m{
        {0., 1.},
        {2., 3.},
    };
    constexpr matrix::InlineDense n{
        {0.00001, 1.00001},
        {2.00001, 3.00001},
    };

    static_assert(approxEqual(m, n));
  };

  "Matrix Ref"_test = [] {
    static constexpr matrix::InlineDense m{
        {0., 1.},
        {2., 3.},
    };
    constexpr matrix::MatRef ref{m};
    static_assert(ref[0, 0] == 0.);
    static_assert(ref[0, 1] == 1.);
    static_assert(ref[1, 0] == 2.);
    static_assert(ref[1, 1] == 3.);
  };

  "Matrix Ref Assignment"_test = [] {
    constexpr auto out = [] {
      matrix::InlineDense m{
          {0., 1.},
          {2., 3.},
      };
      matrix::MatRef ref{m};
      m[0, 0] = 1.;
      return m;
    }();

    static_assert(out[0, 0] == 1.);
  };

  "Matrix Ref Shape"_test = [] {
    static constexpr matrix::InlineDense m{
        {0., 1.},
        {2., 3.},
    };
    constexpr matrix::MatRef ref{m};
    static_assert(ref.shape() == matrix::RowCol{.row = 2, .col = 2});
  };

  return TestRegistry::result();
}
