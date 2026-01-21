#include "bayes3/core.h"

#include <cmath>
#include <random>
#include <type_traits>

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes3;
using namespace tempura::symbolic3;

auto main() -> int {
  "normal() is a Distribution"_test = [] {
    auto x = normal(0.0, 1.0);
    static_assert(Symbolic<decltype(x.sym())>);
    static_assert(Distribution<decltype(x)>);
  };

  "normal() twice produces DIFFERENT types"_test = [] {
    auto x = normal(0.0, 1.0);
    auto y = normal(0.0, 1.0);
    static_assert(!std::is_same_v<decltype(x), decltype(y)>);
  };

  "normal() params are Literal for scalars"_test = [] {
    auto x = normal(0.0, 1.0);
    auto [mu, sigma] = x.params();
    static_assert(std::is_same_v<std::decay_t<decltype(mu)>, Literal<double>>);
    static_assert(std::is_same_v<std::decay_t<decltype(sigma)>, Literal<double>>);
  };

  "normal() tracks parent distributions"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto y = normal(mu, 1.0);

    // y has mu as a parent
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(y.parents())>> == 1);

    // Root has no parents
    static_assert(std::tuple_size_v<std::remove_cvref_t<decltype(mu.parents())>> == 0);
  };

  "sampleTrace samples single distribution"_test = [] {
    auto x = normal(0.0, 1.0);

    std::mt19937_64 rng{42};
    auto trace = sampleTrace(x, rng);

    auto val = trace[x];
    expectTrue(std::isfinite(val));
  };

  "sampleTrace traverses DAG automatically"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(1.0);
    auto y = normal(mu, sigma);

    std::mt19937_64 rng{42};
    // Only pass y - it automatically samples mu and sigma first
    auto trace = sampleTrace(y, rng);

    auto mu_val = trace[mu];
    auto sigma_val = trace[sigma];
    auto y_val = trace[y];

    expectTrue(std::isfinite(mu_val));
    expectTrue(sigma_val > 0.0);
    expectTrue(std::isfinite(y_val));
  };

  "sampleTrace deduplicates diamond dependencies"_test = [] {
    auto shared = normal(0.0, 1.0);
    auto left = normal(shared, 1.0);
    auto right = normal(shared, 2.0);
    auto bottom = normal(left, right);  // Diamond: both left and right depend on shared

    std::mt19937_64 rng{42};
    auto trace = sampleTrace(bottom, rng);

    // All nodes should be sampled
    expectTrue(std::isfinite(trace[shared]));
    expectTrue(std::isfinite(trace[left]));
    expectTrue(std::isfinite(trace[right]));
    expectTrue(std::isfinite(trace[bottom]));
  };

  "unnormalizedLogProb computes joint"_test = [] {
    auto x = normal(0.0, 1.0);

    std::mt19937_64 rng{42};
    auto trace = sampleTrace(x, rng);

    auto lp = unnormalizedLogProb(x, trace);
    expectTrue(std::isfinite(lp));

    auto val = trace[x];
    if (std::abs(val) > 0.1) {
      expectTrue(lp < 0.0);
    }
  };

  "unnormalizedLogProb for hierarchy"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y = normal(mu, 1.0);

    std::mt19937_64 rng{42};
    auto trace = sampleTrace(y, rng);

    auto lp = unnormalizedLogProb(y, trace);

    auto mu_val = trace[mu];
    auto y_val = trace[y];

    // Manual: log p(mu) + log p(y|mu) = -0.5*mu^2 + -0.5*(y-mu)^2
    double expected_lp = -0.5 * mu_val * mu_val + -0.5 * (y_val - mu_val) * (y_val - mu_val);
    expectNear(expected_lp, lp, 1e-10);
  };

  "other distribution types work"_test = [] {
    auto alpha = gamma(2.0, 1.0);
    auto b = gamma(2.0, 1.0);
    auto p = beta(alpha, b);

    std::mt19937_64 rng{42};
    auto trace = sampleTrace(p, rng);

    auto alpha_val = trace[alpha];
    auto b_val = trace[b];
    auto p_val = trace[p];

    expectTrue(alpha_val > 0.0);
    expectTrue(b_val > 0.0);
    expectTrue(p_val > 0.0 && p_val < 1.0);

    auto lp = unnormalizedLogProb(p, trace);
    expectTrue(std::isfinite(lp));
  };

  "trace.get() returns multiple values"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto sigma = halfNormal(1.0);
    auto y = normal(mu, sigma);

    std::mt19937_64 rng{42};
    auto trace = sampleTrace(y, rng);

    auto [m, s, obs] = trace.get(mu, sigma, y);
    expectTrue(std::isfinite(m));
    expectTrue(s > 0.0);
    expectTrue(std::isfinite(obs));
  };

  // ==========================================================================
  // New features: Symbolic log-prob, DeterministicVar, operators
  // ==========================================================================

  "jointUnnormalizedLogProb returns symbolic expression"_test = [] {
    auto x = normal(0.0, 1.0);
    auto lp = jointUnnormalizedLogProb(x);

    // Should be a symbolic expression, not a double
    static_assert(Symbolic<decltype(lp)>);

    // Evaluate it
    double val = evaluate(lp, BinderPack{x = 0.5});
    expectTrue(std::isfinite(val));

    // x=0: lp = -0.5*0^2 = 0
    double at_zero = evaluate(lp, BinderPack{x = 0.0});
    expectNear(0.0, at_zero, 1e-10);
  };

  "jointUnnormalizedLogProb for hierarchy is symbolic"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y = normal(mu, 1.0);
    auto lp = jointUnnormalizedLogProb(y);

    static_assert(Symbolic<decltype(lp)>);

    // Evaluate at specific values
    double val = evaluate(lp, BinderPack{mu = 1.0, y = 2.0});

    // Manual: -0.5*1^2 + -0.5*(2-1)^2 = -0.5 + -0.5 = -1.0
    expectNear(-1.0, val, 1e-10);
  };

  "diff computes gradient of jointUnnormalizedLogProb"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y = normal(mu, 1.0);
    auto lp = jointUnnormalizedLogProb(y);

    // Differentiate w.r.t. mu
    auto dlp_dmu = diff(lp, mu);
    static_assert(Symbolic<decltype(dlp_dmu)>);

    // At mu=1, y=2: ∂lp/∂mu = -mu + (y-mu) = -1 + 1 = 0
    double grad = evaluate(dlp_dmu, BinderPack{mu = 1.0, y = 2.0});
    expectNear(0.0, grad, 1e-10);

    // At mu=0, y=1: ∂lp/∂mu = -mu + (y-mu) = 0 + 1 = 1
    double grad2 = evaluate(dlp_dmu, BinderPack{mu = 0.0, y = 1.0});
    expectNear(1.0, grad2, 1e-10);
  };

  "DeterministicVar from arithmetic"_test = [] {
    auto alpha = normal(0.0, 1.0);
    auto beta_var = normal(0.0, 1.0);
    auto y_hat = alpha + beta_var * 2.0;  // DeterministicVar

    static_assert(is_deterministic_var<decltype(y_hat)>);
    static_assert(GraphNode<decltype(y_hat)>);

    // nodeUnnormalizedLogProb of deterministic var is 0
    auto node_lp = y_hat.nodeUnnormalizedLogProb();
    double val = evaluate(node_lp, BinderPack{});
    expectNear(0.0, val, 1e-10);
  };

  "DeterministicVar in model contributes 0 to jointUnnormalizedLogProb"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto offset = mu + 1.0;  // Deterministic transformation

    auto lp_direct = jointUnnormalizedLogProb(mu);
    auto lp_via_offset = jointUnnormalizedLogProb(offset);

    // Both should evaluate to the same value (offset contributes 0)
    double val1 = evaluate(lp_direct, BinderPack{mu = 0.5});
    double val2 = evaluate(lp_via_offset, BinderPack{mu = 0.5, offset = 1.5});
    expectNear(val1, val2, 1e-10);
  };

  "arithmetic operators work"_test = [] {
    auto a = normal(0.0, 1.0);
    auto b = normal(0.0, 1.0);

    auto sum = a + b;
    auto diff_ab = a - b;
    auto prod = a * b;
    auto quot = a / b;
    auto scaled = a * 2.0;
    auto shifted = 1.0 + a;
    auto neg = -a;

    static_assert(is_deterministic_var<decltype(sum)>);
    static_assert(is_deterministic_var<decltype(diff_ab)>);
    static_assert(is_deterministic_var<decltype(prod)>);
    static_assert(is_deterministic_var<decltype(quot)>);
    static_assert(is_deterministic_var<decltype(scaled)>);
    static_assert(is_deterministic_var<decltype(shifted)>);
    static_assert(is_deterministic_var<decltype(neg)>);

    // Evaluate expressions
    auto bindings = BinderPack{a = 2.0, b = 3.0};
    expectNear(5.0, evaluate(sum.expr(), bindings), 1e-10);
    expectNear(-1.0, evaluate(diff_ab.expr(), bindings), 1e-10);
    expectNear(6.0, evaluate(prod.expr(), bindings), 1e-10);
    expectNear(2.0 / 3.0, evaluate(quot.expr(), bindings), 1e-10);
    expectNear(4.0, evaluate(scaled.expr(), bindings), 1e-10);
    expectNear(3.0, evaluate(shifted.expr(), bindings), 1e-10);
    expectNear(-2.0, evaluate(neg.expr(), bindings), 1e-10);
  };

  "multiple observations with jointUnnormalizedLogProb"_test = [] {
    auto mu = normal(0.0, 1.0);
    auto y1 = normal(mu, 1.0);
    auto y2 = normal(mu, 1.0);

    // Joint log prob of multiple roots
    auto lp = jointUnnormalizedLogProb(y1, y2);
    static_assert(Symbolic<decltype(lp)>);

    // Should include mu once (deduplication)
    // lp = log p(mu) + log p(y1|mu) + log p(y2|mu)
    double val = evaluate(lp, BinderPack{mu = 0.0, y1 = 1.0, y2 = -1.0});

    // -0.5*0^2 + -0.5*1^2 + -0.5*1^2 = 0 - 0.5 - 0.5 = -1.0
    expectNear(-1.0, val, 1e-10);
  };

  return TestRegistry::result();
}
