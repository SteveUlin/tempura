// Tests for symbolic4/distributions
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/interpreter/diff.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/simplify.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // =========================================================================
  // Log-probability functions
  // =========================================================================

  "logNormal returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = logNormal(x, mu, sigma);
    double val = evaluate(lp, x = 2.0, mu = 1.0, sigma = 0.5);

    // Expected: -0.5 * ((2-1)/0.5)^2 - log(0.5) - 0.5*log(2*pi)
    double expected = -0.5 * 4.0 - std::log(0.5) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "unnormalizedLogNormal omits constants"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = unnormalizedLogNormal(x, mu, sigma);
    double val = evaluate(lp, x = 2.0, mu = 1.0, sigma = 0.5);

    // Expected: -0.5 * ((2-1)/0.5)^2 = -0.5 * 4 = -2
    expectNear(-2.0, val, 1e-10);
  };

  "logBernoulli returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct P> p;

    auto lp = logBernoulli(x, p);

    // x=1: log(p) = log(0.7)
    double val1 = evaluate(lp, x = 1.0, p = 0.7);
    expectNear(std::log(0.7), val1, 1e-10);

    // x=0: log(1-p) = log(0.3)
    double val0 = evaluate(lp, x = 0.0, p = 0.7);
    expectNear(std::log(0.3), val0, 1e-10);
  };

  "logHalfNormal returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Sigma> sigma;

    auto lp = logHalfNormal(x, sigma);
    double val = evaluate(lp, x = 1.0, sigma = 2.0);

    // log(sqrt(2/pi)) - log(sigma) - x^2/(2*sigma^2)
    double expected = 0.2257913526447274 - std::log(2.0) - 1.0 / (2.0 * 4.0);
    expectNear(expected, val, 1e-10);
  };

  "logExponential returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Lambda> lambda;

    auto lp = logExponential(x, lambda);
    double val = evaluate(lp, x = 2.0, lambda = 0.5);

    // log(lambda) - lambda * x = log(0.5) - 0.5 * 2 = -0.693 - 1 = -1.693
    double expected = std::log(0.5) - 0.5 * 2.0;
    expectNear(expected, val, 1e-10);
  };

  "unnormalizedLogBeta returns correct expression"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Alpha> alpha;
    Symbol<struct Beta> beta;

    auto lp = unnormalizedLogBeta(x, alpha, beta);
    double val = evaluate(lp, x = 0.3, alpha = 2.0, beta = 5.0);

    // (alpha-1)*log(x) + (beta-1)*log(1-x)
    // = 1*log(0.3) + 4*log(0.7)
    double expected = std::log(0.3) + 4.0 * std::log(0.7);
    expectNear(expected, val, 1e-10);
  };

  // =========================================================================
  // Distribution wrappers
  // =========================================================================

  "NormalDist logProbFor works"_test = [] {
    Symbol<struct X> x;
    auto dist = Normal(1.0, 0.5);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 2.0);

    double expected = -0.5 * 4.0 - std::log(0.5) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "HalfNormalDist logProbFor works"_test = [] {
    Symbol<struct X> x;
    auto dist = HalfNormal(2.0);
    auto lp = dist.logProbFor(x);
    double val = evaluate(lp, x = 1.0);

    double expected = 0.2257913526447274 - std::log(2.0) - 1.0 / (2.0 * 4.0);
    expectNear(expected, val, 1e-10);
  };

  "BetaDist unnormalizedLogProbFor works"_test = [] {
    Symbol<struct X> x;
    auto dist = Beta(2.0, 5.0);
    auto lp = dist.unnormalizedLogProbFor(x);
    double val = evaluate(lp, x = 0.3);

    // (alpha-1)*log(x) + (beta-1)*log(1-x) = 1*log(0.3) + 4*log(0.7)
    double expected = std::log(0.3) + 4.0 * std::log(0.7);
    expectNear(expected, val, 1e-10);
  };

  // =========================================================================
  // StochasticNode and factories
  // =========================================================================

  "normal() creates StochasticNode"_test = [] {
    auto mu = normal(0.0, 1.0);
    static_assert(IsRandomVar<decltype(mu)>);
  };

  "StochasticNode::logProb returns symbolic expression"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto lp = mu.logProb();
    double val = evaluate(lp, mu = 0.5);

    // log N(0.5 | 0, 1) = -0.5 * 0.25 - 0 - 0.919
    double expected = -0.5 * 0.25 - std::log(1.0) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "StochasticNode::sym returns symbol"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto sym = mu.sym();
    static_assert(Symbolic<decltype(sym)>);
  };

  // =========================================================================
  // Joint log-probability
  // =========================================================================

  "logProb for single node"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto lp = logProb(mu);

    double val = evaluate(lp, mu = 0.5);
    double expected = -0.5 * 0.25 - std::log(1.0) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "logProb for joint model"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);

    // Joint = log P(mu) + log P(sigma) + log P(y|mu,sigma)
    auto lp = logProb(mu, sigma, y);  // Must list all RVs for joint
    double val = evaluate(lp, mu = 1.0, sigma = 2.0, y = 1.5);

    // log N(1.0 | 0, 10)
    double lp_mu = -0.5 * (1.0 / 10.0) * (1.0 / 10.0) - std::log(10.0) -
                   0.9189385332046727;

    // log HalfNormal(2.0 | 5)
    double lp_sigma =
        0.2257913526447274 - std::log(5.0) - 2.0 * 2.0 / (2.0 * 5.0 * 5.0);

    // log N(1.5 | 1.0, 2.0)
    double z = (1.5 - 1.0) / 2.0;
    double lp_y = -0.5 * z * z - std::log(2.0) - 0.9189385332046727;

    double expected = lp_mu + lp_sigma + lp_y;
    expectNear(expected, val, 1e-10);
  };

  // =========================================================================
  // Differentiation
  // =========================================================================

  "diff of logNormal wrt mu"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = logNormal(x, mu, sigma);
    auto d_mu = simplify(diff(lp, mu));

    // d/d_mu log N(x|mu,sigma) = (x - mu) / sigma^2
    double val = evaluate(d_mu, x = 2.0, mu = 1.0, sigma = 0.5);
    double expected = (2.0 - 1.0) / (0.5 * 0.5);  // = 4.0
    expectNear(expected, val, 1e-10);
  };

  "diff of logNormal wrt sigma"_test = [] {
    Symbol<struct X> x;
    Symbol<struct Mu> mu;
    Symbol<struct Sigma> sigma;

    auto lp = logNormal(x, mu, sigma);
    auto d_sigma = simplify(diff(lp, sigma));

    // d/d_sigma [-0.5*z^2 - log(sigma) - c] where z = (x-mu)/sigma
    // = -0.5 * 2 * z * d/d_sigma[(x-mu)/sigma] - 1/sigma
    // = -0.5 * 2 * z * [-(x-mu)/sigma^2] - 1/sigma
    // = z * (x-mu) / sigma^2 - 1/sigma
    // = z^2 / sigma - 1/sigma
    // = (z^2 - 1) / sigma
    double val = evaluate(d_sigma, x = 2.0, mu = 1.0, sigma = 0.5);
    double z = (2.0 - 1.0) / 0.5;  // = 2
    double expected = (z * z - 1.0) / 0.5;  // = (4 - 1) / 0.5 = 6
    expectNear(expected, val, 1e-10);
  };

  "diff of logProb wrt parameter"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto y = normal(mu, lit(1.0));

    // Joint log-prob: log P(mu) + log P(y|mu)
    auto lp = logProb(mu, y);
    // Use freeSym() because that's what appears in the log-prob expression
    auto d_mu = simplify(diff(lp, mu.freeSym()));

    // gradient at mu=0.5, y=1.0
    double val = evaluate(d_mu, mu = 0.5, y = 1.0);

    // d/d_mu [log N(mu|0,10) + log N(y|mu,1)]
    // = d/d_mu [-0.5 * (mu/10)^2 + ...] + d/d_mu [-0.5 * (y-mu)^2 + ...]
    // = -mu/100 + (y - mu)
    // = -0.5/100 + (1.0 - 0.5) = -0.005 + 0.5 = 0.495
    double expected = -0.5 / 100.0 + (1.0 - 0.5);
    expectNear(expected, val, 1e-10);
  };

  return TestRegistry::result();
}
