#include "special/gamma.h"

#include <cmath>

#include "unit.h"

using namespace tempura;
using namespace tempura::special;

auto main() -> int {
  "gamma"_test = [] {
    expectNear(logGamma(4.0), std::lgamma(4.0));
    expectNear(logGamma(50.0), std::lgamma(50.0));
    expectNear(logGamma(100.0), std::lgamma(100.0));
  };

  "factorial"_test = [] {
    expectEq(factorial(0), 1.);
    expectEq(factorial(5), 120.);
    expectEq(factorial(10), 3628800.);
    expectEq(factorial(20), 2432902008176640000.);
  };

  "log factorial"_test = [] {
    expectNear(logFactorial(0), std::lgamma(1.0));
    expectNear(logFactorial(5), std::lgamma(6.0));
    expectNear(logFactorial(10), std::lgamma(11.0));
    expectNear(logFactorial(20), std::lgamma(21.0));
  };

  "binomial coefficient"_test = [] {
    expectEq(binomialCoefficient(5, 0), 1.);
    expectEq(binomialCoefficient(5, 1), 5.);
    expectEq(binomialCoefficient(5, 2), 10.);
    expectEq(binomialCoefficient(5, 3), 10.);
    expectEq(binomialCoefficient(5, 4), 5.);
    expectEq(binomialCoefficient(5, 5), 1.);
  };

  "beta"_test = [] {
    expectNear(beta(1.0, 1.0), 1.0);
    expectNear(beta(2.0, 2.0), 0.16666666666666666);
    expectNear(beta(3.0, 3.0), 0.03333333333333333);
    expectNear(beta(2.0, 3.0), 0.08333333333333333);
  };

  // Regularized lower incomplete gamma P(a,x) ∈ [0,1]. Closed-form references:
  //   P(1,x) = 1 − e^(−x),   P(2,x) = 1 − e^(−x)(1 + x)
  "incompleteGamma"_test = [] {
    expectNear(incompleteGamma(1.0, 1.0), 1.0 - std::exp(-1.0));
    expectNear(incompleteGamma(1.0, 2.0), 1.0 - std::exp(-2.0));
    expectNear(incompleteGamma(2.0, 1.0), 1.0 - 2.0 * std::exp(-1.0));
    expectNear(incompleteGamma(2.0, 2.0), 1.0 - 3.0 * std::exp(-2.0));

    // The double overload switches to Gauss-Legendre quadrature for a ≥ 100;
    // cross-check it against the in-regime series / continued-fraction methods
    // on each side of the x = a transition.
    expectNear(incompleteGamma(100.0, 95.0),
               incompleteGammaSeries(100.0, 95.0), 1e-6);
    expectNear(incompleteGamma(100.0, 105.0),
               incompleteGammaContinuedFraction(100.0, 105.0), 1e-6);
  };

  // Regularized incomplete beta I_x(a,b) ∈ [0,1]. Closed-form references:
  //   I_x(1,1) = x,   I_x(2,1) = x²,   I_x(1,2) = 1 − (1−x)²,   I_0.5(2,2) = 0.5
  "incompleteBeta"_test = [] {
    expectNear(incompleteBeta(1.0, 1.0, 0.5), 0.5);
    expectNear(incompleteBeta(1.0, 1.0, 0.3), 0.3);
    expectNear(incompleteBeta(2.0, 1.0, 0.5), 0.25);
    expectNear(incompleteBeta(1.0, 2.0, 0.5), 0.75);
    expectNear(incompleteBeta(2.0, 2.0, 0.5), 0.5);
  };

  return TestRegistry::result();
};
