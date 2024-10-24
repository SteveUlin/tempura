#include <print>

#include "autodiff/node.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  auto x = Variable{};
  auto normal = 1.0 / (std::sqrt(2.0 * std::numbers::pi)) * exp(-0.5 * x * x);
  for (double i = -5; i < 5; i += 0.1) {
    auto [val, dx] = differentiate(normal, Wrt{x}, At{i});
    std::print("{}\n", val);
  }
}
