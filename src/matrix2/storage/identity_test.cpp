#include "matrix2/storage/identity.h"

#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Identity - Default Constructor"_test = [] {
    constexpr matrix::Identity<4> m{};
    static_assert(m[0, 0]);
    static_assert(m[1, 1]);
    static_assert(m[2, 2]);
    static_assert(m[3, 3]);
    static_assert(!m[0, 1]);
  };

  "Identity - Compare to InlineDense"_test = [] {
    constexpr matrix::Identity<4> m{};
    constexpr matrix::InlineDense<double, 4, 4> n{
        {1., 0., 0., 0.},
        {0., 1., 0., 0.},
        {0., 0., 1., 0.},
        {0., 0., 0., 1.},
    };
    static_assert(m == n);
  };

  "Identity - Shape"_test = [] {
    constexpr matrix::Identity<4> m{};
    static_assert(m.shape() == matrix::RowCol{.row = 4, .col = 4});
  };

  "DynamicIdentity Constructor"_test = [] {
    constexpr matrix::DynamicIdentity m{1};
    static_assert(m[0, 0]);
    static_assert(m[1, 1]);
    static_assert(m[2, 2]);
    static_assert(m[3, 3]);
    static_assert(!m[0, 1]);
  };

  "DynamicIdentity - Shape"_test = [] {
    constexpr matrix::DynamicIdentity m{4};
    static_assert(m.shape() == matrix::RowCol{.row = 4, .col = 4});
  };

  "Compare Identity to DynamicIdentity"_test = [] {
    constexpr matrix::Identity<4> m{};
    constexpr matrix::DynamicIdentity n{4};
    static_assert(m == n);
    static_assert(n == m);
  };

  return TestRegistry::result();
}
