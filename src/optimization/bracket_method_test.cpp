#include "optimization/bracket_method.h"
#include "optimization/golden_search.h"
#include "optimization/brents_method.h"

#include <print>

#include "unit.h"

using namespace tempura;
using namespace tempura::optimization;

auto main() -> int {
  auto func = [](double x) { return std::pow(x - 100., 2); };
  // auto func = [](double x) { return std::pow(x, 2); };
  // auto func = [](double x) {
  //   double z = x - std::numbers::pi;
  //   return -1. / std::pow(1 + z * z, 2) + cos(x);
  // };

  auto brack = bracketMethod(-2.0, 3.0, func);
  brack = goldenSectionSearch(brack, func);
  // brack = brentsMethod(brack, func);

  auto [left, middle, right] = brack;

  std::println("left:\t{:.6} {:.6}", left.input, left.output);
  std::println("middle:\t{:.6} {:.6}", middle.input, middle.output);
  std::println("right:\t{:.6} {:.6}", right.input, right.output);
}
