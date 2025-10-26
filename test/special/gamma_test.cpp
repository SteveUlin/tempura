#include "special/gamma.h"

#include <print>

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

  "incompleteGamma Continued Fraction"_test = [] {
    std::println("{}", detail::incompleteGammaContinuedFraction(100.0, 95.0));
    std::println("{}", detail::incompleteGammaSeries(100.0, 95.0));
    std::println("{}", detail::incompleteGammaGaussianQuadature(100.0, 95.0));

    std::println("{}", detail::incompleteGammaContinuedFraction(100.0, 105.0));
    std::println("{}", detail::incompleteGammaSeries(100.0, 105.0));
    std::println("{}", detail::incompleteGammaGaussianQuadature(100.0, 105.0));
  };

  return 0;
};
