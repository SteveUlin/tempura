#include "jacobian.h"

#include <array>
#include <cstddef>

#include "matrix/matrix.h"
#include "matrix/mdarray.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  "square map: jacfwd and jacrev agree with the known Jacobian"_test = [] {
    // f(x,y) = [x²+y, x·y]; J = [[2x, 1], [y, x]]; at (3,4): [[6,1],[4,3]]
    auto f = [](auto x, auto y) { return std::array{x * x + y, x * y}; };
    const std::array in{3.0, 4.0};
    auto jf = jacfwd(f, in);
    auto jr = jacrev(f, in);
    const double expected[2][2] = {{6, 1}, {4, 3}};
    for (std::size_t i = 0; i < 2; ++i)
      for (std::size_t j = 0; j < 2; ++j) {
        expectClose((jf[i, j]), expected[i][j], {.rtol = 1e-12, .atol = 1e-12});
        expectClose((jr[i, j]), expected[i][j], {.rtol = 1e-12, .atol = 1e-12});
      }
  };

  "tall map (M>N): R²→R³"_test = [] {
    // f(x,y) = [x, y, x·y]; J = [[1,0],[0,1],[y,x]]; at (2,5): [[1,0],[0,1],[5,2]]
    auto f = [](auto x, auto y) { return std::array{x, y, x * y}; };
    auto jf = jacfwd(f, std::array{2.0, 5.0});
    auto jr = jacrev(f, std::array{2.0, 5.0});
    expectEq(jf.extent(0), std::size_t{3});  // M outputs × N inputs
    expectEq(jf.extent(1), std::size_t{2});
    const double expected[3][2] = {{1, 0}, {0, 1}, {5, 2}};
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = 0; j < 2; ++j) {
        expectClose((jf[i, j]), expected[i][j], {.rtol = 1e-12, .atol = 1e-12});
        expectClose((jr[i, j]), expected[i][j], {.rtol = 1e-12, .atol = 1e-12});
      }
  };

  "result is a real tempura matrix"_test = [] {
    auto f = [](auto x, auto y) { return std::array{x * y, x + y}; };
    auto jac = jacfwd(f, std::array{1.0, 1.0});
    static_assert(Owning<decltype(jac)>);  // an owning src/matrix Dense, not an ad-hoc array
  };

  return TestRegistry::result();
}
