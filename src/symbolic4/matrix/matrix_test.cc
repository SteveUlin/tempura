#include "symbolic4/matrix/matrix.h"

#include <cmath>

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Dimension tags for testing
struct Features;
struct Observations;

auto main() -> int {
  // ============================================================================
  // Type Traits Tests
  // ============================================================================

  "DimVectorSymbol traits"_test = [] {
    auto v = dimVector<Features>();
    static_assert(is_dim_vector_symbol_v<decltype(v)>);
    static_assert(!is_cholesky_symbol_v<decltype(v)>);
    static_assert(std::is_same_v<get_dim_tag_t<decltype(v)>, Features>);
  };

  "CholeskyCovSymbol traits"_test = [] {
    auto l = cholCov<Features>();
    static_assert(is_cholesky_symbol_v<decltype(l)>);
    static_assert(!is_cholesky_corr_v<decltype(l)>);
    static_assert(!is_dim_vector_symbol_v<decltype(l)>);
    static_assert(std::is_same_v<get_dim_tag_t<decltype(l)>, Features>);
  };

  "CholeskyCorrSymbol traits"_test = [] {
    auto l = cholCorr<Features>();
    static_assert(is_cholesky_symbol_v<decltype(l)>);
    static_assert(is_cholesky_corr_v<decltype(l)>);
    static_assert(!is_dim_vector_symbol_v<decltype(l)>);
  };

  // ============================================================================
  // Symbolic Expression Construction Tests
  // ============================================================================

  "dot creates Expression"_test = [] {
    auto v1 = dimVector<Features>();
    auto v2 = dimVector<Features>();
    auto expr = dot(v1, v2);
    static_assert(is_expression_v<decltype(expr)>);
    static_assert(std::is_same_v<get_op_t<decltype(expr)>, DotOp>);
  };

  "logDetChol creates Expression"_test = [] {
    auto l = cholCov<Features>();
    auto expr = logDetChol(l);
    static_assert(is_expression_v<decltype(expr)>);
    static_assert(std::is_same_v<get_op_t<decltype(expr)>, LogDetCholOp>);
  };

  "quadFormChol creates Expression"_test = [] {
    auto x = dimVector<Features>();
    auto l = cholCov<Features>();
    auto expr = quadFormChol(x, l);
    static_assert(is_expression_v<decltype(expr)>);
    static_assert(std::is_same_v<get_op_t<decltype(expr)>, QuadFormCholOp>);
  };

  // ============================================================================
  // DynamicVector and DynamicMatrix Tests
  // ============================================================================

  "DynamicVector basic operations"_test = [] {
    DynamicVector v{1.0, 2.0, 3.0};
    expectEq(v.size(), 3UL);
    expectNear(v[0], 1.0, 1e-10);
    expectNear(v[1], 2.0, 1e-10);
    expectNear(v[2], 3.0, 1e-10);
  };

  "DynamicMatrix basic operations"_test = [] {
    DynamicMatrix m(2, 3);
    m[0, 0] = 1.0;
    m[0, 1] = 2.0;
    m[1, 0] = 3.0;
    m[1, 2] = 4.0;

    expectEq(m.rows(), 2UL);
    expectEq(m.cols(), 3UL);
    expectNear(m[0, 0], 1.0, 1e-10);
    expectNear(m[0, 1], 2.0, 1e-10);
    expectNear(m[1, 0], 3.0, 1e-10);
    expectNear(m[1, 2], 4.0, 1e-10);
  };

  "DynamicMatrix diagonal access"_test = [] {
    DynamicMatrix m(3, 3);
    m[0, 0] = 1.0;
    m[1, 1] = 2.0;
    m[2, 2] = 3.0;

    expectNear(m.diagonal(0), 1.0, 1e-10);
    expectNear(m.diagonal(1), 2.0, 1e-10);
    expectNear(m.diagonal(2), 3.0, 1e-10);
  };

  // ============================================================================
  // Matrix Evaluation Tests
  // ============================================================================

  "evaluate dot product"_test = [] {
    auto v1 = dimVector<Features>();
    auto v2 = dimVector<Features>();
    auto expr = dot(v1, v2);

    DynamicVector vec1{1.0, 2.0, 3.0};
    DynamicVector vec2{4.0, 5.0, 6.0};

    auto result = evaluateMatrix(
        expr,
        DimVectorBinding<decltype(v1)>{vec1},
        DimVectorBinding<decltype(v2)>{vec2});

    // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
    expectNear(result, 32.0, 1e-10);
  };

  "evaluate logDetChol"_test = [] {
    auto l = cholCov<Features>();
    auto expr = logDetChol(l);

    // Lower triangular 3x3 matrix
    DynamicMatrix chol(3, 3);
    chol[0, 0] = 2.0;
    chol[1, 0] = 0.5;
    chol[1, 1] = 3.0;
    chol[2, 0] = 0.3;
    chol[2, 1] = 0.4;
    chol[2, 2] = 4.0;

    auto result = evaluateMatrix(expr, CholeskyBinding<decltype(l)>{chol});

    // log(2) + log(3) + log(4)
    double expected = std::log(2.0) + std::log(3.0) + std::log(4.0);
    expectNear(result, expected, 1e-10);
  };

  "evaluate solveTriangular"_test = [] {
    auto l = cholCov<Features>();
    auto b = dimVector<Features>();
    auto expr = solveTriangular(l, b);

    // L = [[2, 0], [1, 3]]
    DynamicMatrix chol(2, 2);
    chol[0, 0] = 2.0;
    chol[1, 0] = 1.0;
    chol[1, 1] = 3.0;

    // b = [4, 5]
    DynamicVector vec{4.0, 5.0};

    auto result = evaluateMatrix(
        expr,
        CholeskyBinding<decltype(l)>{chol},
        DimVectorBinding<decltype(b)>{vec});

    // Solve Lx = b:
    // 2*x0 = 4  => x0 = 2
    // 1*x0 + 3*x1 = 5  => 2 + 3*x1 = 5  => x1 = 1
    expectNear(result[0], 2.0, 1e-10);
    expectNear(result[1], 1.0, 1e-10);
  };

  "evaluate quadFormChol"_test = [] {
    auto x = dimVector<Features>();
    auto l = cholCov<Features>();
    auto expr = quadFormChol(x, l);

    // L = [[2, 0], [1, 3]]
    DynamicMatrix chol(2, 2);
    chol[0, 0] = 2.0;
    chol[1, 0] = 1.0;
    chol[1, 1] = 3.0;

    // x = [4, 5]
    DynamicVector vec{4.0, 5.0};

    auto result = evaluateMatrix(
        expr,
        DimVectorBinding<decltype(x)>{vec},
        CholeskyBinding<decltype(l)>{chol});

    // L^-1 x = [2, 1] (from previous test)
    // ||L^-1 x||^2 = 2^2 + 1^2 = 5
    expectNear(result, 5.0, 1e-10);
  };

  // ============================================================================
  // Integration Tests
  // ============================================================================

  "MVN log-prob expression structure"_test = [] {
    // Build symbolic expression for MVN log-prob kernel parts
    auto y = dimVector<Features>();
    auto l = cholCov<Features>();

    // Individual parts are expressions
    auto quad = quadFormChol(y, l);
    auto log_det = logDetChol(l);

    static_assert(is_expression_v<decltype(quad)>);
    static_assert(is_expression_v<decltype(log_det)>);
  };

  // ============================================================================
  // MVN Distribution Tests
  // ============================================================================

  "MVNormalCholeskyDist creates expressions"_test = [] {
    auto mu = dimVector<Features>();
    auto l_cov = cholCov<Features>();
    auto y = dimVector<Features>();

    auto dist = mvnCholesky(mu, l_cov);
    auto lp = dist.unnormalizedLogProbFor(y);

    static_assert(is_expression_v<decltype(lp)>);
  };

  // ============================================================================
  // LKJ Distribution Tests
  // ============================================================================

  "LKJCholeskyDist creates expressions"_test = [] {
    auto l_corr = cholCorr<Features>();
    auto dist = lkjCholesky(lit(2.0));
    auto lp = dist.logProbFor(l_corr);

    static_assert(is_expression_v<decltype(lp)>);
  };

  "LKJ convenience functions"_test = [] {
    auto uniform = uniformCorrelation();
    auto weakly = weaklyInformativeCorrelation();

    // Both should produce valid distributions
    auto l_corr = cholCorr<Features>();
    auto lp1 = uniform.logProbFor(l_corr);
    auto lp2 = weakly.logProbFor(l_corr);

    static_assert(is_expression_v<decltype(lp1)>);
    static_assert(is_expression_v<decltype(lp2)>);
  };

  "LKJ log-prob evaluation"_test = [] {
    auto l_corr = cholCorr<Features>();
    auto dist = lkjCholesky(lit(2.0));
    auto lp_expr = dist.logProbFor(l_corr);

    // Create a 3x3 correlation Cholesky factor
    // For correlation matrix, L[0,0] = 1
    // L[i,i] = sqrt(1 - sum of squared off-diagonal in row i)
    DynamicMatrix chol(3, 3);
    chol[0, 0] = 1.0;  // First diagonal always 1
    chol[1, 0] = 0.3;
    chol[1, 1] = std::sqrt(1.0 - 0.3 * 0.3);  // ≈ 0.954
    chol[2, 0] = 0.2;
    chol[2, 1] = 0.4;
    double diag_22_sq = 1.0 - 0.2 * 0.2 - 0.4 * 0.4;
    chol[2, 2] = std::sqrt(diag_22_sq);  // ≈ 0.894

    auto result = evaluateMatrix(lp_expr, CholeskyBinding<decltype(l_corr)>{chol});

    // LKJ log-prob: Σᵢ₌₁^{K-1} (K - i + 2η - 3) × log(L[i,i])
    // For K=3, η=2:
    //   i=1: (3-1+4-3) × log(L[1,1]) = 3 × log(0.954)
    //   i=2: (3-2+4-3) × log(L[2,2]) = 2 × log(0.894)
    double expected = 3.0 * std::log(chol[1, 1]) + 2.0 * std::log(chol[2, 2]);
    expectNear(result, expected, 1e-10);
  };

  return TestRegistry::result();
}
