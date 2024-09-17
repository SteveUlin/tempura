#include <bit>
#include <complex>

#include "matrix/kronecker_product.h"
#include "matrix/printer.h"
#include "matrix/storage/dense.h"
#include "matrix/storage/identity.h"
#include "matrix/storage/permutation.h"
#include "matrix/multiplication.h"

using namespace tempura::matrix;

auto dft_matrix(int64_t size) {
  Dense<std::complex<double>, {kDynamic, kDynamic}> matrix{{size, size}};
  for (int64_t i = 0; i < size; ++i) {
    for (int64_t j = 0; j < size; ++j) {
      matrix[i, j] = std::polar(1., 2. * std::numbers::pi * i * j / size);
    }
  }
  return matrix;
}

auto fft_radix2(MatrixT auto& m) {
  const int64_t row = m.shape().row;
  const int64_t col = m.shape().col;
  // Is power of 2
  CHECK((row & (row - 1)) == 0);
  Permutation<kDynamic> perm(row);
  for (int64_t delta = row; delta > 1; delta /= 2) {
    std::vector<int64_t> curr(row);
    for (int64_t start = 0; start < row; start += delta) {
      for (int64_t i = 0; i < delta / 2; ++i) {
        curr[start + i] = start + i * 2;
        curr[start + delta / 2 + i] = start + i * 2 + 1;
      }
    }
    Permutation{std::move(curr)}.permute(perm);
  }
  std::cout << toString(m) << std::endl;
  perm.permute(m.rows());
  std::cout << toString(m);

  for (int64_t delta = 2; delta <= row; delta *= 2) {
    for (int64_t start = 0; start < row; start += delta) {
      Dense<std::complex<double>, {kDynamic, kDynamic}> lhs =
          Slice{{delta/2, col}, {start, 0}, m};
      Dense<std::complex<double>, {kDynamic, kDynamic}> rhs =
          Slice{{delta/2, col}, {start/2, 0}, m};
      for (int i = 0; i < delta / 2; ++i) {
        std::cout << "------------" << std::endl;
        std::cout << toString(rhs) << '\n';
        std::complex<double> omega =
            std::polar(1.0, i * 2.0 * std::numbers::pi / static_cast<double>(delta));
        std::cout << "OMEGA: " << omega << '\n';
        Slice{{1, col}, {i, 0}, rhs} *= omega;
        std::cout << toString(rhs) << '\n';
        std::cout << "------------" << std::endl;
      }
      std::tie(lhs, rhs) = std::pair{rhs + lhs, rhs - lhs};
    }
  }
  std::cout << toString(m);
}

auto main() -> int {
  Identity<2> I;

  Dense<double, {3, 3}> m{{8, 0., 0.}, {0., 2., 0.}, {0., 0., 1.}};
  std::cout << toString(dft_matrix(4));

  Dense<std::complex<double>, {8, 1}> vec{
      std::vector<std::complex<double>>(8, std::complex<double>{1., 0.})};
  for (int i = 0; i < 8; ++i) {
    vec[i, 0] = std::sin(2. * std::numbers::pi * i / 8);
  }
  std::cout << toString(vec);
  fft_radix2(vec);
  std::cout << toString(vec);

  return 0;
}
