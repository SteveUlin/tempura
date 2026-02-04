// Tests for MCMC transform types
#include "symbolic4/mcmc/transforms.h"
#include "unit.h"

#include <cmath>

using namespace tempura;
using namespace tempura::symbolic4;

auto main() -> int {
  // ===========================================================================
  // Individual transform tests
  // ===========================================================================

  "Unconstrained transform is identity"_test = [] {
    auto t = unconstrained(Symbol<struct X>{});
    expectNear(5.0, t.transform(5.0), 1e-10);
    expectNear(-3.0, t.inverse(-3.0), 1e-10);
    expectNear(0.0, t.logJacobian(2.0), 1e-10);
  };

  "Positive transform: exp/log"_test = [] {
    auto t = positive(Symbol<struct X>{});

    // transform: z -> exp(z)
    expectNear(std::exp(2.0), t.transform(2.0), 1e-10);
    expectNear(std::exp(-1.0), t.transform(-1.0), 1e-10);

    // inverse: x -> log(x)
    expectNear(0.0, t.inverse(1.0), 1e-10);
    expectNear(std::log(5.0), t.inverse(5.0), 1e-10);

    // Jacobian: log(dx/dz) = log(exp(z)) = z
    expectNear(2.0, t.logJacobian(2.0), 1e-10);
    expectNear(-1.0, t.logJacobian(-1.0), 1e-10);
  };

  "UnitInterval transform: sigmoid/logit"_test = [] {
    auto t = unitInterval(Symbol<struct X>{});

    // transform: z -> sigmoid(z) = 1/(1+exp(-z))
    expectNear(0.5, t.transform(0.0), 1e-10);
    expectNear(1.0 / (1.0 + std::exp(-2.0)), t.transform(2.0), 1e-10);

    // inverse: x -> logit(x) = log(x/(1-x))
    expectNear(0.0, t.inverse(0.5), 1e-10);
    expectNear(std::log(0.7 / 0.3), t.inverse(0.7), 1e-10);

    // Round-trip
    for (double z : {-3.0, -1.0, 0.0, 1.0, 3.0}) {
      double x = t.transform(z);
      expectNear(z, t.inverse(x), 1e-10);
    }

    // Jacobian: log(x*(1-x))
    double x_at_0 = 0.5;
    double expected_jac = std::log(x_at_0 * (1.0 - x_at_0));
    expectNear(expected_jac, t.logJacobian(0.0), 1e-10);
  };

  "LowerBounded transform"_test = [] {
    auto t = lowerBounded(Symbol<struct X>{}, 2.0);

    // transform: z -> 2 + exp(z)
    expectNear(2.0 + std::exp(0.0), t.transform(0.0), 1e-10);
    expectNear(2.0 + std::exp(1.0), t.transform(1.0), 1e-10);

    // inverse: x -> log(x - 2)
    expectNear(0.0, t.inverse(3.0), 1e-10);
    expectNear(std::log(3.0), t.inverse(5.0), 1e-10);
  };

  "UpperBounded transform"_test = [] {
    auto t = upperBounded(Symbol<struct X>{}, 10.0);

    // transform: z -> 10 - exp(z)
    expectNear(10.0 - std::exp(0.0), t.transform(0.0), 1e-10);

    // inverse: x -> log(10 - x)
    expectNear(0.0, t.inverse(9.0), 1e-10);
  };

  "Interval transform"_test = [] {
    auto t = interval(Symbol<struct X>{}, 1.0, 5.0);

    // transform: z -> 1 + 4*sigmoid(z)
    // At z=0: sigmoid(0)=0.5, so x = 1 + 4*0.5 = 3
    expectNear(3.0, t.transform(0.0), 1e-10);

    // As z -> -inf: x -> 1
    // As z -> +inf: x -> 5
    expectTrue(t.transform(-10.0) > 1.0 && t.transform(-10.0) < 1.01);
    expectTrue(t.transform(10.0) > 4.99 && t.transform(10.0) < 5.0);

    // Round-trip
    for (double z : {-2.0, 0.0, 2.0}) {
      double x = t.transform(z);
      expectNear(z, t.inverse(x), 1e-10);
    }
  };

  // ===========================================================================
  // CholeskyTransform tests
  // ===========================================================================

  "CholeskyTransform stateSize"_test = [] {
    // K=2: 2(2+1)/2 = 3
    expectEq(std::size_t{3}, CholeskyTransform::stateSize(2));
    // K=3: 3(3+1)/2 = 6
    expectEq(std::size_t{6}, CholeskyTransform::stateSize(3));
    // K=4: 4(4+1)/2 = 10
    expectEq(std::size_t{10}, CholeskyTransform::stateSize(4));

    auto ct = choleskyTransform(3);
    expectEq(std::size_t{6}, ct.stateSize());
  };

  "CholeskyTransform packedIndex column-major"_test = [] {
    // For K=3, lower triangle:
    //   L = [L00,  0,   0 ]
    //       [L10, L11,  0 ]
    //       [L20, L21, L22]
    // Column-major packing: L00, L10, L20, L11, L21, L22
    // Indices:                0,   1,   2,   3,   4,   5
    auto ct = choleskyTransform(3);

    expectEq(std::size_t{0}, ct.packedIndex(0, 0));  // L00
    expectEq(std::size_t{1}, ct.packedIndex(1, 0));  // L10
    expectEq(std::size_t{2}, ct.packedIndex(2, 0));  // L20
    expectEq(std::size_t{3}, ct.packedIndex(1, 1));  // L11
    expectEq(std::size_t{4}, ct.packedIndex(2, 1));  // L21
    expectEq(std::size_t{5}, ct.packedIndex(2, 2));  // L22
  };

  "CholeskyTransform unpackIndex"_test = [] {
    auto ct = choleskyTransform(3);

    auto [r0, c0] = ct.unpackIndex(0);
    expectEq(std::size_t{0}, r0);
    expectEq(std::size_t{0}, c0);

    auto [r1, c1] = ct.unpackIndex(1);
    expectEq(std::size_t{1}, r1);
    expectEq(std::size_t{0}, c1);

    auto [r3, c3] = ct.unpackIndex(3);
    expectEq(std::size_t{1}, r3);
    expectEq(std::size_t{1}, c3);

    auto [r5, c5] = ct.unpackIndex(5);
    expectEq(std::size_t{2}, r5);
    expectEq(std::size_t{2}, c5);
  };

  "CholeskyTransform isDiagonalIndex"_test = [] {
    auto ct = choleskyTransform(3);

    // Diagonal indices: 0 (L00), 3 (L11), 5 (L22)
    expectTrue(ct.isDiagonalIndex(0));
    expectTrue(ct.isDiagonalIndex(3));
    expectTrue(ct.isDiagonalIndex(5));

    // Off-diagonal indices: 1, 2, 4
    expectTrue(!ct.isDiagonalIndex(1));
    expectTrue(!ct.isDiagonalIndex(2));
    expectTrue(!ct.isDiagonalIndex(4));
  };

  "CholeskyTransform transform basic"_test = [] {
    auto ct = choleskyTransform(2);

    // z = [z00, z10, z11]
    // L00 = exp(z00), L10 = z10, L11 = exp(z11)
    std::vector<double> z = {0.0, 0.5, 1.0};
    auto l = ct.transform(z);

    expectEq(std::size_t{2}, l.rows());
    expectEq(std::size_t{2}, l.cols());

    expectNear(std::exp(0.0), l[0, 0], 1e-10);  // L00 = exp(0) = 1
    expectNear(0.5, l[1, 0], 1e-10);            // L10 = 0.5
    expectNear(std::exp(1.0), l[1, 1], 1e-10);  // L11 = exp(1) = e
  };

  "CholeskyTransform inverse basic"_test = [] {
    auto ct = choleskyTransform(2);

    // Create a Cholesky factor
    DynamicMatrix l(2, 2);
    l[0, 0] = 2.0;   // Diagonal: positive
    l[1, 0] = -0.3;  // Off-diagonal: any real
    l[1, 1] = 1.5;   // Diagonal: positive

    auto z = ct.inverse(l);

    expectEq(std::size_t{3}, z.size());
    expectNear(std::log(2.0), z[0], 1e-10);   // z00 = log(L00)
    expectNear(-0.3, z[1], 1e-10);            // z10 = L10
    expectNear(std::log(1.5), z[2], 1e-10);   // z11 = log(L11)
  };

  "CholeskyTransform roundtrip"_test = [] {
    auto ct = choleskyTransform(3);

    // Start with z values
    std::vector<double> z_orig = {0.5, 0.1, -0.2, 0.8, 0.3, -0.1};
    auto l = ct.transform(z_orig);
    auto z_back = ct.inverse(l);

    expectEq(z_orig.size(), z_back.size());
    for (std::size_t i = 0; i < z_orig.size(); ++i) {
      expectNear(z_orig[i], z_back[i], 1e-10);
    }
  };

  "CholeskyTransform logJacobian"_test = [] {
    auto ct = choleskyTransform(3);

    // z = [z00, z10, z20, z11, z21, z22]
    // Diagonal indices: 0, 3, 5
    // logJacobian = z[0] + z[3] + z[5]
    std::vector<double> z = {0.5, 0.1, -0.2, 0.8, 0.3, -0.1};

    double expected_jac = 0.5 + 0.8 + (-0.1);  // z[0] + z[3] + z[5]
    expectNear(expected_jac, ct.logJacobian(z), 1e-10);
  };

  "CholeskyTransform produces valid Cholesky factor"_test = [] {
    // Verify that transformed matrix L gives a positive definite Sigma = L * L'
    auto ct = choleskyTransform(2);

    std::vector<double> z = {0.0, 0.5, 0.0};  // z00=0, z10=0.5, z11=0
    auto l = ct.transform(z);

    // L = [1.0,  0.0]
    //     [0.5,  1.0]
    // Sigma = L * L' = [1.0,  0.5]
    //                  [0.5,  1.25]

    // Compute Sigma = L * L'
    double s00 = l[0, 0] * l[0, 0];                        // L00^2
    double s10 = l[1, 0] * l[0, 0];                        // L10 * L00
    double s11 = l[1, 0] * l[1, 0] + l[1, 1] * l[1, 1];    // L10^2 + L11^2

    expectNear(1.0, s00, 1e-10);
    expectNear(0.5, s10, 1e-10);
    expectNear(1.25, s11, 1e-10);

    // Positive definiteness: det(Sigma) > 0, trace > 0
    double det = s00 * s11 - s10 * s10;  // 1.0 * 1.25 - 0.25 = 1.0
    expectTrue(det > 0);
    expectTrue(s00 + s11 > 0);
  };

  "CholeskyTransform K=4 packing"_test = [] {
    auto ct = choleskyTransform(4);

    // Verify state size
    expectEq(std::size_t{10}, ct.stateSize());

    // Verify some indices
    // Column 0: elements (0,0), (1,0), (2,0), (3,0) at indices 0,1,2,3
    // Column 1: elements (1,1), (2,1), (3,1) at indices 4,5,6
    // Column 2: elements (2,2), (3,2) at indices 7,8
    // Column 3: element (3,3) at index 9
    expectEq(std::size_t{0}, ct.packedIndex(0, 0));
    expectEq(std::size_t{3}, ct.packedIndex(3, 0));
    expectEq(std::size_t{4}, ct.packedIndex(1, 1));
    expectEq(std::size_t{7}, ct.packedIndex(2, 2));
    expectEq(std::size_t{9}, ct.packedIndex(3, 3));
  };

  // ===========================================================================
  // Type traits
  // ===========================================================================

  "is_transform_v detects transforms"_test = [] {
    static_assert(is_transform_v<Unconstrained<int>>);
    static_assert(is_transform_v<Positive<int>>);
    static_assert(is_transform_v<UnitInterval<int>>);
    static_assert(is_transform_v<LowerBounded<int>>);
    static_assert(is_transform_v<UpperBounded<int>>);
    static_assert(is_transform_v<Interval<int>>);
    static_assert(!is_transform_v<int>);
    static_assert(!is_transform_v<double>);
  };

  "is_cholesky_transform_v detects CholeskyTransform"_test = [] {
    static_assert(is_cholesky_transform_v<CholeskyTransform>);
    static_assert(!is_cholesky_transform_v<int>);
    static_assert(!is_cholesky_transform_v<double>);
    static_assert(!is_cholesky_transform_v<Positive<int>>);
  };

  return TestRegistry::result();
}
