#include "symbolic4/symbolic_state.h"

#include <span>
#include <vector>

#include "symbolic4/distributions/random_var.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/mcmc/plate_transforms.h"
#include "symbolic4/mcmc/support.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  "SymbolicState scalar-only access"_test = [] {
    auto alpha = normal(0.0, 10.0);
    auto beta_rv = normal(0.0, 5.0);

    auto specs = std::make_tuple(
        ScalarParamSpec<decltype(alpha), Unconstrained<decltype(alpha)>>{
            Unconstrained<decltype(alpha)>{alpha}},
        ScalarParamSpec<decltype(beta_rv), Unconstrained<decltype(beta_rv)>>{
            Unconstrained<decltype(beta_rv)>{beta_rv}});

    auto symbols = std::make_tuple(alpha.freeSym(), beta_rv.freeSym());

    SymbolicState state{symbols, specs, 2u};

    // Write via symbol lookup
    state[alpha.freeSym()] = 3.14;
    state[beta_rv.freeSym()] = 2.72;

    // Read back
    expectNear(3.14, state[alpha.freeSym()], 1e-10);
    expectNear(2.72, state[beta_rv.freeSym()], 1e-10);

    // Flat vector access
    expectNear(3.14, state.data()[0], 1e-10);
    expectNear(2.72, state.data()[1], 1e-10);
  };

  "SymbolicState mixed scalar+indexed"_test = [] {
    struct Countries {};

    auto alpha = normal(0.0, 10.0);
    auto theta = plate<Countries>(beta(lit(2.0), lit(3.0)));

    auto alpha_spec = ScalarParamSpec<decltype(alpha), Unconstrained<decltype(alpha)>>{
        Unconstrained<decltype(alpha)>{alpha}};
    auto theta_spec = IndexedParamSpec<decltype(theta),
                                        UnitInterval<decltype(theta)>,
                                        typename decltype(theta)::dims_list>{
        UnitInterval<decltype(theta)>{theta}, 3};

    auto specs = std::make_tuple(alpha_spec, theta_spec);
    auto symbols = std::make_tuple(alpha.freeSym(), theta.freeSym());

    // State: [alpha, theta[0], theta[1], theta[2]]
    SymbolicState state{symbols, specs, 4u};

    // Set scalar
    state[alpha.freeSym()] = 1.5;

    // Set indexed via span
    auto theta_span = state[theta.freeSym()];
    static_assert(std::is_same_v<decltype(theta_span), std::span<double>>);
    theta_span[0] = 0.3;
    theta_span[1] = 0.5;
    theta_span[2] = 0.7;

    // Verify flat vector
    expectNear(1.5, state.data()[0], 1e-10);
    expectNear(0.3, state.data()[1], 1e-10);
    expectNear(0.5, state.data()[2], 1e-10);
    expectNear(0.7, state.data()[3], 1e-10);

    // Verify metadata
    expectEq(0u, state.offset(alpha.freeSym()));
    expectEq(1u, state.paramSize(alpha.freeSym()));
    expectEq(1u, state.offset(theta.freeSym()));
    expectEq(3u, state.paramSize(theta.freeSym()));
    expectEq(2u, state.numSlots());
  };

  "SymbolicState construct from existing data"_test = [] {
    auto mu = normal(0.0, 1.0);

    auto specs = std::make_tuple(
        ScalarParamSpec<decltype(mu), Unconstrained<decltype(mu)>>{
            Unconstrained<decltype(mu)>{mu}});
    auto symbols = std::make_tuple(mu.freeSym());

    std::vector<double> data{42.0};
    SymbolicState state{symbols, specs, std::move(data)};

    expectNear(42.0, state[mu.freeSym()], 1e-10);
  };

  "SymbolicState const access returns const span"_test = [] {
    struct Groups {};

    auto theta = plate<Groups>(normal(lit(0.0), lit(1.0)));

    auto theta_spec = IndexedParamSpec<decltype(theta),
                                        Unconstrained<decltype(theta)>,
                                        typename decltype(theta)::dims_list>{
        Unconstrained<decltype(theta)>{theta}, 2};

    auto specs = std::make_tuple(theta_spec);
    auto symbols = std::make_tuple(theta.freeSym());

    std::vector<double> data{1.0, 2.0};
    const SymbolicState state{symbols, specs, std::move(data)};

    auto span = state[theta.freeSym()];
    static_assert(std::is_same_v<decltype(span), std::span<const double>>);
    expectNear(1.0, span[0], 1e-10);
    expectNear(2.0, span[1], 1e-10);
  };

  return TestRegistry::result();
}
