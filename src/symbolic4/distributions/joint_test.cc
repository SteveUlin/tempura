// Tests for symbolic4/distributions/joint.h
#include "symbolic4/distributions/joint.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/distributions/wrappers.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/strategy/diff.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // logProb for single RV
  // ===========================================================================

  "logProb for single normal"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto lp = logProb(mu);

    static_assert(Symbolic<decltype(lp)>);

    double val = evaluate(lp, mu = 0.5);
    double expected = -0.5 * 0.25 - std::log(1.0) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  "logProb for halfNormal"_test = [] {
    auto sigma = halfNormal(5_c);
    auto lp = logProb(sigma);

    double val = evaluate(lp, sigma = 2.0);
    double expected = 0.2257913526447274 - std::log(5.0) - 4.0 / (2.0 * 25.0);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // logProb for multiple RVs - user explicitly lists all
  // ===========================================================================

  "logProb for two RVs"_test = [] {
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    // User explicitly lists both RVs
    auto lp = logProb(mu, y);

    // Joint = log P(mu) + log P(y|mu)
    double val = evaluate(lp, mu = 1.0, y = 1.5);

    // log N(1.0 | 0, 10)
    double lp_mu =
        -0.5 * (1.0 / 10.0) * (1.0 / 10.0) - std::log(10.0) - 0.9189385332046727;

    // log N(1.5 | 1.0, 1.0)
    double z_y = (1.5 - 1.0) / 1.0;
    double lp_y = -0.5 * z_y * z_y - std::log(1.0) - 0.9189385332046727;

    double expected = lp_mu + lp_y;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // logProb for three RVs
  // ===========================================================================

  "logProb for three RVs"_test = [] {
    auto mu = normal(0_c, 10_c);
    auto sigma = halfNormal(5_c);
    auto y = normal(mu, sigma);

    // User explicitly lists all three RVs
    auto lp = logProb(mu, sigma, y);

    // Joint = log P(mu) + log P(sigma) + log P(y|mu,sigma)
    double val = evaluate(lp, mu = 1.0, sigma = 2.0, y = 1.5);

    // log N(1.0 | 0, 10)
    double lp_mu =
        -0.5 * (1.0 / 10.0) * (1.0 / 10.0) - std::log(10.0) - 0.9189385332046727;

    // log HalfNormal(2.0 | 5)
    double lp_sigma = 0.2257913526447274 - std::log(5.0) - 2.0 * 2.0 / (2.0 * 5.0 * 5.0);

    // log N(1.5 | 1.0, 2.0)
    double z = (1.5 - 1.0) / 2.0;
    double lp_y = -0.5 * z * z - std::log(2.0) - 0.9189385332046727;

    double expected = lp_mu + lp_sigma + lp_y;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // logProb with beta distribution
  // ===========================================================================

  "logProb with beta"_test = [] {
    auto p = beta(2_c, 5_c);
    auto lp = logProb(p);

    double val = evaluate(lp, p = 0.3);

    // Full normalized log-prob: log(B(α,β)^-1) + (α-1)log(x) + (β-1)log(1-x)
    // = lgamma(α+β) - lgamma(α) - lgamma(β) + (α-1)log(x) + (β-1)log(1-x)
    double log_normalizer = std::lgamma(7.0) - std::lgamma(2.0) - std::lgamma(5.0);
    double expected = log_normalizer + std::log(0.3) + 4.0 * std::log(0.7);
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // Likelihood only (omitting prior)
  // ===========================================================================

  "logProb for likelihood only"_test = [] {
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    // Only include y - this is the likelihood (useful when mu is a point estimate)
    auto likelihood = logProb(y);

    double val = evaluate(likelihood, mu = 1.0, y = 1.5);

    // Only log N(1.5 | 1.0, 1.0)
    double z_y = (1.5 - 1.0) / 1.0;
    double expected = -0.5 * z_y * z_y - std::log(1.0) - 0.9189385332046727;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // Differentiation through logProb
  // ===========================================================================

  "diff of logProb wrt parameter"_test = [] {
    auto mu = normal(0_c, 10_c);
    auto y = normal(mu, 1_c);

    auto lp = logProb(mu, y);
    // Use freeSym() because that's what appears in the log-prob expressions
    auto d_mu = diff(lp, mu.freeSym());

    // gradient at mu=0.5, y=1.0
    double val = evaluate(d_mu, mu = 0.5, y = 1.0);

    // d/d_mu [log N(mu|0,10) + log N(y|mu,1)]
    // = d/d_mu [-0.5 * (mu/10)^2 + ...] + d/d_mu [-0.5 * (y-mu)^2 + ...]
    // = -mu/100 + (y - mu)
    // = -0.5/100 + (1.0 - 0.5) = -0.005 + 0.5 = 0.495
    double expected = -0.5 / 100.0 + (1.0 - 0.5);
    expectNear(expected, val, 1e-10);
  };

  "diff of logProb wrt sigma"_test = [] {
    auto sigma = halfNormal(5_c);
    auto y = normal(0_c, sigma);

    auto lp = logProb(sigma, y);
    // Use freeSym() because that's what appears in the log-prob expressions
    auto d_sigma = diff(lp, sigma.freeSym());

    // At sigma=2, y=1
    double val = evaluate(d_sigma, sigma = 2.0, y = 1.0);

    // d/d_sigma [log HalfNormal(sigma|5) + log N(y|0,sigma)]
    // HalfNormal: d/d_sigma [-sigma^2 / (2*25)] = -sigma/25 = -2/25 = -0.08
    // Normal: d/d_sigma [-0.5 * (y/sigma)^2 - log(sigma)]
    //       = y^2 / sigma^3 - 1/sigma = 1/8 - 0.5 = -0.375
    // Total = -0.08 + (-0.375) = -0.455
    double grad_halfnormal = -2.0 / 25.0;
    double grad_normal = 1.0 / 8.0 - 1.0 / 2.0;
    double expected = grad_halfnormal + grad_normal;
    expectNear(expected, val, 1e-10);
  };

  // ===========================================================================
  // unnormalizedLogProb
  // ===========================================================================

  "unnormalizedLogProb omits constants"_test = [] {
    auto mu = normal(0_c, 1_c);
    auto lp = unnormalizedLogProb(mu);

    double val = evaluate(lp, mu = 0.5);
    // Just -0.5 * z^2
    double expected = -0.5 * 0.25;
    expectNear(expected, val, 1e-10);
  };

  return TestRegistry::result();
}
