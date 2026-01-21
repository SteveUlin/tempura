#include "bayes/sym/dsl.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::bayes::sym;
using namespace tempura::symbolic3;

auto main() -> int {
  // =========================================================================
  // Basic deterministic statement creation
  // =========================================================================
  "deterministic statement creates correct type"_test = [] {
    constexpr Symbol alpha;
    constexpr Symbol beta;
    constexpr Symbol x;
    constexpr Symbol y_hat;

    auto det = y_hat << alpha + beta * x;

    // Verify it's a DeterministicStatement
    static_assert(is_deterministic_statement<decltype(det)>);

    // Verify logProb returns Constant<0>
    auto lp = det.logProb();
    static_assert(std::is_same_v<decltype(lp), Constant<0>>);
  };

  // =========================================================================
  // Simple linear regression model: y ~ Normal(alpha + beta * x, sigma)
  // =========================================================================
  "linear regression with deterministic y_hat"_test = [] {
    constexpr Symbol alpha;
    constexpr Symbol beta;
    constexpr Symbol sigma;
    constexpr Symbol x;
    constexpr Symbol y_hat;
    constexpr Symbol y;

    // Model:
    //   alpha ~ Normal(0, 10)
    //   beta ~ Normal(0, 10)
    //   sigma ~ HalfNormal(5)
    //   y_hat = alpha + beta * x  (deterministic)
    //   y ~ Normal(y_hat, sigma)
    auto joint = Joint{
        alpha | Normal(0.0, 10.0),
        beta  | Normal(0.0, 10.0),
        sigma | HalfNormal(5.0),
        y_hat << alpha + beta * x,
        y     | Normal(y_hat, sigma)
    };

    // Fix x=2.0, observe y=5.5, infer alpha, beta, sigma
    auto posterior = joint
        .observe(x = 2.0, y = 5.5)
        .infer(alpha, beta, sigma);

    // Test at alpha=1.0, beta=2.0, sigma=1.0
    // y_hat = 1.0 + 2.0 * 2.0 = 5.0
    // y ~ Normal(5.0, 1.0), observed y = 5.5
    double lp = posterior.logProb(1.0, 2.0, 1.0);

    // Expected log-prob:
    // logNormal(alpha=1.0, 0, 10) + logNormal(beta=2.0, 0, 10)
    //   + logHalfNormal(sigma=1.0, 5) + logNormal(y=5.5, y_hat=5.0, sigma=1.0)
    double lp_alpha = logNormal(1.0, 0.0, 10.0);
    double lp_beta = logNormal(2.0, 0.0, 10.0);
    double lp_sigma = logHalfNormal(1.0, 5.0);
    double lp_y = logNormal(5.5, 5.0, 1.0);
    double expected_lp = lp_alpha + lp_beta + lp_sigma + lp_y;

    expectNear(expected_lp, lp, 1e-10);
  };

  // =========================================================================
  // Gradient through deterministic nodes
  // =========================================================================
  "gradient flows through deterministic nodes"_test = [] {
    constexpr Symbol alpha;
    constexpr Symbol beta;
    constexpr Symbol x;
    constexpr Symbol y_hat;
    constexpr Symbol y;

    // Simplified model with fixed sigma
    auto joint = Joint{
        alpha | Normal(0.0, 10.0),
        beta | Normal(0.0, 10.0),
        y_hat << alpha + beta * x,
        y | Normal(y_hat, 1.0)  // sigma = 1.0 fixed
    };

    auto posterior = joint
        .observe(x = 2.0, y = 5.5)
        .infer(alpha, beta);

    auto grad = posterior.gradient(1.0, 2.0);

    // y_hat = alpha + beta * x = 1.0 + 2.0 * 2.0 = 5.0
    // Residual: y - y_hat = 5.5 - 5.0 = 0.5
    //
    // d/d(alpha) logNormal(y, y_hat, 1) = (y - y_hat) * d(y_hat)/d(alpha) = 0.5 * 1 = 0.5
    // d/d(alpha) logNormal(alpha, 0, 10) = -alpha/100 = -0.01
    // Total d/d(alpha) = 0.5 - 0.01 = 0.49
    double expected_grad_alpha = 0.5 - 1.0 / 100.0;
    expectNear(expected_grad_alpha, grad[0], 1e-10);

    // d/d(beta) logNormal(y, y_hat, 1) = (y - y_hat) * d(y_hat)/d(beta) = 0.5 * x = 0.5 * 2.0 = 1.0
    // d/d(beta) logNormal(beta, 0, 10) = -beta/100 = -0.02
    // Total d/d(beta) = 1.0 - 0.02 = 0.98
    double expected_grad_beta = 0.5 * 2.0 - 2.0 / 100.0;
    expectNear(expected_grad_beta, grad[1], 1e-10);
  };

  // =========================================================================
  // Multiple deterministic nodes
  // =========================================================================
  "multiple deterministic nodes"_test = [] {
    constexpr Symbol a;
    constexpr Symbol b;
    constexpr Symbol sum_ab;
    constexpr Symbol diff_ab;
    constexpr Symbol y1;
    constexpr Symbol y2;

    auto joint = Joint{
        a | Normal(0.0, 1.0),
        b | Normal(0.0, 1.0),
        sum_ab << a + b,
        diff_ab << a - b,
        y1 | Normal(sum_ab, 1.0),
        y2 | Normal(diff_ab, 1.0)
    };

    auto posterior = joint
        .observe(y1 = 3.0, y2 = 1.0)
        .infer(a, b);

    double lp = posterior.logProb(2.0, 1.0);

    // a=2.0, b=1.0 -> sum_ab=3.0, diff_ab=1.0
    // y1=3.0 ~ Normal(3.0, 1.0) -> logNormal(3, 3, 1) = -0.5*log(2*pi) (z=0)
    // y2=1.0 ~ Normal(1.0, 1.0) -> logNormal(1, 1, 1) = -0.5*log(2*pi) (z=0)
    double lp_a = logNormal(2.0, 0.0, 1.0);
    double lp_b = logNormal(1.0, 0.0, 1.0);
    double lp_y1 = logNormal(3.0, 3.0, 1.0);  // Perfect match
    double lp_y2 = logNormal(1.0, 1.0, 1.0);  // Perfect match
    double expected_lp = lp_a + lp_b + lp_y1 + lp_y2;

    expectNear(expected_lp, lp, 1e-10);
  };

  // =========================================================================
  // Deterministic node only contributes 0 to logProb
  // =========================================================================
  "deterministic logProb returns Constant<0>"_test = [] {
    constexpr Symbol x;
    constexpr Symbol y;

    auto det = y << x * x;

    // At compile-time, verify logProb is Constant<0>
    auto lp = det.logProb();
    static_assert(Constant<0>::value == 0);
    static_assert(std::is_same_v<decltype(lp), Constant<0>>);

    // When added to other log-probs, effectively contributes 0
    expectEq(0, Constant<0>::value);
  };

  // =========================================================================
  // Model without deterministic nodes still works (regression test)
  // =========================================================================
  "model without deterministic nodes unchanged"_test = [] {
    constexpr Symbol mu;
    constexpr Symbol sigma;
    constexpr Symbol y;

    auto joint = Joint{
        mu | Normal(0.0, 10.0),
        sigma | HalfNormal(5.0),
        y | Normal(mu, sigma)
    };

    auto posterior = joint
        .observe(y = 2.5)
        .infer(mu, sigma);

    double lp = posterior.logProb(1.0, 2.0);
    double expected = logNormal(1.0, 0.0, 10.0) +
                      logHalfNormal(2.0, 5.0) +
                      logNormal(2.5, 1.0, 2.0);

    expectNear(expected, lp, 1e-10);
  };

  return TestRegistry::result();
}
