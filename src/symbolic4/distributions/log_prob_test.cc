// Tests for symbolic4/distributions/log_prob.h
#include "symbolic4/distributions/log_prob.h"
#include "symbolic4/interpreter/eval.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // logNormal
  // ===========================================================================

  "logNormal returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = logNormal(x, mu, sigma);
    double val = evaluate(lp, x = 2.0, mu = 1.0, sigma = 0.5);

    // -0.5 * ((2-1)/0.5)^2 - log(0.5) - 0.5*log(2*pi)
    double z = (2.0 - 1.0) / 0.5;
    double expected = -0.5 * z * z - std::log(0.5) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "logNormal with literal parameters"_test = [] {
    Symbol<struct X> x;
    auto lp = logNormal(x, 0_c, 1_c);
    double val = evaluate(lp, x = 1.0);

    double expected = -0.5 * 1.0 - 0.0 - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // unnormalizedLogNormal
  // ===========================================================================

  "unnormalizedLogNormal omits constants"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = unnormalizedLogNormal(x, mu, sigma);
    double val = evaluate(lp, x = 2.0, mu = 1.0, sigma = 0.5);

    // -0.5 * ((2-1)/0.5)^2 = -0.5 * 4 = -2
    expectNear(-2.0, val, 1e-10);
  };

  // ===========================================================================
  // logHalfNormal
  // ===========================================================================

  "logHalfNormal returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Sigma> sigma;

    auto lp = logHalfNormal(x, sigma);
    double val = evaluate(lp, x = 1.0, sigma = 2.0);

    // log(sqrt(2/pi)) - log(sigma) - x^2/(2*sigma^2)
    double expected = 0.2257913526447274 - std::log(2.0) - 1.0 / (2.0 * 4.0);
    expectNear(expected, val, 1e-10);
  };

  "unnormalizedLogHalfNormal omits constants"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Sigma> sigma;

    auto lp = unnormalizedLogHalfNormal(x, sigma);
    double val = evaluate(lp, x = 1.0, sigma = 2.0);

    // -x^2 / (2*sigma^2) = -1 / 8 = -0.125
    expectNear(-0.125, val, 1e-10);
  };

  // ===========================================================================
  // logBernoulli
  // ===========================================================================

  "logBernoulli for x=1"_test = [] {
    Symbol<struct X> x;
    Symbol<struct P> p;

    auto lp = logBernoulli(x, p);
    double val = evaluate(lp, x = 1.0, p = 0.7);

    // x * log(p) + (1-x) * log(1-p) = log(0.7)
    expectNear(std::log(0.7), val, 1e-10);
  };

  "logBernoulli for x=0"_test = [] {
    Symbol<struct X> x;
    Symbol<struct P> p;

    auto lp = logBernoulli(x, p);
    double val = evaluate(lp, x = 0.0, p = 0.7);

    // x * log(p) + (1-x) * log(1-p) = log(0.3)
    expectNear(std::log(0.3), val, 1e-10);
  };

  // ===========================================================================
  // logExponential
  // ===========================================================================

  "logExponential returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Lambda> lambda;

    auto lp = logExponential(x, lambda);
    double val = evaluate(lp, x = 2.0, lambda = 0.5);

    // log(lambda) - lambda * x = log(0.5) - 0.5 * 2
    double expected = std::log(0.5) - 1.0;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // unnormalizedLogBeta
  // ===========================================================================

  "unnormalizedLogBeta returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Alpha> alpha;
    Symbol<struct Beta> beta;

    auto lp = unnormalizedLogBeta(x, alpha, beta);
    double val = evaluate(lp, x = 0.3, alpha = 2.0, beta = 5.0);

    // (alpha-1)*log(x) + (beta-1)*log(1-x)
    double expected = std::log(0.3) + 4.0 * std::log(0.7);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // unnormalizedLogGamma
  // ===========================================================================

  "unnormalizedLogGamma returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Alpha> alpha;
    Symbol<struct Beta> beta;

    auto lp = unnormalizedLogGamma(x, alpha, beta);
    double val = evaluate(lp, x = 2.0, alpha = 3.0, beta = 0.5);

    // (alpha-1)*log(x) - beta*x
    double expected = 2.0 * std::log(2.0) - 0.5 * 2.0;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // logUniform
  // ===========================================================================

  "logUniform returns negative log width"_test = [] {
    Symbol<struct X> x;
    Symbol<struct A> a;
    Symbol<struct B> b;

    auto lp = logUniform(x, a, b);
    double val = evaluate(lp, x = 0.5, a = 0.0, b = 2.0);

    // -log(b - a) = -log(2)
    expectNear(-std::log(2.0), val, 1e-10);
  };

  // ===========================================================================
  // logCauchy
  // ===========================================================================

  "logCauchy returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct X0> x0;
    Symbol<struct Gamma> gamma;

    auto lp = logCauchy(x, x0, gamma);
    double val = evaluate(lp, x = 1.0, x0 = 0.0, gamma = 1.0);

    // -log(pi) - log(gamma) - log(1 + ((x-x0)/gamma)^2)
    double expected = -std::log(M_PI) - std::log(1.0) - std::log(1.0 + 1.0);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // unnormalizedLogStudentT
  // ===========================================================================

  "unnormalizedLogStudentT returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Nu> nu;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = unnormalizedLogStudentT(x, nu, mu, sigma);
    double val = evaluate(lp, x = 1.0, nu = 3.0, mu = 0.0, sigma = 1.0);

    // -(nu+1)/2 * log(1 + z^2/nu) where z = (x-mu)/sigma
    // = -2 * log(1 + 1/3) = -2 * log(4/3)
    double expected = -2.0 * std::log(4.0 / 3.0);
    expectNear(expected, val, 1e-10);
  };

  return TestRegistry::result();
}
