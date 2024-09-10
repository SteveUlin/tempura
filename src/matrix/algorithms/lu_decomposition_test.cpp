#include "matrix/algorithms/lu_decomposition.h"

#include "matrix/storage/dense.h"
#include "matrix/multiplication.h"
#include "matrix/printer.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "LU"_test = [] {
    float ϵ = 5.F * std::numeric_limits<float>::epsilon();
    auto C = matrix::Dense<float, {3, 3}>{
        {1., 1., 2.}, {1., 2.F + ϵ, 0}, {4, 14, 4}};
    auto b = C * matrix::Dense<float, {3, 1}>{std::vector{1., 2., 3.}};
    matrix::LU<matrix::Dense<float, {3, 3}>, matrix::Pivoting::None> lu{C};
    std::cout << toString(lu.matrix()) << std::endl;
    std::cout << toString(b) << std::endl;
    std::cout << toString(lu.solve(b)) << std::endl;
  };
  return 0;
}
