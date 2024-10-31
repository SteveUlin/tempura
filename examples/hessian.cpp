#include <print>

#include "autodiff/node.h"
#include "autodiff/dual.h"
#include "broadcast_array.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  Variable<Dual<double, BroadcastArray<double, 2>>> x;
  Variable<Dual<double, BroadcastArray<double, 2>>> y;

  auto f = pow(x, 2.) + pow(y, 2.);
  auto [value, dx, dy] = differentiate(f,
    x = Dual<double, BroadcastArray<double, 2>>{4., {1., 0.}},
    y = Dual<double, BroadcastArray<double, 2>>{2., {0., 1.}});

  std::println("f = {}", value.value);
  std::println("df/dx = {}", value.gradient[0]);
  std::println("df/dy = {}", value.gradient[1]);

  std::println("backpropagate df/dx = {}", dx.value);
  std::println("backpropagate df/dy = {}", dy.value);

  std::println("d²f/dx² = {}", dx.gradient[0]);
  std::println("d²f/dxdy = {}", dx.gradient[1]);
  std::println("d²f/dydx = {}", dy.gradient[0]);
  std::println("d²f/dy² = {}", dy.gradient[1]);
}

