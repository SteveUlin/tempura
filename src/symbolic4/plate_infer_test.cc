// Tests for plate inference API - auto-elimination of manual likelihood computation
#include "symbolic4/distributions/distributions.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/indexed.h"
#include "symbolic4/infer.h"
#include "unit.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // data<Obs>() in plate expressions
  // ===========================================================================

  "data<Obs>() creates IndexedData"_test = [] {
    struct Obs {};
    auto x = data<Obs>();

    // Verify it's an IndexedData type
    static_assert(is_indexed_data_v<decltype(x)>);
  };

  "data<Obs>() converts to IndexedSymbol for arithmetic"_test = [] {
    struct Obs {};
    auto x = data<Obs>();

    // IndexedData::sym() returns the IndexedSymbol for use in expressions
    auto expr = x.sym() + lit(1.0);
    static_assert(Symbolic<decltype(expr)>);

    auto expr2 = x.sym() * lit(2.0);
    static_assert(Symbolic<decltype(expr2)>);
  };

  "data<Obs>() in plate expression"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);
    auto x = data<Obs>();
    auto sigma = halfNormal(2.0);

    // Create plate with data-dependent mean
    // Use *alpha, *beta, *sigma to get constrained expressions (using freeSym())
    // x.sym() is correct since x is IndexedData, not RandomVar
    auto y = plate<Obs>(normal(*alpha + *beta * x.sym(), *sigma));

    // Verify it's an indexed random variable
    static_assert(is_indexed_random_var_v<decltype(y)>);
  };

  // ===========================================================================
  // infer() with indexed parameters
  // ===========================================================================

  "infer() detects indexed parameters"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto y = plate<Obs>(normal(alpha, lit(1.0)));

    // infer() should return PlateTransformedPosteriorBuilder for indexed params
    auto builder = infer(alpha, y);

    // Should have withDimension method
    auto builder2 = builder.withDimension<Obs>(10);
    (void)builder2;
  };

  "infer() with scalar-only params returns TransformedPosteriorBuilder"_test = [] {
    auto mu = normal(0.0, 10.0);
    auto sigma = halfNormal(5.0);
    auto y = normal(mu, sigma);

    // Should work the same as before
    // Note: .observe() returns the final posterior, no need for .build()
    auto posterior = infer(mu, sigma, y).observe(y = 3.5);
    (void)posterior;
  };

  // ===========================================================================
  // Linear regression with plates
  // ===========================================================================

  "linear regression with plate API"_test = [] {
    struct Obs {};

    // Test: plate model using explicit .sym() for parameter references
    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);

    // Use operator* to get constrained expressions (exp(z) for positive params)
    // This uses the unconstrained symbol that bindings know about
    auto y = plate<Obs>(normal(*alpha, *sigma));

    std::vector<double> y_data = {2.1, 4.0, 5.9};

    // Create builder - this should use PlateTransformedPosteriorBuilder
    auto builder = infer(alpha, sigma, y);

    // Add dimension and observe
    auto posterior = builder
        .withDimension<Obs>(3)
        .observe(y = indexed(y_data));

    std::cout << "Plate test stateDim: " << posterior.stateDim() << "\n";
    std::cout << "y_data size: " << y_data.size() << "\n";

    // Try evaluating just the transformed values
    std::vector<double> z = {0.0, 0.0};  // alpha, sigma
    auto x_transformed = posterior.transform(z);
    std::cout << "Transformed: alpha=" << x_transformed[0] << ", sigma=" << x_transformed[1] << "\n";

    double lp = posterior.logProb(z);
    std::cout << "Plate logProb = " << lp << "\n";

    expectTrue(std::isfinite(lp));
  };

  // ===========================================================================
  // Unified .bind() method
  // ===========================================================================

  "unified bind() with observation and data"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto beta = normal(0.0, 5.0);
    auto sigma = halfNormal(2.0);
    auto x = data<Obs>();
    // Use *alpha, *beta, *sigma for constrained expressions; x.sym() for IndexedData
    auto y = plate<Obs>(normal(*alpha + *beta * x.sym(), *sigma));

    std::vector<double> x_data = {1.0, 2.0, 3.0};
    std::vector<double> y_data = {2.0, 4.0, 6.0};

    // Single bind() call for both x and y
    // .bind() returns PlateTransformedPosterior directly, no .build() needed
    auto posterior = infer(alpha, beta, sigma, y)
        .bind(x = indexed(x_data), y = indexed(y_data));

    // Test evaluation
    std::vector<double> z = {0.0, 2.0, 0.0};
    double lp = posterior.logProb(z);
    expectTrue(std::isfinite(lp));
  };

  // ===========================================================================
  // Gradient computation
  // ===========================================================================

  // First, verify that basic symbol evaluation works
  "simple symbol evaluation test"_test = [] {
    auto alpha = normal(0.0, 10.0);

    // Test that we can evaluate an expression with alpha's freeSym
    auto expr = alpha.freeSym() + lit(1.0);  // z + 1

    // Evaluate with explicit binding
    double result1 = evaluate(expr, alpha.freeSym() = 0.5);
    std::cout << "Direct eval: alpha.freeSym() + 1 at z=0.5: " << result1 << "\n";
    expectNear(1.5, result1, 1e-10);

    double result2 = evaluate(expr, alpha.freeSym() = 2.0);
    std::cout << "Direct eval: alpha.freeSym() + 1 at z=2.0: " << result2 << "\n";
    expectNear(3.0, result2, 1e-10);

    // Test evaluateIndexed with simple expression
    double result3 = evaluateIndexed(expr, BinderPack{alpha.freeSym() = 0.5});
    std::cout << "evaluateIndexed: alpha.freeSym() + 1 at z=0.5: " << result3 << "\n";
    expectNear(1.5, result3, 1e-10);
  };

  // Test collectLogProbs output directly
  "collectLogProbs evaluation test"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = plate<Obs>(normal(*alpha, *sigma));

    // Get the log-prob expression
    auto lp_expr = collectLogProbs(alpha, sigma, y);

    // Data for y
    std::vector<double> y_data = {1.0, 2.0, 3.0};
    auto y_binding = y = indexed(y_data);

    // Evaluate directly at different alpha values
    std::cout << "\nDirect collectLogProbs evaluation:\n";
    for (double a : {0.0, 0.5, 1.0, 2.0}) {
      double lp = evaluateIndexed(lp_expr,
          alpha.freeSym() = a,
          sigma.freeSym() = 0.2,
          y_binding);
      std::cout << "collectLogProbs at alpha=" << a << ", sigma=0.2: " << lp << "\n";
    }

    // Now test: are the symbols the same?
    std::cout << "\nSymbol type check:\n";
    using AlphaSym = decltype(alpha.freeSym());
    using SigmaSym = decltype(sigma.freeSym());
    std::cout << "alpha.freeSym() type: " << typeid(AlphaSym).name() << "\n";
    std::cout << "sigma.freeSym() type: " << typeid(SigmaSym).name() << "\n";

    // Test what infer() produces
    auto builder = infer(alpha, sigma, y);
    // The builder stores symbols internally - let's see if the types match
    // by doing a simpler test: create posterior and test with direct bindings

    auto posterior = builder.withDimension<Obs>(3).observe(y_binding);
    std::cout << "\nPosterior stateDim: " << posterior.stateDim() << "\n";
  };

  // More detailed debug: manually build bindings like PlateTransformedPosterior does
  "manual binding construction test"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    auto y = plate<Obs>(normal(*alpha, *sigma));

    auto lp_expr = collectLogProbs(alpha, sigma, y);
    std::vector<double> y_data = {1.0, 2.0, 3.0};

    // Create bindings manually, like PlateTransformedPosterior would
    std::vector<double> z = {0.5, 0.2};  // z_alpha, z_sigma

    // Binding method 1: direct bindings (known to work)
    auto alpha_binding = alpha.freeSym() = z[0];
    auto sigma_binding = sigma.freeSym() = z[1];
    auto y_bind = y = indexed(y_data);

    double lp1 = evaluateIndexed(lp_expr, alpha_binding, sigma_binding, y_bind);
    std::cout << "\nDirect bindings: logProb = " << lp1 << "\n";

    // Binding method 2: via BinderPack (like PlateTransformedPosterior)
    auto param_bindings = BinderPack{alpha_binding, sigma_binding};
    auto obs_bindings = BinderPack{y_bind};
    // Merge them
    auto merged = BinderPack{alpha_binding, sigma_binding, y_bind};

    double lp2 = evaluateIndexed(lp_expr, merged);
    std::cout << "BinderPack merged: logProb = " << lp2 << "\n";

    // Compare
    std::cout << "Difference: " << (lp2 - lp1) << "\n";
    expectNear(lp1, lp2, 1e-10);

    // Now test via PlateTransformedPosterior
    auto posterior = infer(alpha, sigma, y)
        .withDimension<Obs>(3)
        .observe(y = indexed(y_data));

    double lp3 = posterior.logProb(z);
    std::cout << "PlateTransformedPosterior: logProb = " << lp3 << "\n";
    std::cout << "Difference from direct: " << (lp3 - lp1) << "\n";

    // Test: what if we evaluate with alpha=0 and sigma=0 (default values)?
    // This would happen if the binding lookup fails
    double lp_at_zero = evaluateIndexed(lp_expr,
        alpha.freeSym() = 0.0,
        sigma.freeSym() = 0.0,
        y_bind);
    std::cout << "At z={0,0}: logProb = " << lp_at_zero << "\n";
    std::cout << "Matches PlateTransformedPosterior? " << (std::abs(lp_at_zero - lp3) < 0.01 ? "YES" : "NO") << "\n";

    // Test: check if symbol types match between test and infer()
    // The test creates its own alpha, sigma, y - and infer() also uses them
    // But infer() stores symbols internally - let's check type compatibility
    using TestAlphaSym = decltype(alpha.freeSym());
    using TestSigmaSym = decltype(sigma.freeSym());
    using TestYSym = decltype(y.freeSym());

    // Check that the binding types match
    std::cout << "\nType checking:\n";
    std::cout << "TestAlphaSym: " << typeid(TestAlphaSym).name() << "\n";
    std::cout << "TestSigmaSym: " << typeid(TestSigmaSym).name() << "\n";
    std::cout << "TestYSym: " << typeid(TestYSym).name() << "\n";

    // Test: evaluate lp_expr (built in test) with posterior's evaluation method
    // by manually creating the same binding structure
    // This tests if the issue is in mergeAllBindings or elsewhere
    auto bindings_tuple = std::make_tuple(alpha.freeSym() = z[0], sigma.freeSym() = z[1]);
    auto param_pack = std::apply([](const auto&... bs) { return BinderPack{bs...}; }, bindings_tuple);
    auto obs_pack = BinderPack{y_bind};

    // Simulate mergeAllBindings
    auto manually_merged = BinderPack{
        static_cast<const std::decay_t<decltype(std::get<0>(bindings_tuple))>&>(param_pack),
        static_cast<const std::decay_t<decltype(std::get<1>(bindings_tuple))>&>(param_pack),
        static_cast<const std::decay_t<decltype(y_bind)>&>(obs_pack)
    };

    double lp_manual_merge = evaluateIndexed(lp_expr, manually_merged);
    std::cout << "Manual merge simulation: logProb = " << lp_manual_merge << "\n";

    // Key test: Does infer() use the same lp_expr internally?
    // The expression TYPE should be the same if using the same alpha, sigma, y
    auto infer_lp_expr = collectLogProbs(alpha, sigma, y);
    using TestExprType = decltype(lp_expr);
    using InferExprType = decltype(infer_lp_expr);
    static_assert(std::is_same_v<TestExprType, InferExprType>,
                  "Expression types should match!");

    // If types match, evaluation should also match
    double lp_infer_expr = evaluateIndexed(infer_lp_expr,
        alpha.freeSym() = z[0],
        sigma.freeSym() = z[1],
        y_bind);
    std::cout << "Re-created collectLogProbs expr: logProb = " << lp_infer_expr << "\n";
    expectNear(lp1, lp_infer_expr, 1e-10);

    // Debug: Use posterior's debug methods to test components
    std::cout << "\n=== Debug Tests ===\n";

    // Test 1: Does posterior's expression work with test's bindings?
    auto posterior_expr = posterior.debugLogProbExpr();
    using PosteriorExprType = decltype(posterior_expr);
    std::cout << "Posterior expr type matches test expr? "
              << (std::is_same_v<PosteriorExprType, TestExprType> ? "YES" : "NO") << "\n";

    double lp_posterior_expr = evaluateIndexed(posterior_expr,
        alpha.freeSym() = z[0],
        sigma.freeSym() = z[1],
        y_bind);
    std::cout << "Posterior expr with test bindings: logProb = " << lp_posterior_expr << "\n";

    // Test 2: Use posterior's debugEvalWithBindings to bypass internal binding construction
    auto direct_bindings = BinderPack{alpha.freeSym() = z[0], sigma.freeSym() = z[1], y_bind};
    double lp_debug_eval = posterior.debugEvalWithBindings(direct_bindings);
    std::cout << "debugEvalWithBindings: logProb = " << lp_debug_eval << "\n";

    // Test 3: Check if symbols in posterior match test's symbols
    auto posterior_symbols = posterior.debugSymbols();
    using Sym0 = std::decay_t<std::tuple_element_t<0, decltype(posterior_symbols)>>;
    using Sym1 = std::decay_t<std::tuple_element_t<1, decltype(posterior_symbols)>>;
    using Sym2 = std::decay_t<std::tuple_element_t<2, decltype(posterior_symbols)>>;
    using TestAlpha = decltype(alpha.freeSym());
    using TestSigma = decltype(sigma.freeSym());
    using TestY = decltype(y.freeSym());

    std::cout << "Sym0 matches TestAlpha? " << (std::is_same_v<Sym0, TestAlpha> ? "YES" : "NO") << "\n";
    std::cout << "Sym1 matches TestSigma? " << (std::is_same_v<Sym1, TestSigma> ? "YES" : "NO") << "\n";
    std::cout << "Sym2 matches TestY? " << (std::is_same_v<Sym2, TestY> ? "YES" : "NO") << "\n";

    std::cout << "Sym0 type: " << typeid(Sym0).name() << "\n";
    std::cout << "TestAlpha type: " << typeid(TestAlpha).name() << "\n";

    // Test 4: Use debugLogProbWithMergedBindings - this calls the same code path as logProb
    // but we can see if it matches
    double lp_debug_merged = posterior.debugLogProbWithMergedBindings(z);
    std::cout << "debugLogProbWithMergedBindings: logProb = " << lp_debug_merged << "\n";

    // Test 5: Get the built bindings and merged bindings types
    auto built_bindings = posterior.debugBuildBindings(z);
    auto merged_bindings = posterior.debugMergedBindings(z);

    // Print the binding types
    std::cout << "\nBinding types:\n";
    std::cout << "Built bindings type: " << typeid(built_bindings).name() << "\n";
    std::cout << "Merged bindings type: " << typeid(merged_bindings).name() << "\n";
    std::cout << "Expected (direct_bindings) type: " << typeid(direct_bindings).name() << "\n";

    // Test 6: Evaluate with the built bindings + observation
    double lp_built_eval = evaluateIndexed(posterior_expr, merged_bindings);
    std::cout << "Eval with merged_bindings: logProb = " << lp_built_eval << "\n";
  };

  "gradients via finite difference match analytic"_test = [] {
    struct Obs {};

    auto alpha = normal(0.0, 10.0);
    auto sigma = halfNormal(2.0);
    // Use *alpha, *sigma to get constrained expressions
    auto y = plate<Obs>(normal(*alpha, *sigma));

    std::vector<double> y_data = {1.0, 2.0, 3.0};

    // .observe() returns PlateTransformedPosterior directly, no .build() needed
    auto posterior = infer(alpha, sigma, y)
        .withDimension<Obs>(3)
        .observe(y = indexed(y_data));

    // Test gradients at some point
    std::vector<double> z = {0.5, 0.2};  // z_alpha, z_sigma

    // Debug: print logProb at various z values - test smaller increments
    for (double a : {0.0, 0.5, 1.0}) {
      std::vector<double> z_test = {a, 0.2};
      std::cout << "logProb at {" << a << ", 0.2} = " << posterior.logProb(z_test) << "\n";
    }

    auto grad = posterior.gradient(z);

    // Verify by finite difference
    constexpr double eps = 1e-5;
    std::cout << "z = {" << z[0] << ", " << z[1] << "}\n";
    std::cout << "analytic grad = {" << grad[0] << ", " << grad[1] << "}\n";

    for (std::size_t i = 0; i < z.size(); ++i) {
      std::vector<double> z_plus = z;
      std::vector<double> z_minus = z;
      z_plus[i] += eps;
      z_minus[i] -= eps;

      double lp_plus = posterior.logProb(z_plus);
      double lp_minus = posterior.logProb(z_minus);
      double fd_grad = (lp_plus - lp_minus) / (2 * eps);
      std::cout << "i=" << i << ": lp+=" << lp_plus << ", lp-=" << lp_minus
                << ", fd_grad=" << fd_grad << ", analytic=" << grad[i] << "\n";

      // Allow 1% relative tolerance or 1e-4 absolute
      double tol = std::max(1e-4, std::abs(fd_grad) * 0.01);
      expectNear(fd_grad, grad[i], tol);
    }
  };

  // ===========================================================================
  // Non-centered parameterization
  // ===========================================================================

  "non-centered parameterization with infer"_test = [] {
    struct Groups {};

    // Hierarchical model with non-centered parameterization:
    //   mu ~ Normal(0, 5)
    //   sigma ~ Exponential(1)
    //   z[i] ~ Normal(0, 1)           ← standard normal in z-space
    //   theta[i] = mu + sigma * z[i]  ← deterministic transform
    //   y[i] ~ Normal(theta[i], 1)
    auto mu = normal(0.0, 5.0);
    auto sigma = exponential(1.0);
    auto z = plate<Groups>(normal(lit(0.0), lit(1.0)));

    // Non-centered: theta = mu + sigma * z (used inline in likelihood)
    auto theta_expr = mu.constrainedExpr() + sigma.constrainedExpr() * z.sym();
    auto y = plate<Groups>(normal(theta_expr, lit(1.0)));

    constexpr std::size_t kN = 5;
    std::vector<double> y_data = {1.2, 2.3, 0.8, 1.5, 2.0};

    auto posterior = infer(y)
        .bind(y = indexed(y_data));

    // State dim: mu (1 scalar) + sigma (1 scalar) + z (5 indexed) = 7
    expectEq(posterior.stateDim(), 2 + kN);

    // Evaluate log-prob at a test point
    // State layout: [z_mu, z_sigma, z[0], ..., z[4]]
    std::vector<double> state(2 + kN, 0.0);
    double lp = posterior.logProb(state);
    std::cout << "Non-centered logProb at zeros: " << lp << "\n";
    expectTrue(std::isfinite(lp));

    // Gradient matches finite differences
    auto grad = posterior.gradient(state);
    constexpr double eps = 1e-5;

    for (std::size_t i = 0; i < state.size(); ++i) {
      std::vector<double> z_plus = state;
      std::vector<double> z_minus = state;
      z_plus[i] += eps;
      z_minus[i] -= eps;

      double fd_grad = (posterior.logProb(z_plus) - posterior.logProb(z_minus)) / (2 * eps);
      double tol = std::max(1e-4, std::abs(fd_grad) * 0.01);
      expectNear(fd_grad, grad[i], tol);
    }

    std::cout << "Non-centered parameterization: all gradients match ✓\n";
  };

  "bmj-style non-centered model"_test = [] {
    struct Countries {};

    // BMJ model (non-centered):
    //   a ~ Normal(-2, 1)
    //   sigma ~ Exponential(1)
    //   z_b[L] ~ Normal(0, 1)
    //   p[L] = logistic(a + sigma * z_b[L])
    //   k[L] ~ Binomial(n[L], p[L])
    auto a = normal(-2.0, 1.0);
    auto sigma = exponential(1.0);
    auto z_b = plate<Countries>(normal(lit(0.0), lit(1.0)));

    auto n = data<Countries>();

    // p = sigmoid(a + sigma * z_b)
    auto p = 1_c / (1_c + exp(-(a.constrainedExpr() + sigma.constrainedExpr() * z_b.sym())));
    auto k = plate<Countries>(binomial(n, p));

    // Small test data: 4 countries
    constexpr std::size_t kNumCountries = 4;
    std::vector<double> n_obs = {10.0, 20.0, 15.0, 8.0};
    std::vector<double> k_obs = {2.0, 3.0, 1.0, 2.0};

    auto posterior = infer(k)
        .bind(n = indexed(n_obs), k = indexed(k_obs));

    // State dim: a (1) + sigma (1) + z_b (4) = 6
    expectEq(posterior.stateDim(), 2 + kNumCountries);

    // Evaluate log-prob
    std::vector<double> state(2 + kNumCountries, 0.0);
    state[0] = -2.0;  // a ≈ -2 in unconstrained space
    double lp = posterior.logProb(state);
    std::cout << "BMJ non-centered logProb: " << lp << "\n";
    expectTrue(std::isfinite(lp));

    // Gradient matches finite differences
    auto grad = posterior.gradient(state);
    constexpr double eps = 1e-5;

    for (std::size_t i = 0; i < state.size(); ++i) {
      std::vector<double> z_plus = state;
      std::vector<double> z_minus = state;
      z_plus[i] += eps;
      z_minus[i] -= eps;

      double fd_grad = (posterior.logProb(z_plus) - posterior.logProb(z_minus)) / (2 * eps);
      double tol = std::max(1e-4, std::abs(fd_grad) * 0.01);
      expectNear(fd_grad, grad[i], tol);
    }

    std::cout << "BMJ non-centered: all gradients match ✓\n";
  };

  return TestRegistry::result();
}
