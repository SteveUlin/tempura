#include <complex>

#include "matrix/storage/dense.h"
#include "matrix/storage/identity.h"
#include "matrix/kronecker_product.h"
#include "matrix/printer.h"

using namespace tempura::matrix;

auto dft_matrix(int64_t size) {
  Dense<std::complex<double>, {kDynamic, kDynamic}> matrix{{size, size}};
  for (int64_t i = 0; i < size; ++i) {
    for (int64_t j = 0; j < size; ++j) {
      matrix[i, j] = std::polar(1., 2. * M_PI * i * j / size);
    }
  }
  return matrix;
}

auto main() -> int {
  Identity<2> I;

  Dense<double, {3, 3}> m {
    {8, 0., 0.}, {0., 2., 0.}, {0., 0., 1.}};
  std::cout << toString(dft_matrix(4));

  return 0;
}
