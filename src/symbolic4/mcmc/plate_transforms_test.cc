#include "symbolic4/mcmc/plate_transforms.h"

#include <cmath>
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

    // IndexedParamSpec would be created by the factory
    // Just test the transform values
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
  // makePlateTransformedPosterior
  // =========================================================================

  "makePlateTransformedPosterior scalar only"_test = [] {
    auto alpha = gamma(2_c, 0.1_c);
    auto beta_param = gamma(2_c, 0.1_c);

    auto joint = logProb(alpha, beta_param);
    auto posterior = makePlateTransformedPosterior(joint, alpha, beta_param).build();

    expectEq(posterior.stateDim(), 2UL);

    // Test evaluation at z = [log(3), log(17)]
    std::vector<double> z = {std::log(3.0), std::log(17.0)};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Test transform
    auto x = posterior.transform(z);
    expectNear(x[0], 3.0, 1e-10);
    expectNear(x[1], 17.0, 1e-10);
  };

  "makePlateTransformedPosterior with plate"_test = [] {
    struct Countries {};

    auto alpha = gamma(2_c, 0.1_c);
    auto theta = plate(beta(alpha, 3.0_c), Countries{});

    auto joint = logProb(alpha, theta);

    // Create posterior with dimension specified
    auto posterior = makePlateTransformedPosterior(joint, alpha, theta)
                         .withDimension(Countries{}, 5)
                         .build();

    // State: [z_alpha, z_theta[0], ..., z_theta[4]]
    expectEq(posterior.stateDim(), 6UL);

    // Create state: alpha = 3.0, theta = [0.3, 0.4, 0.5, 0.6, 0.7]
    std::vector<double> z(6);
    z[0] = std::log(3.0);  // z_alpha
    for (int i = 0; i < 5; ++i) {
      double theta_i = 0.3 + 0.1 * i;
      z[1 + i] = std::log(theta_i / (1.0 - theta_i));  // logit(theta_i)
    }

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));

    // Gradient should also work
    auto grad = posterior.gradient(z);
    expectEq(grad.size(), 6UL);
    for (double g : grad) {
      expectTrue(std::isfinite(g));
    }
  };

  "makePlateTransformedPosterior hierarchical"_test = [] {
    struct Countries {};

    // Full hierarchical model:
    // alpha ~ Gamma(2, 0.1)
    // beta ~ Gamma(2, 0.1)
    // theta[i] ~ Beta(alpha, beta) for i in Countries

    auto alpha = gamma(2.0_c, 0.1_c);
    auto beta_param = gamma(2.0_c, 0.1_c);
    auto theta = plate(beta(alpha, beta_param), Countries{});

    auto joint = logProb(alpha, beta_param, theta);

    auto posterior = makePlateTransformedPosterior(joint, alpha, beta_param, theta)
                         .withDimension(Countries{}, 3)
                         .build();

    // State: [z_alpha, z_beta, z_theta[0], z_theta[1], z_theta[2]]
    expectEq(posterior.stateDim(), 5UL);

    std::vector<double> z = {
        std::log(3.0),   // alpha
        std::log(17.0),  // beta
        0.0,             // logit(0.5)
        -0.5,            // logit(~0.38)
        0.5              // logit(~0.62)
    };

    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));
  };

  // =========================================================================
  // Jacobian correctness: gradient matches finite-difference of logProb
  // =========================================================================

  "scalar constrained: gradient matches finite-diff logProb"_test = [] {
    // halfNormal has Positive support → exp transform → non-trivial Jacobian
    auto sigma = halfNormal(2.0_c);
    auto posterior = makePlateTransformedPosterior(logProb(sigma), sigma).build();

    std::vector<double> z = {0.7};
    auto grad = posterior.gradient(z);

    // Finite difference: d(logProb)/dz ≈ (logProb(z+ε) - logProb(z-ε)) / (2ε)
    constexpr double eps = 1e-5;
    std::vector<double> z_plus = {0.7 + eps};
    std::vector<double> z_minus = {0.7 - eps};
    double fd_grad = (posterior.logProb(z_plus) - posterior.logProb(z_minus)) / (2 * eps);

    // Tolerance: O(ε²) ≈ 1e-10, use 1e-4 for safety
    expectNear(fd_grad, grad[0], 1e-4);
  };

  "indexed constrained: gradient matches finite-diff logProb"_test = [] {
    // beta has Unit support → sigmoid transform → non-trivial Jacobian
    auto theta = plate(beta(2.0_c, 3.0_c), TestGroups{});
    auto posterior = makePlateTransformedPosterior(collectLogProbs(theta), theta)
                         .withDimension(TestGroups{}, 3)
                         .build();

    std::vector<double> z = {0.2, -0.5, 1.0};
    auto grad = posterior.gradient(z);

    constexpr double eps = 1e-5;
    for (std::size_t i = 0; i < 3; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (posterior.logProb(z_plus) - posterior.logProb(z_minus)) / (2 * eps);

      expectNear(fd_grad, grad[i], 1e-4);
    }
  };

  "mixed scalar+indexed: gradient matches finite-diff logProb"_test = [] {
    auto sigma = halfNormal(2.0_c);
    auto theta = plate(beta(2.0_c, 3.0_c), TestCountries2{});
    auto joint = collectLogProbs(sigma, theta);
    auto posterior = makePlateTransformedPosterior(joint, sigma, theta)
                         .withDimension(TestCountries2{}, 2)
                         .build();

    // State: [z_sigma, z_theta[0], z_theta[1]]
    std::vector<double> z = {0.5, -0.3, 0.8};
    auto grad = posterior.gradient(z);

    constexpr double eps = 1e-5;
    for (std::size_t i = 0; i < 3; ++i) {
      auto z_plus = z;
      auto z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;
      double fd_grad = (posterior.logProb(z_plus) - posterior.logProb(z_minus)) / (2 * eps);

      expectNear(fd_grad, grad[i], 1e-4);
    }
  };

  // =========================================================================
  // Samples symbol-indexed access
  // =========================================================================

  "Samples symbol access for mixed scalar+indexed params"_test = [] {
    // Create a simple model with mixed params:
    // sigma ~ HalfNormal(2)   (scalar)
    // theta[i] ~ Beta(2, 3)   (indexed)
    auto sigma = halfNormal(2.0_c);
    auto theta = plate(beta(2.0_c, 3.0_c), TestCountries2{});
    auto joint = collectLogProbs(sigma, theta);
    auto posterior = makePlateTransformedPosterior(joint, sigma, theta)
                         .withDimension(TestCountries2{}, 3)
                         .build();

    using SamplesType = typename std::decay_t<decltype(posterior)>::SamplesType;

    // Verify the symbols tuple type matches what we expect
    // NonObservedSymbolsTuple should contain:
    // - Symbol<sigma_id> (scalar)
    // - IndexedSymbol<theta_id, TestCountries2> (indexed)
    using SymbolsTuple = typename SamplesType::SymbolsTuple;
    static_assert(std::tuple_size_v<SymbolsTuple> == 2,
                  "Should have 2 non-observed param symbols");

    // First symbol should be scalar (sigma)
    using Sym0 = std::tuple_element_t<0, SymbolsTuple>;
    static_assert(std::is_same_v<Sym0, typename decltype(sigma)::symbol_type>,
                  "First symbol should match sigma's symbol_type");

    // Second symbol should be indexed (theta)
    using Sym1 = std::tuple_element_t<1, SymbolsTuple>;
    static_assert(std::is_same_v<Sym1, typename decltype(theta)::symbol_type>,
                  "Second symbol should match theta's symbol_type");

    // Verify the SamplesType can be instantiated and used
    // The key test: can we access samples by symbol for both scalar and indexed params?

    // For now, just verify the type structure is correct.
    // The actual sample() + operator[] test is deferred to an integration test.
    // Key insight: if the static_asserts above pass, the types match correctly.
    expectEq(posterior.stateDim(), 4UL);  // 1 scalar + 3 indexed
  };

  return TestRegistry::result();
}
