#include <complex>

#include "matrix/storage/dense.h"
#include "matrix/storage/identity.h"
#include "matrix/kronecker_product.h"
#include "matrix/printer.h"

using namespace tempura::matrix;

auto main() -> int {
  Identity<2> I;

  Dense<double, {3, 3}> m {
    {8, 0., 0.}, {0., 2., 0.}, {0., 0., 1.}};
  std::cout << toString(KroneckerProduct{m, I});

  return 0;
}
