#include "symbolic4/mcmc/matrix_mcmc.h"

#include <cmath>
#include <numbers>

#include "symbolic4/matrix/eval.h"
#include "symbolic4/matrix/mvn.h"
#include "symbolic4/matrix/types.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

struct TestDims;

auto main() -> int {
  // ===========================================================================
  // MatrixPosterior basic tests
  // ===========================================================================

  "MatrixPosterior stateDim"_test = [] {
    // K=2: mu has 2 params, Cholesky has 2*(2+1)/2 = 3 params
    // Total: 5
    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).build();
    expectEq(posterior.stateDim(), 5ul);

    // K=3: mu has 3 params, Cholesky has 3*(3+1)/2 = 6 params
    // Total: 9
    auto posterior3 = makeMatrixPosterior(dist, mu, L, y, 3).build();
    expectEq(posterior3.stateDim(), 9ul);
  };

  "MatrixPosterior transform and inverse roundtrip"_test = [] {
    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).build();

    // Start with unconstrained state
    std::vector<double> z = {0.5, -0.3, 0.2, 0.1, -0.1};

    // Transform to constrained
    auto x = posterior.transform(z);

    // Inverse back to unconstrained
    auto z_back = posterior.inverse(x);

    // Should match original
    for (std::size_t i = 0; i < z.size(); ++i) {
      expectNear(z[i], z_back[i], 1e-10);
    }
  };

  "MatrixPosterior Cholesky transform"_test = [] {
    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).build();

    // z = [mu0, mu1, z_L00, z_L10, z_L11]
    // L[0,0] = exp(z_L00), L[1,0] = z_L10, L[1,1] = exp(z_L11)
    std::vector<double> z = {1.0, 2.0, 0.0, 0.5, std::log(2.0)};

    auto x = posterior.transform(z);

    // x = [mu0, mu1, L00, L10, L11]
    expectNear(x[0], 1.0, 1e-10);           // mu0
    expectNear(x[1], 2.0, 1e-10);           // mu1
    expectNear(x[2], 1.0, 1e-10);           // L00 = exp(0) = 1
    expectNear(x[3], 0.5, 1e-10);           // L10 = 0.5 (unchanged)
    expectNear(x[4], 2.0, 1e-10);           // L11 = exp(log(2)) = 2
  };

  "MatrixPosterior extractMu and extractCholesky"_test = [] {
    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).build();

    std::vector<double> z = {1.0, 2.0, 0.0, 0.5, std::log(2.0)};

    auto mu_vec = posterior.extractMu(z);
    expectEq(mu_vec.size(), 2ul);
    expectNear(mu_vec[0], 1.0, 1e-10);
    expectNear(mu_vec[1], 2.0, 1e-10);

    auto L_mat = posterior.extractCholesky(z);
    expectEq(L_mat.rows(), 2ul);
    expectEq(L_mat.cols(), 2ul);
    expectNear(L_mat[0, 0], 1.0, 1e-10);   // exp(0)
    expectNear(L_mat[1, 0], 0.5, 1e-10);   // direct
    expectNear(L_mat[1, 1], 2.0, 1e-10);   // exp(log(2))
    expectNear(L_mat[0, 1], 0.0, 1e-10);   // upper triangle is 0
  };

  // ===========================================================================
  // LogProb tests
  // ===========================================================================

  "MatrixPosterior logProb at known point"_test = [] {
    // Test that logProb evaluates correctly for a simple case
    // MVN with mu=[0,0], L=I (identity), evaluating at y=[0,0]
    // log p(y|mu,L) = -log|L| - 0.5 ||L^{-1}(y-mu)||^2
    //               = -0 - 0.5 * 0 = 0

    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    // Create observation binding
    DynamicVector y_val(2);
    y_val[0] = 0.0;
    y_val[1] = 0.0;
    auto y_binding = DimVectorBinding<decltype(y)>{y_val};

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).observe(y_binding);

    // State: mu=[0,0], L=I
    // z = [mu0, mu1, z_L00, z_L10, z_L11] = [0, 0, 0, 0, 0]
    // This gives mu=[0,0], L[0,0]=exp(0)=1, L[1,0]=0, L[1,1]=exp(0)=1
    std::vector<double> z = {0.0, 0.0, 0.0, 0.0, 0.0};

    double lp = posterior.logProb(z);

    // Expected: logProb = -logDetChol(L) - 0.5*quadForm = -0 - 0 = 0
    // Plus Jacobian: sum of z_diag = 0 + 0 = 0
    // Total: 0
    expectNear(lp, 0.0, 1e-10);
  };

  "MatrixPosterior logProb non-zero residual"_test = [] {
    // MVN with mu=[0,0], L=I, y=[1,0]
    // ||L^{-1}(y-mu)||^2 = ||[1,0]||^2 = 1
    // log p = -0 - 0.5*1 = -0.5

    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    DynamicVector y_val(2);
    y_val[0] = 1.0;
    y_val[1] = 0.0;
    auto y_binding = DimVectorBinding<decltype(y)>{y_val};

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).observe(y_binding);

    std::vector<double> z = {0.0, 0.0, 0.0, 0.0, 0.0};  // mu=[0,0], L=I

    double lp = posterior.logProb(z);

    // Expected: -0.5 (from quadratic form) + 0 (Jacobian)
    expectNear(lp, -0.5, 1e-10);
  };

  "MatrixPosterior logProb with non-identity L"_test = [] {
    // MVN with mu=[0,0], L=[[2,0],[0,2]], y=[0,0]
    // log p = -logDetChol(L) - 0.5*0 = -log(2) - log(2) = -2*log(2)
    // Plus Jacobian from transform: z_L00 + z_L11 = log(2) + log(2) = 2*log(2)
    // Total: 0

    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    DynamicVector y_val(2);
    y_val[0] = 0.0;
    y_val[1] = 0.0;
    auto y_binding = DimVectorBinding<decltype(y)>{y_val};

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).observe(y_binding);

    double log2 = std::log(2.0);
    std::vector<double> z = {0.0, 0.0, log2, 0.0, log2};

    double lp = posterior.logProb(z);

    // logProb = -logDet(L) = -(log(2) + log(2)) = -2*log(2)
    // Jacobian = z_L00 + z_L11 = log(2) + log(2) = 2*log(2)
    // Total = 0
    expectNear(lp, 0.0, 1e-10);
  };

  // ===========================================================================
  // Gradient tests
  // ===========================================================================

  "MatrixPosterior gradient finite difference sanity"_test = [] {
    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    DynamicVector y_val(2);
    y_val[0] = 1.0;
    y_val[1] = 0.5;
    auto y_binding = DimVectorBinding<decltype(y)>{y_val};

    auto posterior = makeMatrixPosterior(dist, mu, L, y, 2).observe(y_binding);

    std::vector<double> z = {0.1, 0.2, 0.3, 0.4, 0.5};

    auto grad = posterior.gradient(z);

    // Verify gradient is non-zero and reasonable
    expectEq(grad.size(), 5ul);

    // Check that gradient points toward increasing log-prob
    // A small step in gradient direction should increase log-prob
    double lp0 = posterior.logProb(z);
    std::vector<double> z_step(5);
    double step = 0.001;
    for (std::size_t i = 0; i < 5; ++i) {
      z_step[i] = z[i] + step * grad[i];
    }
    double lp1 = posterior.logProb(z_step);

    // Should increase (or stay same if at maximum)
    expectTrue(lp1 >= lp0 - 1e-6);
  };

  // ===========================================================================
  // Transform tests
  // ===========================================================================

  "CholeskyTransform stateSize"_test = [] {
    expectEq(CholeskyTransform::stateSize(1), 1ul);   // 1*2/2 = 1
    expectEq(CholeskyTransform::stateSize(2), 3ul);   // 2*3/2 = 3
    expectEq(CholeskyTransform::stateSize(3), 6ul);   // 3*4/2 = 6
    expectEq(CholeskyTransform::stateSize(4), 10ul);  // 4*5/2 = 10
  };

  "CholeskyTransform packedIndex"_test = [] {
    // K=3: column-major lower triangle
    // Col 0: (0,0), (1,0), (2,0) -> indices 0, 1, 2
    // Col 1: (1,1), (2,1) -> indices 3, 4
    // Col 2: (2,2) -> index 5

    expectEq(CholeskyTransform::packedIndex(0, 0, 3), 0ul);
    expectEq(CholeskyTransform::packedIndex(1, 0, 3), 1ul);
    expectEq(CholeskyTransform::packedIndex(2, 0, 3), 2ul);
    expectEq(CholeskyTransform::packedIndex(1, 1, 3), 3ul);
    expectEq(CholeskyTransform::packedIndex(2, 1, 3), 4ul);
    expectEq(CholeskyTransform::packedIndex(2, 2, 3), 5ul);
  };

  "CholeskyTransform transform and inverse"_test = [] {
    CholeskyTransform xform(2);

    // z = [z_L00, z_L10, z_L11]
    std::vector<double> z = {0.0, 0.5, std::log(2.0)};

    auto L = xform.transform(z);

    expectNear(L[0, 0], 1.0, 1e-10);   // exp(0)
    expectNear(L[1, 0], 0.5, 1e-10);   // direct
    expectNear(L[1, 1], 2.0, 1e-10);   // exp(log(2))
    expectNear(L[0, 1], 0.0, 1e-10);   // upper triangle

    auto z_back = xform.inverse(L);
    for (std::size_t i = 0; i < z.size(); ++i) {
      expectNear(z[i], z_back[i], 1e-10);
    }
  };

  "CholeskyTransform logJacobian"_test = [] {
    CholeskyTransform xform(2);

    // z = [z_L00, z_L10, z_L11]
    // Jacobian = z_L00 + z_L11 (diagonal elements)
    std::vector<double> z = {0.5, 0.3, 0.2};

    double log_jac = xform.logJacobian(z);
    expectNear(log_jac, 0.5 + 0.2, 1e-10);  // z[0] + z[2]
  };

  "DimVectorTransform identity"_test = [] {
    DimVectorTransform xform{3};

    std::vector<double> z = {1.0, 2.0, 3.0};
    auto x = xform.transform(z);

    expectEq(x.size(), 3ul);
    expectNear(x[0], 1.0, 1e-10);
    expectNear(x[1], 2.0, 1e-10);
    expectNear(x[2], 3.0, 1e-10);

    // Jacobian is 0 for identity
    expectNear(xform.logJacobian(z), 0.0, 1e-10);
  };

  // ===========================================================================
  // Prior tests
  // ===========================================================================

  "muLogPrior computation"_test = [] {
    DynamicVector mu(2);
    mu[0] = 0.0;
    mu[1] = 0.0;

    // With mean=0, sd=1, log p(0) = -0.5*log(2*pi) - 0 = -0.9189...
    // For 2 components: 2 * (-0.9189...) = -1.8378...
    double lp = muLogPrior(mu, 0.0, 1.0);
    double expected = -std::log(2.0 * std::numbers::pi);  // 2 * -0.5*log(2*pi)
    expectNear(lp, expected, 1e-10);
  };

  "MatrixPosteriorWithPriors includes priors"_test = [] {
    auto mu = dimVector<TestDims>();
    auto L = cholCov<TestDims>();
    auto y = dimVector<TestDims>();
    auto dist = mvnCholesky(mu, L);

    DynamicVector y_val(2);
    y_val[0] = 0.0;
    y_val[1] = 0.0;
    auto y_binding = DimVectorBinding<decltype(y)>{y_val};

    MVNPriorConfig config;
    config.mu_prior_mean = 0.0;
    config.mu_prior_sd = 10.0;
    config.lkj_eta = 1.0;
    config.scale_prior_sd = 2.5;

    auto posterior_no_prior = makeMatrixPosterior(dist, mu, L, y, 2).observe(y_binding);
    auto posterior_with_prior = makeMatrixPosteriorWithPriors(dist, mu, L, y, 2, config)
                                   .observe(y_binding);

    std::vector<double> z = {0.0, 0.0, 0.0, 0.0, 0.0};

    double lp_no_prior = posterior_no_prior.logProb(z);
    double lp_with_prior = posterior_with_prior.logProb(z);

    // With priors should be different (priors add log-density)
    expectTrue(lp_with_prior != lp_no_prior);
  };

  return TestRegistry::result();
}
