#include "optimization/bracket_method.h"
#include "optimization/golden_search.h"

#include <print>

#include "unit.h"

using namespace tempura;
using namespace tempura::optimization;

auto main() -> int {
  auto func = [](double x) { return std::pow(x - 100., 2); };
  auto func2 = [](double x) { return std::pow(x, 2); };
  auto func3 = [](double x) {
    double z = x - std::numbers::pi * 1'000'000;
    return -1. / std::pow(1 + z * z * z * z, 2);
  };

  auto brack = bracketMethod(-2.0, -1.0, func3);
  brack = goldenSectionSearch(brack, func3);

  auto [left, middle, right] = brack;

  std::println("left:\t{:.6} {:.6}", left.input, left.output);
  std::println("middle:\t{:.6} {:.6}", middle.input, middle.output);
  std::println("right:\t{:.6} {:.6}", right.input, right.output);
}
