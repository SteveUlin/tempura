// Tests for DirichletSimplex
#include "symbolic4/distributions/dirichlet_simplex.h"
#include "unit.h"

#include <cmath>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Basic construction
  // ===========================================================================

  "DirichletSimplex K=3 constructs with valid alpha"_test = [] {
    auto dir = dirichletSimplex<3>({2.0, 2.0, 2.0});
    expectEq(dir.alpha_[0], 2.0);
    expectEq(dir.alpha_[1], 2.0);
    expectEq(dir.alpha_[2], 2.0);
  };

  "DirichletSimplex K=4 constructs with asymmetric alpha"_test = [] {
    auto dir = dirichletSimplex<4>({1.0, 2.0, 0.5, 3.0});
    expectEq(dir.alpha_[0], 1.0);
    expectEq(dir.alpha_[1], 2.0);
    expectEq(dir.alpha_[2], 0.5);
    expectEq(dir.alpha_[3], 3.0);
  };

  // ===========================================================================
  // Transform (delegates to SimplexTransform)
  // ===========================================================================

  "DirichletSimplex transform produces valid simplex"_test = [] {
    auto dir = dirichletSimplex<3>({2.0, 2.0, 2.0});

    for (auto z : std::vector<std::array<double, 2>>{
        {0.0, 0.0}, {1.0, -1.0}, {-2.0, 3.0}}) {
      auto delta = dir.transform(z);

      // All positive
      for (std::size_t i = 0; i < 3; ++i) {
        expectTrue(delta[i] > 0.0);
      }
      // Sum to 1
      double sum = delta[0] + delta[1] + delta[2];
      expectNear(1.0, sum, 1e-14);
    }
  };

  // ===========================================================================
  // Round-trip (inverse o transform = identity)
  // ===========================================================================

  "DirichletSimplex round-trip K=3"_test = [] {
    auto dir = dirichletSimplex<3>({2.0, 3.0, 1.5});

    for (auto z : std::vector<std::array<double, 2>>{
        {0.0, 0.0}, {1.0, -1.0}, {-2.0, 2.0}, {3.0, 3.0}}) {
      auto delta = dir.transform(z);
      auto z_back = dir.inverse(delta);
      expectNear(z[0], z_back[0], 1e-10);
      expectNear(z[1], z_back[1], 1e-10);
    }
  };

  "DirichletSimplex round-trip K=5"_test = [] {
    auto dir = dirichletSimplex<5>({1.0, 2.0, 3.0, 4.0, 5.0});
    std::array<double, 4> z = {0.5, -0.3, 1.2, -0.8};
    auto delta = dir.transform(z);
    auto z_back = dir.inverse(delta);

    for (std::size_t i = 0; i < 4; ++i) {
      expectNear(z[i], z_back[i], 1e-10);
    }
  };

  // ===========================================================================
  // logProbWithJacobian matches manual computation
  // ===========================================================================

  "logProbWithJacobian matches manual K=3 symmetric"_test = [] {
    std::array<double, 3> alpha = {2.0, 2.0, 2.0};
    auto dir = dirichletSimplex<3>(alpha);
    std::array<double, 2> z = {0.3, 0.1};

    // Compute manually
    auto delta = dir.transform(z);

    // Dirichlet log-prob: sum_i (alpha[i]-1)*log(delta[i]) + normalizing constant
    double log_dirichlet = 0.0;
    double alpha_sum = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
      log_dirichlet += (alpha[i] - 1.0) * std::log(delta[i]);
      log_dirichlet -= std::lgamma(alpha[i]);
      alpha_sum += alpha[i];
    }
    log_dirichlet += std::lgamma(alpha_sum);

    // Stick-breaking Jacobian
    double log_jacobian = dir.transform_.logJacobian(z);

    double expected = log_dirichlet + log_jacobian;
    double actual = dir.logProbWithJacobian(z);

    expectNear(expected, actual, 1e-10);
  };

  "logProbWithJacobian matches manual K=4 asymmetric"_test = [] {
    std::array<double, 4> alpha = {0.5, 2.0, 1.5, 3.0};
    auto dir = dirichletSimplex<4>(alpha);
    std::array<double, 3> z = {-0.5, 0.7, 0.2};

    auto delta = dir.transform(z);

    double log_dirichlet = 0.0;
    double alpha_sum = 0.0;
    for (std::size_t i = 0; i < 4; ++i) {
      log_dirichlet += (alpha[i] - 1.0) * std::log(delta[i]);
      log_dirichlet -= std::lgamma(alpha[i]);
      alpha_sum += alpha[i];
    }
    log_dirichlet += std::lgamma(alpha_sum);

    double log_jacobian = dir.transform_.logJacobian(z);

    double expected = log_dirichlet + log_jacobian;
    double actual = dir.logProbWithJacobian(z);

    expectNear(expected, actual, 1e-10);
  };

  // ===========================================================================
  // Dirichlet(1,1,...,1) should have flat log-prob (uniform on simplex)
  // ===========================================================================

  "Dirichlet(1,1,1) is uniform - Dirichlet component is constant"_test = [] {
    std::array<double, 3> alpha = {1.0, 1.0, 1.0};
    auto dir = dirichletSimplex<3>(alpha);

    // For alpha_i = 1, the Dirichlet log-prob is constant (only normalizing const)
    // The variation comes only from the Jacobian
    std::array<double, 2> z1 = {0.0, 0.0};
    std::array<double, 2> z2 = {1.0, -1.0};

    auto delta1 = dir.transform(z1);
    auto delta2 = dir.transform(z2);

    // Dirichlet kernel = sum_i (1-1)*log(delta[i]) = 0 for all delta
    double kernel1 = 0.0;
    double kernel2 = 0.0;
    for (std::size_t i = 0; i < 3; ++i) {
      kernel1 += (alpha[i] - 1.0) * std::log(delta1[i]);
      kernel2 += (alpha[i] - 1.0) * std::log(delta2[i]);
    }

    expectNear(kernel1, kernel2, 1e-10);
  };

  // ===========================================================================
  // chainRuleGrad against finite difference
  // ===========================================================================

  "chainRuleGrad with zero external gradient matches logProb finite diff K=3"_test = [] {
    std::array<double, 3> alpha = {2.0, 3.0, 1.5};
    auto dir = dirichletSimplex<3>(alpha);
    std::array<double, 2> z = {0.5, -0.3};
    std::array<double, 3> zero_grad = {0.0, 0.0, 0.0};

    auto analytic = dir.chainRuleGrad(z, zero_grad);

    constexpr double eps = 1e-6;
    for (std::size_t i = 0; i < 2; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (dir.logProbWithJacobian(z_plus) -
                        dir.logProbWithJacobian(z_minus)) / (2.0 * eps);
      expectNear(fd_grad, analytic[i], 1e-5);
    }
  };

  "chainRuleGrad with external gradient matches finite diff K=3"_test = [] {
    std::array<double, 3> alpha = {2.0, 2.0, 2.0};
    auto dir = dirichletSimplex<3>(alpha);
    std::array<double, 2> z = {0.3, 0.1};

    // External objective: f(delta) = sum_i delta[i]^2
    // grad_delta[i] = 2 * delta[i]
    auto delta = dir.transform(z);
    std::array<double, 3> grad_delta = {2.0 * delta[0], 2.0 * delta[1], 2.0 * delta[2]};

    auto objective = [&](const std::array<double, 2>& z_val) {
      auto d = dir.transform(z_val);
      double f = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
      return f + dir.logProbWithJacobian(z_val);
    };

    auto analytic = dir.chainRuleGrad(z, grad_delta);

    constexpr double eps = 1e-6;
    for (std::size_t i = 0; i < 2; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (objective(z_plus) - objective(z_minus)) / (2.0 * eps);
      expectNear(fd_grad, analytic[i], 1e-5);
    }
  };

  "chainRuleGrad K=5 matches finite diff"_test = [] {
    std::array<double, 5> alpha = {1.5, 2.0, 0.8, 3.0, 1.2};
    auto dir = dirichletSimplex<5>(alpha);
    std::array<double, 4> z = {0.2, -0.5, 0.8, -0.1};

    // External objective: f(delta) = sum_i log(delta[i]) (entropy-like)
    // grad_delta[i] = 1 / delta[i]
    auto delta = dir.transform(z);
    std::array<double, 5> grad_delta;
    for (std::size_t i = 0; i < 5; ++i) {
      grad_delta[i] = 1.0 / delta[i];
    }

    auto objective = [&](const std::array<double, 4>& z_val) {
      auto d = dir.transform(z_val);
      double f = 0.0;
      for (std::size_t i = 0; i < 5; ++i) {
        f += std::log(d[i]);
      }
      return f + dir.logProbWithJacobian(z_val);
    };

    auto analytic = dir.chainRuleGrad(z, grad_delta);

    constexpr double eps = 1e-6;
    for (std::size_t i = 0; i < 4; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (objective(z_plus) - objective(z_minus)) / (2.0 * eps);
      expectNear(fd_grad, analytic[i], 1e-5);
    }
  };

  // ===========================================================================
  // logProbGrad (gradient of Dirichlet+Jacobian only)
  // ===========================================================================

  "logProbGrad matches finite diff K=3"_test = [] {
    std::array<double, 3> alpha = {2.0, 3.0, 1.5};
    auto dir = dirichletSimplex<3>(alpha);
    std::array<double, 2> z = {0.7, -0.2};

    auto analytic = dir.logProbGrad(z);

    constexpr double eps = 1e-6;
    for (std::size_t i = 0; i < 2; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (dir.logProbWithJacobian(z_plus) -
                        dir.logProbWithJacobian(z_minus)) / (2.0 * eps);
      expectNear(fd_grad, analytic[i], 1e-5);
    }
  };

  // ===========================================================================
  // Edge cases
  // ===========================================================================

  "DirichletSimplex K=2 works correctly"_test = [] {
    // K=2 is essentially Beta distribution on the first component
    std::array<double, 2> alpha = {2.0, 3.0};
    auto dir = dirichletSimplex<2>(alpha);

    std::array<double, 1> z = {0.5};
    auto delta = dir.transform(z);

    expectNear(1.0, delta[0] + delta[1], 1e-14);

    // Verify logProbWithJacobian finite diff
    auto analytic = dir.logProbGrad(z);
    constexpr double eps = 1e-6;
    std::array<double, 1> z_plus = {z[0] + eps};
    std::array<double, 1> z_minus = {z[0] - eps};
    double fd_grad = (dir.logProbWithJacobian(z_plus) -
                      dir.logProbWithJacobian(z_minus)) / (2.0 * eps);
    expectNear(fd_grad, analytic[0], 1e-5);
  };

  "DirichletSimplex handles small alpha (sparse)"_test = [] {
    // Small alpha leads to concentration at corners
    std::array<double, 3> alpha = {0.1, 0.1, 0.1};
    auto dir = dirichletSimplex<3>(alpha);

    // Various z values should still work
    for (auto z : std::vector<std::array<double, 2>>{
        {0.0, 0.0}, {2.0, 2.0}, {-2.0, -2.0}}) {
      auto delta = dir.transform(z);
      expectNear(1.0, delta[0] + delta[1] + delta[2], 1e-14);
      for (std::size_t i = 0; i < 3; ++i) {
        expectTrue(delta[i] > 0.0);
      }
    }
  };

  "DirichletSimplex handles large alpha (concentrated)"_test = [] {
    // Large alpha leads to concentration at center
    std::array<double, 3> alpha = {100.0, 100.0, 100.0};
    auto dir = dirichletSimplex<3>(alpha);

    // At z=0, should be near center
    std::array<double, 2> z = {0.0, 0.0};
    auto delta = dir.transform(z);
    expectNear(1.0, delta[0] + delta[1] + delta[2], 1e-14);
  };

  // ===========================================================================
  // Numerical stability with extreme z
  // ===========================================================================

  "DirichletSimplex stable with extreme z values"_test = [] {
    std::array<double, 3> alpha = {2.0, 2.0, 2.0};
    auto dir = dirichletSimplex<3>(alpha);

    // Large positive z[0] - delta[0] near 1
    {
      std::array<double, 2> z = {20.0, 0.0};
      auto delta = dir.transform(z);
      expectNear(1.0, delta[0] + delta[1] + delta[2], 1e-10);
      expectTrue(std::isfinite(dir.logProbWithJacobian(z)));
    }

    // Large negative z[0] - delta[0] near 0
    {
      std::array<double, 2> z = {-20.0, 0.0};
      auto delta = dir.transform(z);
      expectNear(1.0, delta[0] + delta[1] + delta[2], 1e-10);
      expectTrue(std::isfinite(dir.logProbWithJacobian(z)));
    }
  };

  return TestRegistry::result();
}
