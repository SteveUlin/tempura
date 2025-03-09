#include "chebyshev.h"

#include <print>

#include "special/gamma.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Simple Eval"_test = [] {
    Chebyshev chebyshev([](double x) { return x; }, -1., 1.);
    expectNear(1., chebyshev(1.));
  };

  "Exp Approx"_test = [] {
    Chebyshev chebyshev([](double x) { return std::exp(x); }, -1., 1.);
    expectNear(1., chebyshev(0.));
    expectNear(std::numbers::e, chebyshev(1.));
    expectNear(std::exp(0.5), chebyshev(0.5));

    chebyshev.setThreshold(1e-10);
    expectEq(11, chebyshev.degree());  // Check degree after setting threshold

    // Re-check values after changing the degree.
    expectNear(1., chebyshev(0.));
    expectNear(std::numbers::e, chebyshev(1.));
    expectNear(std::exp(0.5), chebyshev(0.5));
  };

  "Sin Approx"_test = [] {
    Chebyshev chebyshev([](double x) { return std::sin(x); }, -1., 1.);
    expectNear(0., chebyshev(0.));
    expectNear(std::sin(-1.), chebyshev(-1.));
    expectNear(std::sin(1.), chebyshev(1.));
    expectNear(std::sin(0.5), chebyshev(0.5));

    chebyshev.setThreshold(1e-8);
    expectEq(10, chebyshev.degree());

    // Re-check after threshold change
    expectNear(0., chebyshev(0.));
    expectNear(std::sin(-1.), chebyshev(-1.));
    expectNear(std::sin(1.), chebyshev(1.));
    expectNear(std::sin(0.5), chebyshev(0.5));
  };

  "Discontinuity Outside Interval"_test = [] {
    Chebyshev chebyshev([](double x) { return 1.0 / (x - 2.0); }, -1., 1.);
    expectNear(1.0 / (0. - 2.), chebyshev(0.));
    expectNear(1.0 / (-1. - 2.), chebyshev(-1.));
    expectNear(1.0 / (1. - 2.), chebyshev(1.));
    expectNear(1.0 / (0.5 - 2.), chebyshev(0.5));
  };

  "Steep Change"_test = [] {
    Chebyshev chebyshev([](double x) { return std::tanh(10 * x); }, -1., 1.,
                        100);
    chebyshev.setThreshold(1e-8);
    // Definitely not on a node
    expectNear(std::tanh(10 * -std::sqrt(.3)), chebyshev(-std::sqrt(.3)));
    expectNear(0, chebyshev(0.));
    expectNear(std::tanh(10 * .5), chebyshev(.5));
  };

  "Zero Function"_test = [] {
    Chebyshev chebyshev([](double /*unused*/) { return 0; }, -1., 1.);
    expectNear(0, chebyshev(0.));
    expectNear(0, chebyshev(-1.));
    expectNear(0, chebyshev(1.));
  };
  // Deriative and integral tests

  "Derivative"_test = [] {
    Chebyshev chebyshev([](double x) { return x * x; }, -1., 1.);
    auto derivative = chebyshev.derivative();
    expectNear(-2., derivative(-1.));
    expectNear(2., derivative(1.));
    expectNear(0., derivative(0.));
    expectNear(1., derivative(0.5));
  };

  "Integral"_test = [] {
    Chebyshev chebyshev([](double x) { return x * x; }, -1., 1.);
    auto integral = chebyshev.integral();
    expectNear(0., integral(-1.));
    expectNear(1. / 3., integral(0.));
    expectNear(.375, integral(0.5));
    expectNear(2. / 3., integral(1.));
  };

}
