#include "symbolic4/mcmc/plate_transforms.h"

#include <cmath>
#include <random>
#include <vector>

#include "symbolic4/distributions/collect_log_prob.h"
#include "symbolic4/distributions/joint.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Dimension tags at namespace scope (templates require non-local types)
namespace {
struct TestGroups {};
struct TestCountries2 {};
}  // namespace

auto main() -> int {
  // =========================================================================
  // Basic plate transform setup
  // =========================================================================

  "ScalarParamSpec basic"_test = [] {
    auto sigma = halfNormal(5_c);
    auto t = autoTransform(sigma);

    ScalarParamSpec<decltype(sigma), decltype(t)> spec{t};

    expectEq(spec.size(), 1UL);
    expectFalse(spec.is_indexed);

    // Test transform: exp(0) = 1
    expectNear(spec.transformValue(0.0), 1.0, 1e-10);
    // Jacobian for log transform: z
    expectNear(spec.logJacobian(0.5), 0.5, 1e-10);
  };

  "IndexedParamSpec basic"_test = [] {
    struct Countries {};
    auto alpha = gamma(2_c, 0.1_c);
    auto theta = plate(beta(alpha, 3.0_c), Countries{});
    auto t = autoTransform(theta);

    expectTrue(is_unit_interval_v<decltype(t)>);
  };

  // =========================================================================
  // DimSizes
  // =========================================================================

  "DimSizes single dimension"_test = [] {
    struct Countries {};
    DimSizes<Countries> dims{{38}};
    expectEq(dims.get<Countries>(), 38UL);
  };

  "DimSizes multiple dimensions"_test = [] {
    struct Countries {};
    struct Years {};
    DimSizes<Countries, Years> dims{{38, 10}};
    expectEq(dims.get<Countries>(), 38UL);
    expectEq(dims.get<Years>(), 10UL);
  };

  // =========================================================================
  // Binding-based logProbAt
  // =========================================================================

  "scalar only: logProbAt finite"_test = [] {
    auto alpha = gamma(2_c, 0.1_c);
    auto beta_param = gamma(2_c, 0.1_c);

    auto joint = logProb(alpha, beta_param);
    auto posterior = makePlateTransformedPosterior(joint, alpha, beta_param).build();

    expectEq(posterior.stateDim(), 2UL);

    // logProbAt with constrained-space init values
    double lp = posterior.logProbAt(BinderPack{alpha = 3.0, beta_param = 17.0});
    expectTrue(std::isfinite(lp));
  };

  "with plate: logProbAt and gradientAt finite"_test = [] {
    struct Countries {};
    auto alpha = gamma(2_c, 0.1_c);
    auto theta = plate(beta(alpha, 3.0_c), Countries{});
    auto joint = logProb(alpha, theta);

    auto posterior = makePlateTransformedPosterior(joint, alpha, theta)
                         .withDimension(Countries{}, 5)
                         .build();

    expectEq(posterior.stateDim(), 6UL);

    std::vector<double> theta_init(5);
    for (int i = 0; i < 5; ++i) theta_init[i] = 0.3 + 0.1 * i;

    double lp = posterior.logProbAt(
        BinderPack{alpha = 3.0, theta = std::vector<double>(theta_init)});
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradientAt(
        BinderPack{alpha = 3.0, theta = std::vector<double>(theta_init)});
    // Gradient is a MutableState — check scalar and indexed access
    expectTrue(std::isfinite(grad[alpha]));
    auto g_theta = grad[theta];
    for (std::size_t i = 0; i < 5; ++i)
      expectTrue(std::isfinite(g_theta[i]));
  };

  "hierarchical: logProbAt finite"_test = [] {
    struct Countries {};
    auto alpha = gamma(2.0_c, 0.1_c);
    auto beta_param = gamma(2.0_c, 0.1_c);
    auto theta = plate(beta(alpha, beta_param), Countries{});

    auto joint = logProb(alpha, beta_param, theta);
    auto posterior = makePlateTransformedPosterior(joint, alpha, beta_param, theta)
                         .withDimension(Countries{}, 3)
                         .build();

    expectEq(posterior.stateDim(), 5UL);

    double lp = posterior.logProbAt(BinderPack{
        alpha = 3.0, beta_param = 17.0,
        theta = std::vector<double>{0.5, 0.38, 0.62}});
    expectTrue(std::isfinite(lp));
  };

  // =========================================================================
  // Gradient correctness: analytic matches finite-diff
  // =========================================================================

  "scalar: gradient matches finite-diff"_test = [] {
    auto sigma = halfNormal(2.0_c);
    auto posterior = makePlateTransformedPosterior(logProb(sigma), sigma).build();

    expectTrue(posterior.debugGradientCheck(BinderPack{sigma = 2.0}));
  };

  "indexed: gradient matches finite-diff"_test = [] {
    auto theta = plate(beta(2.0_c, 3.0_c), TestGroups{});
    auto posterior = makePlateTransformedPosterior(collectLogProbs(theta), theta)
                         .withDimension(TestGroups{}, 3)
                         .build();

    expectTrue(posterior.debugGradientCheck(
        BinderPack{theta = std::vector<double>{0.55, 0.38, 0.72}}));
  };

  "mixed scalar+indexed: gradient matches finite-diff"_test = [] {
    auto sigma = halfNormal(2.0_c);
    auto theta = plate(beta(2.0_c, 3.0_c), TestCountries2{});
    auto joint = collectLogProbs(sigma, theta);
    auto posterior = makePlateTransformedPosterior(joint, sigma, theta)
                         .withDimension(TestCountries2{}, 2)
                         .build();

    expectTrue(posterior.debugGradientCheck(
        BinderPack{sigma = 1.5, theta = std::vector<double>{0.4, 0.7}}));
  };

  // =========================================================================
  // Samples symbol-indexed access (type structure verification)
  // =========================================================================

  "Samples symbol access for mixed scalar+indexed params"_test = [] {
    auto sigma = halfNormal(2.0_c);
    auto theta = plate(beta(2.0_c, 3.0_c), TestCountries2{});
    auto joint = collectLogProbs(sigma, theta);
    auto posterior = makePlateTransformedPosterior(joint, sigma, theta)
                         .withDimension(TestCountries2{}, 3)
                         .build();

    using SamplesType = typename std::decay_t<decltype(posterior)>::SamplesType;
    using SymbolsTuple = typename SamplesType::SymbolsTuple;
    static_assert(std::tuple_size_v<SymbolsTuple> == 2,
                  "Should have 2 non-observed param symbols");

    using Sym0 = std::tuple_element_t<0, SymbolsTuple>;
    static_assert(std::is_same_v<Sym0, typename decltype(sigma)::symbol_type>,
                  "First symbol should match sigma's symbol_type");

    using Sym1 = std::tuple_element_t<1, SymbolsTuple>;
    static_assert(std::is_same_v<Sym1, typename decltype(theta)::symbol_type>,
                  "Second symbol should match theta's symbol_type");

    expectEq(posterior.stateDim(), 4UL);
  };

  // =========================================================================
  // HMC sampling integration test (migrated from hmc_adapter_test)
  // =========================================================================

  "HMC sampling produces valid samples"_test = [] {
    // Simple model: mu ~ Normal(2, 0.5), observed y = 2.0
    auto mu = normal(0.0_c, 5.0_c);
    auto sigma = halfNormal(2.0_c);
    auto y = normal(mu.sym(), sigma.sym());

    auto posterior = makePlateTransformedPosterior(
        logProb(mu, sigma, y), mu, sigma
    ).observe(y = 3.5);

    // Verify finite logProb/gradient via bindings
    double lp = posterior.logProbAt(BinderPack{mu = 0.0, sigma = 1.0});
    expectTrue(std::isfinite(lp));

    auto grad = posterior.gradientAt(BinderPack{mu = 0.0, sigma = 1.0});
    expectTrue(std::isfinite(grad[mu]));
    expectTrue(std::isfinite(grad[sigma]));

    // Run HMC sampling
    std::mt19937_64 rng(42);
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.1, .steps = 10, .warmup = 100, .draws = 200},
        BinderPack{mu = 0.0, sigma = 1.0},
        rng);

    // Verify we got the right number of draws
    expectEq(samples.size(), 200UL);

    // Verify sigma is constrained to positive (HalfNormal prior)
    auto sigma_draws = samples[sigma];
    for (std::size_t i = 0; i < sigma_draws.size(); ++i)
      expectTrue(sigma_draws[i] > 0.0);
  };

  "Mixed scalar+indexed HMC sampling"_test = [] {
    auto a = normal(-2.0_c, 1.0_c);
    auto sigma_rv = exponential(1.0_c);
    auto z_b = plate(normal(0.0_c, 1.0_c), TestCountries2{});
    auto joint = logProb(a, sigma_rv, z_b);

    auto posterior = makePlateTransformedPosterior(joint, a, sigma_rv, z_b)
                         .withDimension(TestCountries2{}, 3)
                         .build();

    std::mt19937_64 rng(123);
    auto samples = posterior.sample(
        HmcConfig{.epsilon = 0.05, .steps = 10, .warmup = 100, .draws = 20},
        BinderPack{a = -2.0, sigma_rv = 1.0,
                   z_b = std::vector<double>(3, 0.0)},
        rng);

    // Scalar access
    auto a_draws = samples[a];
    expectEq(a_draws.size(), 20UL);

    // Positive constraint on sigma
    auto sigma_draws = samples[sigma_rv];
    for (std::size_t i = 0; i < sigma_draws.size(); ++i)
      expectTrue(sigma_draws[i] > 0.0);

    // Indexed access
    auto z_b_draws = samples[z_b];
    expectEq(z_b_draws.rows(), 20UL);
    expectEq(z_b_draws.cols(), 3UL);
  };

  // =========================================================================
  // Backward compat: span-based API still works
  // =========================================================================

  "span-based logProb still works"_test = [] {
    auto alpha = gamma(2_c, 0.1_c);
    auto posterior = makePlateTransformedPosterior(logProb(alpha), alpha).build();

    std::vector<double> z = {std::log(3.0)};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    auto x = posterior.transform(z);
    expectNear(x[0], 3.0, 1e-10);
  };

  return TestRegistry::result();
}
