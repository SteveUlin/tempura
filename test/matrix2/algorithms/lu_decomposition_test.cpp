#include "matrix2/algorithms/lu_decomposition.h"

#include "matrix2/multiplication.h"
#include "matrix2/storage/inline_dense.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Simple LU"_test = [] {
    constexpr double ϵ = 5. * std::numeric_limits<float>::epsilon();
    constexpr auto C = matrix::InlineDense<double, 3, 3>{
      {1., 1., 2.},
      {1., 2. + ϵ, 0.},
      {4., 14., 4.},
    };
    matrix::LU lu(C);
    auto b = C * matrix::InlineDense{{1.}, {2.}, {3.}};
    std::println("{}", matrix::toString(lu.matrix()));
    std::println("{}", matrix::toString(b));
    lu.solve(b);
    std::println("{}", matrix::toString(b));
  };
}
