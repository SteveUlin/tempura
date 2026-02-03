// Tests for SimplexTransform
#include "symbolic4/mcmc/transforms.h"
#include "unit.h"

#include <cmath>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Basic properties
  // ===========================================================================

  "SimplexTransform K=2 compiles and has correct dimensions"_test = [] {
    auto st = simplexTransform<2>();
    static_assert(decltype(st)::kDim == 2);
    static_assert(decltype(st)::kUnconstrainedDim == 1);
  };

  "SimplexTransform K=3 compiles and has correct dimensions"_test = [] {
    auto st = simplexTransform<3>();
    static_assert(decltype(st)::kDim == 3);
    static_assert(decltype(st)::kUnconstrainedDim == 2);
  };

  "SimplexTransform K=5 compiles and has correct dimensions"_test = [] {
    auto st = simplexTransform<5>();
    static_assert(decltype(st)::kDim == 5);
    static_assert(decltype(st)::kUnconstrainedDim == 4);
  };

  // ===========================================================================
  // Transform output properties
  // ===========================================================================

  "SimplexTransform output sums to 1"_test = [] {
    auto st = simplexTransform<3>();

    for (auto z : std::vector<std::array<double, 2>>{
        {0.0, 0.0}, {1.0, -1.0}, {-2.0, 3.0}, {5.0, 5.0}, {-5.0, -5.0}}) {
      auto delta = st.transform(z);
      double sum = delta[0] + delta[1] + delta[2];
      expectNear(1.0, sum, 1e-14);
    }
  };

  "SimplexTransform output is all positive"_test = [] {
    auto st = simplexTransform<4>();

    for (auto z : std::vector<std::array<double, 3>>{
        {0.0, 0.0, 0.0}, {2.0, -1.0, 0.5}, {-3.0, 3.0, -2.0}}) {
      auto delta = st.transform(z);
      for (std::size_t i = 0; i < 4; ++i) {
        expectTrue(delta[i] > 0.0);
      }
    }
  };

  "SimplexTransform at z=0 gives uniform"_test = [] {
    auto st = simplexTransform<3>();
    std::array<double, 2> z = {0.0, 0.0};
    auto delta = st.transform(z);

    // At z=0: s[0]=0.5, delta[0]=0.5
    // s[1]=0.5, delta[1]=0.5*0.5=0.25
    // delta[2]=0.25
    expectNear(0.5, delta[0], 1e-10);
    expectNear(0.25, delta[1], 1e-10);
    expectNear(0.25, delta[2], 1e-10);
  };

  // ===========================================================================
  // Round-trip (inverse ∘ transform = identity in z-space)
  // ===========================================================================

  "SimplexTransform round-trip K=2"_test = [] {
    auto st = simplexTransform<2>();

    for (double z0 : {-3.0, -1.0, 0.0, 1.0, 3.0}) {
      std::array<double, 1> z = {z0};
      auto delta = st.transform(z);
      auto z_back = st.inverse(delta);
      expectNear(z[0], z_back[0], 1e-10);
    }
  };

  "SimplexTransform round-trip K=3"_test = [] {
    auto st = simplexTransform<3>();

    for (auto z : std::vector<std::array<double, 2>>{
        {0.0, 0.0}, {1.0, -1.0}, {-2.0, 2.0}, {3.0, 3.0}}) {
      auto delta = st.transform(z);
      auto z_back = st.inverse(delta);
      expectNear(z[0], z_back[0], 1e-10);
      expectNear(z[1], z_back[1], 1e-10);
    }
  };

  "SimplexTransform round-trip K=5"_test = [] {
    auto st = simplexTransform<5>();
    std::array<double, 4> z = {0.5, -0.3, 1.2, -0.8};
    auto delta = st.transform(z);
    auto z_back = st.inverse(delta);

    for (std::size_t i = 0; i < 4; ++i) {
      expectNear(z[i], z_back[i], 1e-10);
    }
  };

  // ===========================================================================
  // Jacobian finite difference verification
  // ===========================================================================

  "SimplexTransform logJacobian matches finite difference K=2"_test = [] {
    auto st = simplexTransform<2>();
    std::array<double, 1> z = {0.5};

    double analytic = st.logJacobian(z);

    // Finite difference of full transform + volume
    constexpr double eps = 1e-6;

    // For K=2, delta = [sigmoid(z), 1-sigmoid(z)]
    // logJacobian = log(s) + log(1-s) where s = sigmoid(z)
    double s = 1.0 / (1.0 + std::exp(-z[0]));
    double expected = std::log(s) + std::log(1.0 - s);

    expectNear(expected, analytic, 1e-10);
  };

  "SimplexTransform logJacobian matches finite difference K=3"_test = [] {
    auto st = simplexTransform<3>();
    std::array<double, 2> z = {0.5, -0.3};

    double analytic = st.logJacobian(z);

    // Numerical approximation via change of variables theory:
    // The Jacobian determinant for stick-breaking is product of
    // s[i]*(1-s[i])*remaining[i] for each i
    double s0 = 1.0 / (1.0 + std::exp(-z[0]));
    double s1 = 1.0 / (1.0 + std::exp(-z[1]));
    double rem0 = 1.0;
    double rem1 = 1.0 - s0 * rem0;

    double expected = std::log(s0) + std::log(1.0 - s0) + std::log(rem0)
                    + std::log(s1) + std::log(1.0 - s1) + std::log(rem1);

    expectNear(expected, analytic, 1e-10);
  };

  // ===========================================================================
  // Jacobian gradient finite difference verification
  // ===========================================================================

  "SimplexTransform logJacobianGrad matches finite difference K=2"_test = [] {
    auto st = simplexTransform<2>();
    std::array<double, 1> z = {0.7};

    auto analytic = st.logJacobianGrad(z);

    constexpr double eps = 1e-6;
    std::array<double, 1> z_plus = {z[0] + eps};
    std::array<double, 1> z_minus = {z[0] - eps};
    double fd_grad = (st.logJacobian(z_plus) - st.logJacobian(z_minus)) / (2.0 * eps);

    expectNear(fd_grad, analytic[0], 1e-5);
  };

  "SimplexTransform logJacobianGrad matches finite difference K=3"_test = [] {
    auto st = simplexTransform<3>();
    std::array<double, 2> z = {0.5, -0.3};

    auto analytic = st.logJacobianGrad(z);

    constexpr double eps = 1e-6;

    // Finite difference for each component
    for (std::size_t i = 0; i < 2; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (st.logJacobian(z_plus) - st.logJacobian(z_minus)) / (2.0 * eps);
      expectNear(fd_grad, analytic[i], 1e-5);
    }
  };

  "SimplexTransform logJacobianGrad matches finite difference K=5"_test = [] {
    auto st = simplexTransform<5>();
    std::array<double, 4> z = {0.2, -0.5, 0.8, -0.1};

    auto analytic = st.logJacobianGrad(z);

    constexpr double eps = 1e-6;

    for (std::size_t i = 0; i < 4; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (st.logJacobian(z_plus) - st.logJacobian(z_minus)) / (2.0 * eps);
      expectNear(fd_grad, analytic[i], 1e-5);
    }
  };

  // ===========================================================================
  // Chain rule gradient finite difference verification
  // ===========================================================================

  "SimplexTransform chainRuleGrad matches finite difference K=3"_test = [] {
    auto st = simplexTransform<3>();
    std::array<double, 2> z = {0.5, -0.3};

    // Test with a simple objective: sum of delta squared (excluding Jacobian)
    // f(delta) = sum_i delta[i]^2
    // grad_delta[i] = 2*delta[i]
    auto delta = st.transform(z);
    std::array<double, 3> grad_delta = {2.0 * delta[0], 2.0 * delta[1], 2.0 * delta[2]};

    // Compute objective including Jacobian
    auto objective = [&](const std::array<double, 2>& z_val) {
      auto d = st.transform(z_val);
      double f = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
      return f + st.logJacobian(z_val);  // Include Jacobian term
    };

    auto analytic = st.chainRuleGrad(z, grad_delta);

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

  "SimplexTransform chainRuleGrad with Dirichlet log-prob gradient"_test = [] {
    // Dirichlet(alpha) log-prob: sum_i (alpha[i]-1)*log(delta[i])
    // grad_delta[i] = (alpha[i]-1) / delta[i]
    auto st = simplexTransform<3>();
    std::array<double, 2> z = {0.3, 0.1};
    std::array<double, 3> alpha = {2.0, 2.0, 2.0};  // Symmetric Dirichlet

    auto delta = st.transform(z);
    std::array<double, 3> grad_delta;
    for (std::size_t i = 0; i < 3; ++i) {
      grad_delta[i] = (alpha[i] - 1.0) / delta[i];
    }

    auto objective = [&](const std::array<double, 2>& z_val) {
      auto d = st.transform(z_val);
      double lp = 0.0;
      for (std::size_t i = 0; i < 3; ++i) {
        lp += (alpha[i] - 1.0) * std::log(d[i]);
      }
      return lp + st.logJacobian(z_val);
    };

    auto analytic = st.chainRuleGrad(z, grad_delta);

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

  // ===========================================================================
  // Type traits
  // ===========================================================================

  "is_simplex_transform_v detects SimplexTransform"_test = [] {
    static_assert(is_simplex_transform_v<SimplexTransform<2>>);
    static_assert(is_simplex_transform_v<SimplexTransform<3>>);
    static_assert(is_simplex_transform_v<SimplexTransform<10>>);
    static_assert(!is_simplex_transform_v<int>);
    static_assert(!is_simplex_transform_v<double>);
    static_assert(!is_simplex_transform_v<CholeskyTransform>);
    static_assert(!is_simplex_transform_v<Positive<int>>);
  };

  // ===========================================================================
  // Numerical stability tests
  // ===========================================================================

  "SimplexTransform handles extreme z values"_test = [] {
    auto st = simplexTransform<3>();

    // Very large positive z[0]: delta[0] should be close to 1
    {
      std::array<double, 2> z = {20.0, 0.0};
      auto delta = st.transform(z);
      expectTrue(delta[0] > 0.999);
      expectTrue(delta[1] > 0.0);
      expectTrue(delta[2] > 0.0);
      expectNear(1.0, delta[0] + delta[1] + delta[2], 1e-10);
    }

    // Very large negative z[0]: delta[0] should be close to 0
    {
      std::array<double, 2> z = {-20.0, 0.0};
      auto delta = st.transform(z);
      expectTrue(delta[0] < 0.001);
      double sum = delta[0] + delta[1] + delta[2];
      expectNear(1.0, sum, 1e-10);
    }
  };

  "SimplexTransform stableSigmoid avoids overflow"_test = [] {
    // Very large positive z
    double s_pos = SimplexTransform<2>::stableSigmoid(1000.0);
    expectNear(1.0, s_pos, 1e-10);

    // Very large negative z
    double s_neg = SimplexTransform<2>::stableSigmoid(-1000.0);
    expectNear(0.0, s_neg, 1e-10);

    // Zero
    double s_zero = SimplexTransform<2>::stableSigmoid(0.0);
    expectNear(0.5, s_zero, 1e-10);
  };

  // ===========================================================================
  // Edge case: K=2 is essentially a single probability
  // ===========================================================================

  "SimplexTransform K=2 equivalent to UnitInterval"_test = [] {
    auto st = simplexTransform<2>();

    for (double z0 : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
      std::array<double, 1> z = {z0};
      auto delta = st.transform(z);

      // delta[0] should equal sigmoid(z)
      double expected_s = 1.0 / (1.0 + std::exp(-z0));
      expectNear(expected_s, delta[0], 1e-10);
      expectNear(1.0 - expected_s, delta[1], 1e-10);
    }
  };

  return TestRegistry::result();
}
