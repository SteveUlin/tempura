// Tests for numerically stable Bernoulli log-likelihood helpers
#include "symbolic4/mcmc/likelihoods.h"
#include "unit.h"

#include <cmath>
#include <limits>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Sigmoid tests
  // ===========================================================================

  "sigmoid basic values"_test = [] {
    expectNear(0.5, sigmoid(0.0), 1e-10);
    expectNear(1.0 / (1.0 + std::exp(-1.0)), sigmoid(1.0), 1e-10);
    expectNear(1.0 / (1.0 + std::exp(1.0)), sigmoid(-1.0), 1e-10);
  };

  "sigmoid numerical stability at extreme eta"_test = [] {
    // At large positive eta, sigmoid should approach 1.0 without overflow
    expectNear(1.0, sigmoid(1000.0), 1e-10);
    expectNear(1.0, sigmoid(500.0), 1e-10);

    // At large negative eta, sigmoid should approach 0.0 without underflow to NaN
    expectNear(0.0, sigmoid(-1000.0), 1e-10);
    expectNear(0.0, sigmoid(-500.0), 1e-10);

    // Both should be finite
    expectTrue(std::isfinite(sigmoid(1000.0)));
    expectTrue(std::isfinite(sigmoid(-1000.0)));
  };

  // ===========================================================================
  // logSigmoid tests
  // ===========================================================================

  "logSigmoid basic values"_test = [] {
    // At eta=0: sigmoid=0.5, log(0.5) = -log(2)
    expectNear(-std::log(2.0), logSigmoid(0.0), 1e-10);

    // At eta=1: log(sigmoid(1))
    double s1 = 1.0 / (1.0 + std::exp(-1.0));
    expectNear(std::log(s1), logSigmoid(1.0), 1e-10);

    // At eta=-1: log(sigmoid(-1))
    double sm1 = 1.0 / (1.0 + std::exp(1.0));
    expectNear(std::log(sm1), logSigmoid(-1.0), 1e-10);
  };

  "logSigmoid numerical stability at extreme eta"_test = [] {
    // At large positive eta: log(sigmoid) -> 0
    expectNear(0.0, logSigmoid(1000.0), 1e-10);
    expectNear(0.0, logSigmoid(500.0), 1e-10);

    // At large negative eta: log(sigmoid) -> eta (since sigmoid -> exp(eta))
    // More precisely: logSigmoid(eta) = eta - log(1 + exp(eta)) -> eta for eta << 0
    expectNear(-1000.0, logSigmoid(-1000.0), 1e-10);
    expectNear(-500.0, logSigmoid(-500.0), 1e-10);

    // Both should be finite
    expectTrue(std::isfinite(logSigmoid(1000.0)));
    expectTrue(std::isfinite(logSigmoid(-1000.0)));
  };

  // ===========================================================================
  // log1mSigmoid tests
  // ===========================================================================

  "log1mSigmoid basic values"_test = [] {
    // At eta=0: 1 - sigmoid(0) = 0.5, log(0.5) = -log(2)
    expectNear(-std::log(2.0), log1mSigmoid(0.0), 1e-10);

    // At eta=1: log(1 - sigmoid(1))
    double s1 = 1.0 / (1.0 + std::exp(-1.0));
    expectNear(std::log(1.0 - s1), log1mSigmoid(1.0), 1e-10);

    // At eta=-1: log(1 - sigmoid(-1))
    double sm1 = 1.0 / (1.0 + std::exp(1.0));
    expectNear(std::log(1.0 - sm1), log1mSigmoid(-1.0), 1e-10);
  };

  "log1mSigmoid numerical stability at extreme eta"_test = [] {
    // At large positive eta: 1 - sigmoid -> 0, log(1 - sigmoid) -> -eta
    expectNear(-1000.0, log1mSigmoid(1000.0), 1e-10);
    expectNear(-500.0, log1mSigmoid(500.0), 1e-10);

    // At large negative eta: 1 - sigmoid -> 1, log(1 - sigmoid) -> 0
    expectNear(0.0, log1mSigmoid(-1000.0), 1e-10);
    expectNear(0.0, log1mSigmoid(-500.0), 1e-10);

    // Both should be finite
    expectTrue(std::isfinite(log1mSigmoid(1000.0)));
    expectTrue(std::isfinite(log1mSigmoid(-1000.0)));
  };

  // ===========================================================================
  // bernoulliLogLik tests
  // ===========================================================================

  "bernoulliLogLik(eta, 1) == logSigmoid(eta)"_test = [] {
    for (double eta : {-100.0, -10.0, -1.0, 0.0, 1.0, 10.0, 100.0}) {
      expectNear(logSigmoid(eta), bernoulliLogLik(eta, 1.0), 1e-10);
    }
  };

  "bernoulliLogLik(eta, 0) == log1mSigmoid(eta)"_test = [] {
    for (double eta : {-100.0, -10.0, -1.0, 0.0, 1.0, 10.0, 100.0}) {
      expectNear(log1mSigmoid(eta), bernoulliLogLik(eta, 0.0), 1e-10);
    }
  };

  "bernoulliLogLik numerical stability at extreme eta"_test = [] {
    // y=1, eta=1000: should be close to 0
    expectNear(0.0, bernoulliLogLik(1000.0, 1.0), 1e-10);
    expectTrue(std::isfinite(bernoulliLogLik(1000.0, 1.0)));

    // y=1, eta=-1000: should be close to -1000
    expectNear(-1000.0, bernoulliLogLik(-1000.0, 1.0), 1e-10);
    expectTrue(std::isfinite(bernoulliLogLik(-1000.0, 1.0)));

    // y=0, eta=1000: should be close to -1000
    expectNear(-1000.0, bernoulliLogLik(1000.0, 0.0), 1e-10);
    expectTrue(std::isfinite(bernoulliLogLik(1000.0, 0.0)));

    // y=0, eta=-1000: should be close to 0
    expectNear(0.0, bernoulliLogLik(-1000.0, 0.0), 1e-10);
    expectTrue(std::isfinite(bernoulliLogLik(-1000.0, 0.0)));
  };

  "bernoulliLogLik is always non-positive"_test = [] {
    for (double eta : {-100.0, -10.0, -1.0, 0.0, 1.0, 10.0, 100.0}) {
      for (double y : {0.0, 0.3, 0.5, 0.7, 1.0}) {
        expectLE(bernoulliLogLik(eta, y), 0.0);
      }
    }
  };

  // ===========================================================================
  // bernoulliGrad tests
  // ===========================================================================

  "bernoulliGrad basic values"_test = [] {
    // At eta=0: sigmoid(0) = 0.5
    // y=1: grad = 1 - 0.5 = 0.5
    expectNear(0.5, bernoulliGrad(0.0, 1.0), 1e-10);
    // y=0: grad = 0 - 0.5 = -0.5
    expectNear(-0.5, bernoulliGrad(0.0, 0.0), 1e-10);
  };

  "bernoulliGrad numerical stability at extreme eta"_test = [] {
    // At eta=1000: sigmoid -> 1
    // y=1: grad = 1 - 1 = 0
    expectNear(0.0, bernoulliGrad(1000.0, 1.0), 1e-10);
    // y=0: grad = 0 - 1 = -1
    expectNear(-1.0, bernoulliGrad(1000.0, 0.0), 1e-10);

    // At eta=-1000: sigmoid -> 0
    // y=1: grad = 1 - 0 = 1
    expectNear(1.0, bernoulliGrad(-1000.0, 1.0), 1e-10);
    // y=0: grad = 0 - 0 = 0
    expectNear(0.0, bernoulliGrad(-1000.0, 0.0), 1e-10);

    // All should be finite
    expectTrue(std::isfinite(bernoulliGrad(1000.0, 1.0)));
    expectTrue(std::isfinite(bernoulliGrad(1000.0, 0.0)));
    expectTrue(std::isfinite(bernoulliGrad(-1000.0, 1.0)));
    expectTrue(std::isfinite(bernoulliGrad(-1000.0, 0.0)));
  };

  "bernoulliGrad matches finite difference"_test = [] {
    constexpr double eps = 1e-7;
    for (double eta : {-5.0, -1.0, 0.0, 1.0, 5.0}) {
      for (double y : {0.0, 1.0}) {
        double analytic = bernoulliGrad(eta, y);
        double fd = (bernoulliLogLik(eta + eps, y) - bernoulliLogLik(eta - eps, y)) / (2.0 * eps);
        // Finite difference tolerance ~eps
        expectNear(analytic, fd, 1e-5);
      }
    }
  };

  // ===========================================================================
  // Consistency tests
  // ===========================================================================

  "logSigmoid and log1mSigmoid sum to log(sigmoid * (1-sigmoid))"_test = [] {
    for (double eta : {-10.0, -1.0, 0.0, 1.0, 10.0}) {
      double s = sigmoid(eta);
      double expected = std::log(s * (1.0 - s));
      double actual = logSigmoid(eta) + log1mSigmoid(eta);
      expectNear(expected, actual, 1e-10);
    }
  };

  "sigmoid symmetry: sigmoid(-eta) = 1 - sigmoid(eta)"_test = [] {
    for (double eta : {0.1, 1.0, 5.0, 10.0, 100.0}) {
      expectNear(sigmoid(-eta), 1.0 - sigmoid(eta), 1e-10);
    }
  };

  "logSigmoid symmetry: logSigmoid(-eta) = log1mSigmoid(eta)"_test = [] {
    for (double eta : {-100.0, -10.0, -1.0, 0.0, 1.0, 10.0, 100.0}) {
      expectNear(logSigmoid(-eta), log1mSigmoid(eta), 1e-10);
    }
  };

  return TestRegistry::result();
}
