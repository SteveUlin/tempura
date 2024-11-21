#include "matrix2/matrix.h"

#include "matrix2/dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Approx Equal"_test = [] {
    constexpr matrix::Dense m{
        {0., 1.},
        {2., 3.},
    };
    constexpr matrix::Dense n{
        {0.00001, 1.00001},
        {2.00001, 3.00001},
    };

    static_assert(approxEqual(m, n));
  };

  return TestRegistry::result();
}
