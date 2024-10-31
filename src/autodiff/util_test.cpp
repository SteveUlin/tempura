#include "autodiff/util.h"

#include <print>

#include "matrix/printer.h"
#include "matrix/storage/dense.h"
#include "matrix/multiplication.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  auto func = [](auto x, auto y) { return x * x * y; };
  auto mat_func = [](auto x) {
    matrix::Dense M{{1.0, 2.0}, {3.0, 4.0}};
    return M * x;
  };

  {
    auto [value, dx, dy] = valueAndForwardGradient(func, 2.0, 3.0);
    std::println("f = {}", value);
    std::println("df/dx = {}", dx);
    std::println("df/dy = {}", dy);
  }
  std::println("-----------");
  {
    auto [value, dx, dy] = valueAndReverseGradient(func, 2.0, 3.0);
    std::println("f = {}", value);
    std::println("df/dx = {}", dx);
    std::println("df/dy = {}", dy);
  }
  std::println("-----------");
  {
    matrix::Dense x{{1.0}, {2.0}};
    auto jacobian_matrix = jacobianReverse(mat_func, x);
    std::println("Jacobian matrix:\n{}", matrix::toString(jacobian_matrix));
  }
  std::println("-----------");
  {
    matrix::Dense x{{1.0}, {2.0}};
    auto jacobian_matrix = jacobianForward(mat_func, x);
    std::println("Jacobian matrix:\n{}", matrix::toString(jacobian_matrix));
  }

  return 0;
}
