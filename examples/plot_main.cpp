#include <format>
#include <iostream>
#include <limits>
#include <print>
#include <thread>
#include <vector>

#include "bayes/cauchy.h"
#include "bayes/exponential.h"
#include "bayes/gamma.h"
#include "bayes/logistic.h"
#include "bayes/normal.h"
#include "plot.h"

using namespace tempura;

auto main() -> int {
  std::function<double(double)> fn = [](double x) {
    return (std::abs(x) < std::numeric_limits<double>::epsilon())
               ? 1.
               : std::sin(x) / x;
  };

  std::cout << plotFn(fn, 0., 50., 100, 17);
  while (true) {
    double x1;
    double x2;
    std::cin >> x1 >> x2;
    std::cout << linePlot(fn, x1, x2, 100, 17);
  }
  return 0;
}
