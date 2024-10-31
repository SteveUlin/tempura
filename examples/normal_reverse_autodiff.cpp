#include <print>

#include "autodiff/node.h"

using namespace tempura;
using namespace tempura::autodiff;

auto main() -> int {
  Variable x{};
  NodeExpr normal = 1.0 / (std::sqrt(2.0 * std::numbers::pi)) * exp(-0.5 * x * x);
  for (double i = -5; i < 5; i += 0.1) {
    x.bind(i);
    normal.node->propagateNode(IndependentVariable<double>::make(1.0));
    normal.node->propagate(1.0);
    // x.derivativeNode()->propagate(1.0);
    // std::print("{}\n", x.derivative());
    std::print("{}\n", normal.value());
    normal.node->clearValue();
    x.clearDerivative();
  }
}

