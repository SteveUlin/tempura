#include "symbolic4/mcmc/transform_pack.h"

#include <cmath>
#include <vector>

#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/mcmc/plate_transforms.h"
#include "symbolic4/mcmc/support.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "TransformPack scalar-only transform round-trip"_test = [] {
    auto alpha = normal(0_c, 10_c);  // Real → unconstrained
    auto sigma = halfNormal(2_c);    // Positive → exp transform

    auto specs = std::make_tuple(
        ScalarParamSpec<decltype(alpha), Unconstrained<decltype(alpha)>>{
            Unconstrained<decltype(alpha)>{alpha}},
        ScalarParamSpec<decltype(sigma), Positive<decltype(sigma)>>{
            Positive<decltype(sigma)>{sigma}});

    auto symbols = std::make_tuple(alpha.freeSym(), sigma.freeSym());
    auto pack = makeTransformPack(symbols, specs);

    expectEq(2u, pack.stateDim());

    // z = {1.0, 0.5}
    std::vector<double> z{1.0, 0.5};

    // Transform: alpha stays 1.0, sigma → exp(0.5)
    auto x = pack.transform(z);
    expectNear(1.0, x[0], 1e-10);
    expectNear(std::exp(0.5), x[1], 1e-10);

    // Inverse round-trips
    auto z_back = pack.inverse(x);
    expectNear(z[0], z_back[0], 1e-10);
    expectNear(z[1], z_back[1], 1e-10);
  };

  "TransformPack logJacobian"_test = [] {
    auto sigma = halfNormal(2_c);  // Positive → log-Jacobian = z

    auto specs = std::make_tuple(
        ScalarParamSpec<decltype(sigma), Positive<decltype(sigma)>>{
            Positive<decltype(sigma)>{sigma}});

    auto symbols = std::make_tuple(sigma.freeSym());
    auto pack = makeTransformPack(symbols, specs);

    std::vector<double> z{0.7};
    double lj = pack.logJacobian(z);

    // For Positive transform: logJacobian = z
    expectNear(0.7, lj, 1e-10);
  };

  "TransformPack chainRuleGrad"_test = [] {
    auto alpha = normal(0_c, 10_c);
    auto sigma = halfNormal(2_c);

    auto specs = std::make_tuple(
        ScalarParamSpec<decltype(alpha), Unconstrained<decltype(alpha)>>{
            Unconstrained<decltype(alpha)>{alpha}},
        ScalarParamSpec<decltype(sigma), Positive<decltype(sigma)>>{
            Positive<decltype(sigma)>{sigma}});

    auto symbols = std::make_tuple(alpha.freeSym(), sigma.freeSym());
    auto pack = makeTransformPack(symbols, specs);

    std::vector<double> z{1.0, 0.5};
    std::vector<double> grad_x{2.0, 3.0};

    auto grad_z = pack.chainRuleGrad(grad_x, z);

    // Unconstrained: grad_z = grad_x (identity chain rule)
    expectNear(2.0, grad_z[0], 1e-10);

    // Positive: grad_z = grad_x * exp(z) + 1.0
    // dx/dz = exp(z), d(logJ)/dz = 1
    double expected_sigma_grad = 3.0 * std::exp(0.5) + 1.0;
    expectNear(expected_sigma_grad, grad_z[1], 1e-10);
  };

  "TransformPack with indexed params"_test = [] {
    struct Countries {};

    auto sigma = halfNormal(2_c);
    auto theta = plate(beta(2.0_c, 3.0_c), Countries{});

    // Build specs like the posterior does
    auto sigma_spec = ScalarParamSpec<decltype(sigma), Positive<decltype(sigma)>>{
        Positive<decltype(sigma)>{sigma}};
    auto theta_spec = IndexedParamSpec<decltype(theta),
                                        UnitInterval<decltype(theta)>,
                                        typename decltype(theta)::dims_list>{
        UnitInterval<decltype(theta)>{theta}, 3};  // 3 countries

    auto specs = std::make_tuple(sigma_spec, theta_spec);
    auto symbols = std::make_tuple(sigma.freeSym(), theta.freeSym());
    auto pack = makeTransformPack(symbols, specs);

    // State: [z_sigma, z_theta[0], z_theta[1], z_theta[2]]
    expectEq(4u, pack.stateDim());

    // Transform
    std::vector<double> z{0.5, 0.0, 1.0, -1.0};
    auto x = pack.transform(z);

    expectNear(std::exp(0.5), x[0], 1e-10);  // sigma = exp(z)

    // theta values: logistic transform (sigmoid)
    auto sigmoid = [](double z) { return 1.0 / (1.0 + std::exp(-z)); };
    expectNear(sigmoid(0.0), x[1], 1e-10);   // theta[0]
    expectNear(sigmoid(1.0), x[2], 1e-10);   // theta[1]
    expectNear(sigmoid(-1.0), x[3], 1e-10);  // theta[2]

    // Round-trip
    auto z_back = pack.inverse(x);
    for (std::size_t i = 0; i < 4; ++i) {
      expectNear(z[i], z_back[i], 1e-8);
    }
  };

  "TransformPack symbol lookup offset/size"_test = [] {
    struct Countries {};

    auto alpha = normal(0_c, 10_c);
    auto theta = plate(beta(2.0_c, 3.0_c), Countries{});

    auto alpha_spec = ScalarParamSpec<decltype(alpha), Unconstrained<decltype(alpha)>>{
        Unconstrained<decltype(alpha)>{alpha}};
    auto theta_spec = IndexedParamSpec<decltype(theta),
                                        UnitInterval<decltype(theta)>,
                                        typename decltype(theta)::dims_list>{
        UnitInterval<decltype(theta)>{theta}, 5};

    auto specs = std::make_tuple(alpha_spec, theta_spec);
    auto symbols = std::make_tuple(alpha.freeSym(), theta.freeSym());
    auto pack = makeTransformPack(symbols, specs);

    expectEq(6u, pack.stateDim());
    expectEq(0u, pack.offset(alpha));
    expectEq(1u, pack.paramSize(alpha));
    expectEq(1u, pack.offset(theta));
    expectEq(5u, pack.paramSize(theta));
  };

  return TestRegistry::result();
}
