#include "qr.h"

#include <cstddef>

#include "matrix.h"
#include "mdarray.h"
#include "transpose.h"
#include "unit.h"
#include "vec.h"

using namespace tempura;

// constexpr oracle: Q·R reconstructs A.
constexpr auto reconstructsA() -> bool {
  InlineDense<double, 3, 3> a{{12, -51, 4}, {6, 167, -68}, {-4, 24, -41}};
  auto qr = qrDecompose(a);
  auto q = formQ(qr);
  auto r = formR(qr);
  auto qrm = multiply(q, r);
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      if (!isClose(qrm[i, j], a[i, j], {.rtol = 1e-9, .atol = 1e-9})) return false;
  return true;
}
static_assert(reconstructsA());

auto main() -> int {
  "Q·R = A and QᵀQ = I (orthonormal), R upper-triangular"_test = [] {
    // classic Golub example; its QR is well known
    Dense<double, Dyn, Dyn> a(dims(3, 3));
    auto va = a.toMdspan();
    double vals[3][3] = {{12, -51, 4}, {6, 167, -68}, {-4, 24, -41}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];

    auto qr = qrDecompose(a);
    auto q = formQ(qr);
    auto r = formR(qr);

    auto recon = multiply(q, r);
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((recon[i, j]), vals[i][j], {.rtol = 1e-9, .atol = 1e-9});

    auto gram = multiply(transposed(q), q);  // QᵀQ = I
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j)
        expectClose((gram[i, j]), i == j ? 1.0 : 0.0, {.rtol = 1e-9, .atol = 1e-9});

    for (std::size_t i = 0; i < 3; ++i)  // R strictly-lower entries vanish
      for (std::size_t j = 0; j < i; ++j) expectClose((r[i, j]), 0.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "QR solves A·x = b (residual ≈ 0)"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(3, 3));
    auto va = a.toMdspan();
    double vals[3][3] = {{4, -2, 1}, {-2, 4, -2}, {1, -2, 4}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 3; ++j) va[i, j] = vals[i][j];
    Vec<double, Dyn> b(dims(3));
    b[0] = 1; b[1] = 4; b[2] = 7;

    auto qr = qrDecompose(a);
    auto x = solve(qr, b);
    for (std::size_t i = 0; i < 3; ++i) {  // residual r = A·x − b
      double row = 0;
      for (std::size_t j = 0; j < 3; ++j) row += vals[i][j] * x[j];
      expectClose(row, (b[i]), {.rtol = 1e-9, .atol = 1e-9});
    }
  };

  "scaled column norm: extreme magnitudes still reconstruct (no overflow/underflow)"_test = [] {
    // a naive Σxᵢ² overflows to ∞ at 1e200 and underflows to 0 at 1e-200 (dropping a
    // reflector); the scaled norm survives both and Q·R = A holds.
    for (double mag : {1e200, 1e-200}) {
      Dense<double, Dyn, Dyn> a(dims(2, 2));
      auto va = a.toMdspan();
      va[0, 0] = 1 * mag; va[0, 1] = 2 * mag;
      va[1, 0] = 3 * mag; va[1, 1] = 4 * mag;
      auto qr = qrDecompose(a);
      auto recon = multiply(formQ(qr), formR(qr));
      const double expected[2][2] = {{1 * mag, 2 * mag}, {3 * mag, 4 * mag}};
      for (std::size_t i = 0; i < 2; ++i)
        for (std::size_t j = 0; j < 2; ++j)
          expectClose((recon[i, j]), expected[i][j], {.rtol = 1e-10, .atol = 0.0});
    }
  };

  "structurally singular matrix is flagged; factorization still reconstructs"_test = [] {
    Dense<double, Dyn, Dyn> a(dims(2, 2));
    auto va = a.toMdspan();
    va[0, 0] = 1; va[0, 1] = 0; va[1, 0] = 0; va[1, 1] = 0;  // a zero pivot column emerges
    auto qr = qrDecompose(a);
    expectTrue(qr.singular);  // solve() would assert on this; factorization is still valid
    auto recon = multiply(formQ(qr), formR(qr));
    const double expected[2][2] = {{1, 0}, {0, 0}};
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j)
        expectClose((recon[i, j]), expected[i][j], {.rtol = 1e-12, .atol = 1e-12});
  };

  return TestRegistry::result();
}
