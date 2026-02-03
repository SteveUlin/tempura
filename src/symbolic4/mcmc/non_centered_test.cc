// Tests for non-centered parameterization helper
#include "symbolic4/mcmc/non_centered.h"
#include "unit.h"

#include <array>
#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Basic functionality tests
  // ===========================================================================

  "NonCenteredParam scale() returns exp(log_scale)"_test = [] {
    // State: [location, log_scale, z0, z1, z2]
    //         0         1          2   3   4
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    std::array<double, 5> state = {0.0, 0.5, 0.0, 0.0, 0.0};
    expectNear(std::exp(0.5), ncp.scale(state), 1e-10);

    state[1] = -1.0;
    expectNear(std::exp(-1.0), ncp.scale(state), 1e-10);

    state[1] = 0.0;
    expectNear(1.0, ncp.scale(state), 1e-10);
  };

  "NonCenteredParam param() returns location + scale * z[j]"_test = [] {
    // State: [location, log_scale, z0, z1, z2]
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    // location = 2.0, log_scale = 0 (scale = 1), z = [1.0, -0.5, 2.0]
    std::array<double, 5> state = {2.0, 0.0, 1.0, -0.5, 2.0};

    // param[0] = 2.0 + 1.0 * 1.0 = 3.0
    expectNear(3.0, ncp.param(state, 0), 1e-10);

    // param[1] = 2.0 + 1.0 * (-0.5) = 1.5
    expectNear(1.5, ncp.param(state, 1), 1e-10);

    // param[2] = 2.0 + 1.0 * 2.0 = 4.0
    expectNear(4.0, ncp.param(state, 2), 1e-10);
  };

  "NonCenteredParam param() with non-unit scale"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    // location = 1.0, log_scale = log(2) (scale = 2), z = [1.5, -1.0]
    std::array<double, 4> state = {1.0, std::log(2.0), 1.5, -1.0};

    // param[0] = 1.0 + 2.0 * 1.5 = 4.0
    expectNear(4.0, ncp.param(state, 0), 1e-10);

    // param[1] = 1.0 + 2.0 * (-1.0) = -1.0
    expectNear(-1.0, ncp.param(state, 1), 1e-10);
  };

  "NonCenteredParam z() returns z[j] from state"_test = [] {
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    std::array<double, 5> state = {0.0, 0.0, 1.5, -2.0, 0.5};

    expectNear(1.5, ncp.z(state, 0), 1e-10);
    expectNear(-2.0, ncp.z(state, 1), 1e-10);
    expectNear(0.5, ncp.z(state, 2), 1e-10);
  };

  "NonCenteredParam zLogPrior() returns -0.5 * z^2"_test = [] {
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    std::array<double, 5> state = {0.0, 0.0, 1.0, -2.0, 0.0};

    // z[0] = 1.0: log prior = -0.5 * 1^2 = -0.5
    expectNear(-0.5, ncp.zLogPrior(state, 0), 1e-10);

    // z[1] = -2.0: log prior = -0.5 * 4 = -2.0
    expectNear(-2.0, ncp.zLogPrior(state, 1), 1e-10);

    // z[2] = 0.0: log prior = -0.5 * 0 = 0.0
    expectNear(0.0, ncp.zLogPrior(state, 2), 1e-10);
  };

  // ===========================================================================
  // Gradient tests
  // ===========================================================================

  "NonCenteredParam addParamGrad() updates correct indices"_test = [] {
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    // State: location = 1.0, log_scale = log(2), z = [1.0, 0.5, -1.0]
    std::array<double, 5> state = {1.0, std::log(2.0), 1.0, 0.5, -1.0};
    std::array<double, 5> grad = {0.0, 0.0, 0.0, 0.0, 0.0};

    double df_dparam = 3.0;  // Upstream gradient
    double scale = 2.0;
    double z_1 = 0.5;

    ncp.addParamGrad(grad, 1, df_dparam, state);

    // grad[location] += df_dparam = 3.0
    expectNear(3.0, grad[0], 1e-10);

    // grad[log_scale] += df_dparam * scale * z[1] = 3.0 * 2.0 * 0.5 = 3.0
    expectNear(3.0, grad[1], 1e-10);

    // grad[z_offset + 1] += df_dparam * scale = 3.0 * 2.0 = 6.0
    expectNear(6.0, grad[3], 1e-10);

    // Other z gradients should be unchanged
    expectNear(0.0, grad[2], 1e-10);
    expectNear(0.0, grad[4], 1e-10);
  };

  "NonCenteredParam addZPriorGrad() adds -z[j]"_test = [] {
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    std::array<double, 5> state = {0.0, 0.0, 1.5, -2.0, 0.5};
    std::array<double, 5> grad = {0.0, 0.0, 0.0, 0.0, 0.0};

    ncp.addZPriorGrad(grad, 0, state);
    expectNear(-1.5, grad[2], 1e-10);

    ncp.addZPriorGrad(grad, 1, state);
    expectNear(2.0, grad[3], 1e-10);  // -(-2.0) = 2.0

    ncp.addZPriorGrad(grad, 2, state);
    expectNear(-0.5, grad[4], 1e-10);
  };

  // ===========================================================================
  // Gradient matches finite difference tests
  // ===========================================================================

  "NonCenteredParam gradient matches finite difference for location"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    std::array<double, 4> state = {1.0, std::log(2.0), 1.5, -0.5};
    std::array<double, 4> grad = {};

    // Compute param[0] = location + scale * z[0]
    // Its gradient w.r.t. location should be 1.0 (by addParamGrad)
    ncp.addParamGrad(grad, 0, 1.0, state);  // df/dparam = 1

    // Finite difference: (param(loc+eps) - param(loc-eps)) / (2*eps)
    constexpr double eps = 1e-6;
    auto state_plus = state;
    auto state_minus = state;
    state_plus[0] += eps;
    state_minus[0] -= eps;

    double fd_grad = (ncp.param(state_plus, 0) - ncp.param(state_minus, 0)) / (2.0 * eps);
    expectNear(fd_grad, grad[0], 1e-5);
  };

  "NonCenteredParam gradient matches finite difference for log_scale"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    std::array<double, 4> state = {1.0, std::log(2.0), 1.5, -0.5};
    std::array<double, 4> grad = {};

    ncp.addParamGrad(grad, 0, 1.0, state);

    constexpr double eps = 1e-6;
    auto state_plus = state;
    auto state_minus = state;
    state_plus[1] += eps;
    state_minus[1] -= eps;

    double fd_grad = (ncp.param(state_plus, 0) - ncp.param(state_minus, 0)) / (2.0 * eps);
    expectNear(fd_grad, grad[1], 1e-5);
  };

  "NonCenteredParam gradient matches finite difference for z[j]"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    std::array<double, 4> state = {1.0, std::log(2.0), 1.5, -0.5};
    std::array<double, 4> grad = {};

    ncp.addParamGrad(grad, 0, 1.0, state);

    constexpr double eps = 1e-6;
    auto state_plus = state;
    auto state_minus = state;
    state_plus[2] += eps;  // z[0]
    state_minus[2] -= eps;

    double fd_grad = (ncp.param(state_plus, 0) - ncp.param(state_minus, 0)) / (2.0 * eps);
    expectNear(fd_grad, grad[2], 1e-5);
  };

  "NonCenteredParam zLogPrior gradient matches finite difference"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    std::array<double, 4> state = {0.0, 0.0, 1.5, -0.5};

    // Analytic gradient of z prior: -z[j]
    double analytic_grad = -state[2];  // -z[0] = -1.5

    // Finite difference
    constexpr double eps = 1e-6;
    auto state_plus = state;
    auto state_minus = state;
    state_plus[2] += eps;
    state_minus[2] -= eps;

    double fd_grad = (ncp.zLogPrior(state_plus, 0) - ncp.zLogPrior(state_minus, 0)) / (2.0 * eps);
    expectNear(fd_grad, analytic_grad, 1e-5);
  };

  // ===========================================================================
  // Chain rule test (combining param grad and prior grad)
  // ===========================================================================

  "NonCenteredParam combined gradient matches finite difference"_test = [] {
    // Test a typical use case: log posterior includes both likelihood and z prior
    // log_post = f(param[j]) - 0.5 * z[j]^2
    // where f(x) = -0.5 * (x - 5)^2 for simplicity (normal likelihood centered at 5)

    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    auto log_post = [&](std::span<const double> s) {
      double p0 = ncp.param(s, 0);
      double likelihood = -0.5 * (p0 - 5.0) * (p0 - 5.0);
      double prior = ncp.zLogPrior(s, 0);
      return likelihood + prior;
    };

    std::array<double, 4> state = {1.0, std::log(2.0), 1.5, -0.5};
    std::array<double, 4> grad = {};

    // Compute analytic gradient
    double p0 = ncp.param(state, 0);
    double df_dparam = -(p0 - 5.0);  // d/dp[-0.5*(p-5)^2] = -(p-5)
    ncp.addParamGrad(grad, 0, df_dparam, state);
    ncp.addZPriorGrad(grad, 0, state);

    // Verify each component against finite difference
    constexpr double eps = 1e-6;

    for (std::size_t i = 0; i < 4; ++i) {
      auto state_plus = state;
      auto state_minus = state;
      state_plus[i] += eps;
      state_minus[i] -= eps;

      double fd = (log_post(state_plus) - log_post(state_minus)) / (2.0 * eps);
      expectNear(fd, grad[i], 1e-4);
    }
  };

  // ===========================================================================
  // Edge cases
  // ===========================================================================

  "NonCenteredParam with very small scale"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    // log_scale = -20 gives scale ~ 2e-9, so effect is negligible
    std::array<double, 4> state = {5.0, -20.0, 100.0, -100.0};
    double scale = std::exp(-20.0);

    // param[0] = 5.0 + scale * 100.0 ~ 5.0 + 2e-7
    double p0 = ncp.param(state, 0);
    double expected = 5.0 + scale * 100.0;
    expectNear(expected, p0, 1e-15);

    // Verify it's very close to location
    expectNear(5.0, p0, 1e-6);
  };

  "NonCenteredParam with large scale"_test = [] {
    constexpr auto ncp = nonCenteredParam<2>(0, 1, 2);

    std::array<double, 4> state = {0.0, std::log(10.0), 2.0, -1.0};

    // param[0] = 0 + 10 * 2 = 20
    expectNear(20.0, ncp.param(state, 0), 1e-10);

    // param[1] = 0 + 10 * (-1) = -10
    expectNear(-10.0, ncp.param(state, 1), 1e-10);
  };

  "NonCenteredParam accumulates gradients correctly"_test = [] {
    constexpr auto ncp = nonCenteredParam<3>(0, 1, 2);

    std::array<double, 5> state = {0.0, 0.0, 1.0, 2.0, 3.0};  // scale = 1
    std::array<double, 5> grad = {0.0, 0.0, 0.0, 0.0, 0.0};

    // Add gradients from multiple j values
    ncp.addParamGrad(grad, 0, 1.0, state);
    ncp.addParamGrad(grad, 1, 1.0, state);
    ncp.addParamGrad(grad, 2, 1.0, state);

    // grad[location] = 1 + 1 + 1 = 3
    expectNear(3.0, grad[0], 1e-10);

    // grad[log_scale] = 1*1*1 + 1*1*2 + 1*1*3 = 6 (sum of z values)
    expectNear(6.0, grad[1], 1e-10);

    // Individual z gradients: scale = 1 for each
    expectNear(1.0, grad[2], 1e-10);
    expectNear(1.0, grad[3], 1e-10);
    expectNear(1.0, grad[4], 1e-10);
  };

  return TestRegistry::result();
}
