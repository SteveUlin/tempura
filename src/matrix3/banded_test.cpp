#include "matrix3/banded.h"

#include "matrix3/matrix.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::matrix3;

auto main() -> int {
  "banded_basic_construction"_test = [] {
    // 3x3 matrix with 3 bands (1 below, 1 center, 1 above)
    InlineDense<double, 3, 3> storage{
        {0.0, 1.0, 2.0},  // Row 0: X, diag, super-diag
        {3.0, 4.0, 5.0},  // Row 1: sub-diag, diag, super-diag
        {6.0, 7.0, 0.0},  // Row 2: sub-diag, diag, X
    };

    Banded<InlineDense<double, 3, 3>, 1> banded{storage};

    // Verify extents
    static_assert(decltype(banded)::kRows == 3);
    static_assert(decltype(banded)::kCols == 3);
    static_assert(decltype(banded)::kBands == 3);
    static_assert(decltype(banded)::kCenterBand == 1);
  };

  "banded_in_band_access"_test = [] {
    InlineDense<double, 3, 3> storage{
        {0.0, 1.0, 2.0},
        {3.0, 4.0, 5.0},
        {6.0, 7.0, 0.0},
    };

    Banded<InlineDense<double, 3, 3>, 1> banded{storage};

    // Diagonal elements (band = 1)
    expectEq(1.0, banded[0, 0]);
    expectEq(4.0, banded[1, 1]);
    expectEq(7.0, banded[2, 2]);

    // Super-diagonal (band = 2)
    expectEq(2.0, banded[0, 1]);
    expectEq(5.0, banded[1, 2]);

    // Sub-diagonal (band = 0)
    expectEq(3.0, banded[1, 0]);
    expectEq(6.0, banded[2, 1]);
  };

  "banded_out_of_band_read_zero"_test = [] {
    InlineDense<double, 3, 3> storage{
        {0.0, 1.0, 2.0},
        {3.0, 4.0, 5.0},
        {6.0, 7.0, 0.0},
    };

    Banded<InlineDense<double, 3, 3>, 1> banded{storage};

    // Out of band elements should read as zero
    expectEq(0.0, banded[0, 2]);  // Too far above diagonal
    expectEq(0.0, banded[2, 0]);  // Too far below diagonal
  };

  // NOTE: Writing to out-of-band elements is undefined behavior
  // This test verifies out-of-band reads are zero, but does not test writes

  "banded_in_band_write"_test = [] {
    InlineDense<double, 3, 3> storage{
        {0.0, 1.0, 2.0},
        {3.0, 4.0, 5.0},
        {6.0, 7.0, 0.0},
    };

    Banded<InlineDense<double, 3, 3>, 1> banded{storage};

    // Write to in-band elements
    banded[0, 0] = 10.0;  // band = 0 - 0 + 1 = 1 → storage[0, 1]
    banded[1, 2] = 50.0;  // band = 2 - 1 + 1 = 2 → storage[1, 2]
    banded[2, 1] = 70.0;  // band = 1 - 2 + 1 = 0 → storage[2, 0]

    // Verify writes took effect
    expectEq(10.0, banded[0, 0]);
    expectEq(50.0, banded[1, 2]);
    expectEq(70.0, banded[2, 1]);

    // Verify changes in underlying storage via data accessor
    const auto& data = banded.data();
    expectEq(10.0, data[0, 1]);  // band 1
    expectEq(50.0, data[1, 2]);  // band 2
    expectEq(70.0, data[2, 0]);  // band 0
  };

  "banded_band_calculation"_test = [] {
    // Test various band calculations
    // band = col - row + CenterBand

    // For CenterBand = 1:
    // (0,0): band = 0 - 0 + 1 = 1 ✓
    // (0,1): band = 1 - 0 + 1 = 2 ✓
    // (1,0): band = 0 - 1 + 1 = 0 ✓
    // (0,2): band = 2 - 0 + 1 = 3 ✗ (out of range for 3 bands)

    InlineDense<double, 3, 3> storage{
        {11.0, 22.0, 33.0},
        {44.0, 55.0, 66.0},
        {77.0, 88.0, 99.0},
    };

    Banded<InlineDense<double, 3, 3>, 1> banded{storage};

    // Verify band mapping
    expectEq(22.0, banded[0, 0]);  // storage[0,1]
    expectEq(33.0, banded[0, 1]);  // storage[0,2]
    expectEq(44.0, banded[1, 0]);  // storage[1,0]
    expectEq(55.0, banded[1, 1]);  // storage[1,1]
    expectEq(66.0, banded[1, 2]);  // storage[1,2]
    expectEq(77.0, banded[2, 1]);  // storage[2,0]
    expectEq(88.0, banded[2, 2]);  // storage[2,1]
  };

  "banded_different_center_bands"_test = [] {
    // Test with CenterBand = 0 (only upper diagonal bands)
    InlineDense<double, 4, 2> storage{
        {1.0, 2.0},
        {3.0, 4.0},
        {5.0, 6.0},
        {7.0, 8.0},
    };

    Banded<InlineDense<double, 4, 2>, 0> banded{storage};

    static_assert(decltype(banded)::kCenterBand == 0);

    // band = col - row + 0
    // (0,0): band = 0 ✓ → storage[0,0]
    // (0,1): band = 1 ✓ → storage[0,1]
    // (1,0): band = -1 ✗
    // (1,1): band = 0 ✓ → storage[1,0]
    expectEq(1.0, banded[0, 0]);
    expectEq(2.0, banded[0, 1]);
    expectEq(0.0, banded[1, 0]);  // Out of band
    expectEq(3.0, banded[1, 1]);
  };

  "banded_constexpr"_test = [] {
    constexpr auto test = []() {
      InlineDense<int, 2, 2> storage{
          {10, 20},
          {30, 40},
      };
      Banded<InlineDense<int, 2, 2>, 0> banded{storage};
      return banded[0, 0] + banded[0, 1] + banded[1, 1];
    };

    static_assert(test() == 10 + 20 + 30);  // 60
  };

  "banded_deduction_guide"_test = [] {
    InlineDense<double, 3, 3> storage{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0},
    };

    Banded banded{storage};

    // Should deduce CenterBand = kCols / 2 = 3 / 2 = 1
    static_assert(decltype(banded)::kCenterBand == 1);
  };

  "banded_factory_function"_test = [] {
    InlineDense<double, 4, 3> storage{
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0},
        {10.0, 11.0, 12.0},
    };

    auto banded = makeBanded<1>(storage);

    static_assert(decltype(banded)::kCenterBand == 1);
    expectEq(2.0, banded[0, 0]);
  };

  "banded_data_accessor"_test = [] {
    InlineDense<double, 2, 2> storage{
        {1.0, 2.0},
        {3.0, 4.0},
    };

    Banded<InlineDense<double, 2, 2>, 0> banded{storage};

    const auto& data = banded.data();
    expectEq(1.0, data[0, 0]);
    expectEq(2.0, data[0, 1]);
  };

  "banded_extent_accessors"_test = [] {
    using BandedType = Banded<InlineDense<int, 5, 3>, 1>;
    static_assert(BandedType::rows() == 5);
    static_assert(BandedType::cols() == 5);

    BandedType banded;
    static_assert(banded.rows() == 5);
    static_assert(banded.cols() == 5);
  };

  "banded_tridiagonal_matrix"_test = [] {
    // Common use case: tridiagonal matrix (3 bands)
    InlineDense<double, 4, 3> storage{
        {0.0, 2.0, 3.0},   // Row 0: X, diag, upper
        {1.0, 2.0, 3.0},   // Row 1: lower, diag, upper
        {1.0, 2.0, 3.0},   // Row 2: lower, diag, upper
        {1.0, 2.0, 0.0},   // Row 3: lower, diag, X
    };

    Banded<InlineDense<double, 4, 3>, 1> tri{storage};

    // Verify tridiagonal structure
    expectEq(2.0, tri[0, 0]);  // Diagonal
    expectEq(3.0, tri[0, 1]);  // Upper
    expectEq(0.0, tri[0, 2]);  // Zero (out of band)

    expectEq(1.0, tri[1, 0]);  // Lower
    expectEq(2.0, tri[1, 1]);  // Diagonal
    expectEq(3.0, tri[1, 2]);  // Upper

    expectEq(0.0, tri[3, 0]);  // Zero (out of band)
    expectEq(1.0, tri[3, 2]);  // Lower
    expectEq(2.0, tri[3, 3]);  // Diagonal
  };

  "banded_value_types"_test = [] {
    // Test with int
    InlineDense<int, 2, 2> int_storage{
        {1, 2},
        {3, 4},
    };
    Banded<InlineDense<int, 2, 2>, 0> int_banded{int_storage};
    expectEq(1, int_banded[0, 0]);

    // Test with float
    InlineDense<float, 2, 2> float_storage{
        {1.5f, 2.5f},
        {3.5f, 4.5f},
    };
    Banded<InlineDense<float, 2, 2>, 0> float_banded{float_storage};
    expectEq(1.5f, float_banded[0, 0]);
  };

  "solveTridiagonal_simple_2x2"_test = [] {
    // System: [2 1] [x0]   [5]
    //         [1 2] [x1] = [4]
    // Solution: x0 = 2, x1 = 1
    InlineDense<double, 2, 3> storage{
        {0.0, 2.0, 1.0},  // X, b0=2, c0=1
        {1.0, 2.0, 0.0},  // a1=1, b1=2, X
    };
    Banded<InlineDense<double, 2, 3>, 1> A{storage};

    InlineDense<double, 2, 1> b{{5.0}, {4.0}};

    auto x = solveTridiagonal(A, b);

    expectNear(2.0, x[0, 0], 1e-10);
    expectNear(1.0, x[1, 0], 1e-10);
  };

  "solveTridiagonal_3x3"_test = [] {
    // System: [4 1 0] [x0]   [5]
    //         [1 4 1] [x1] = [6]
    //         [0 1 4] [x2]   [5]
    // Diagonally dominant, solution: x0 = 1, x1 = 1, x2 = 1
    InlineDense<double, 3, 3> storage{
        {0.0, 4.0, 1.0},  // X, b0=4, c0=1
        {1.0, 4.0, 1.0},  // a1=1, b1=4, c1=1
        {1.0, 4.0, 0.0},  // a2=1, b2=4, X
    };
    Banded<InlineDense<double, 3, 3>, 1> A{storage};

    InlineDense<double, 3, 1> b{{5.0}, {6.0}, {5.0}};

    auto x = solveTridiagonal(A, b);

    expectNear(1.0, x[0, 0], 1e-10);
    expectNear(1.0, x[1, 0], 1e-10);
    expectNear(1.0, x[2, 0], 1e-10);
  };

  "solveTridiagonal_4x4_varying_solution"_test = [] {
    // System with varying solution values
    InlineDense<double, 4, 3> storage{
        {0.0, 3.0, 1.0},  // X, b0=3, c0=1
        {1.0, 3.0, 1.0},  // a1=1, b1=3, c1=1
        {1.0, 3.0, 1.0},  // a2=1, b2=3, c2=1
        {1.0, 3.0, 0.0},  // a3=1, b3=3, X
    };
    Banded<InlineDense<double, 4, 3>, 1> A{storage};

    // RHS chosen so solution is [1, 2, 3, 4]
    // Ax: [3*1 + 1*2, 1*1 + 3*2 + 1*3, 1*2 + 3*3 + 1*4, 1*3 + 3*4]
    //   = [5, 10, 15, 15]
    InlineDense<double, 4, 1> b{{5.0}, {10.0}, {15.0}, {15.0}};

    auto x = solveTridiagonal(A, b);

    expectNear(1.0, x[0, 0], 1e-10);
    expectNear(2.0, x[1, 0], 1e-10);
    expectNear(3.0, x[2, 0], 1e-10);
    expectNear(4.0, x[3, 0], 1e-10);
  };

  "solveTridiagonal_single_equation"_test = [] {
    // Trivial 1x1 system: [5] [x0] = [10]
    // Solution: x0 = 2
    InlineDense<double, 1, 3> storage{
        {0.0, 5.0, 0.0},  // X, b0=5, X
    };
    Banded<InlineDense<double, 1, 3>, 1> A{storage};

    InlineDense<double, 1, 1> b{{10.0}};

    auto x = solveTridiagonal(A, b);

    expectNear(2.0, x[0, 0], 1e-10);
  };

  "solveTridiagonal_negative_values"_test = [] {
    // System with negative coefficients
    InlineDense<double, 3, 3> storage{
        {0.0, 5.0, -1.0},   // X, b0=5, c0=-1
        {-2.0, 6.0, -1.0},  // a1=-2, b1=6, c1=-1
        {-2.0, 7.0, 0.0},   // a2=-2, b2=7, X
    };
    Banded<InlineDense<double, 3, 3>, 1> A{storage};

    InlineDense<double, 3, 1> b{{4.0}, {5.0}, {6.0}};

    auto x = solveTridiagonal(A, b);

    // Verify Ax = b
    double r0 = A[0, 0] * x[0, 0] + A[0, 1] * x[1, 0];
    double r1 = A[1, 0] * x[0, 0] + A[1, 1] * x[1, 0] + A[1, 2] * x[2, 0];
    double r2 = A[2, 1] * x[1, 0] + A[2, 2] * x[2, 0];

    expectNear(4.0, r0, 1e-10);
    expectNear(5.0, r1, 1e-10);
    expectNear(6.0, r2, 1e-10);
  };

  "solveTridiagonal_constexpr"_test = [] {
    // Compile-time evaluation
    constexpr auto test = []() {
      InlineDense<double, 2, 3> storage{
          {0.0, 2.0, 1.0},
          {1.0, 2.0, 0.0},
      };
      Banded<InlineDense<double, 2, 3>, 1> A{storage};

      InlineDense<double, 2, 1> b{{5.0}, {4.0}};

      auto x = solveTridiagonal(A, b);
      return x[0, 0] + x[1, 0];  // Should be 2 + 1 = 3
    };

    static_assert(test() == 3.0);
  };

  "multiplyBanded_tridiagonal_3x3"_test = [] {
    // Matrix: [4 1 0]     Vector: [1]
    //         [1 4 1]  *          [2]
    //         [0 1 4]             [3]
    // Result: [6, 12, 14]
    InlineDense<double, 3, 3> storage{
        {0.0, 4.0, 1.0},
        {1.0, 4.0, 1.0},
        {1.0, 4.0, 0.0},
    };
    Banded<InlineDense<double, 3, 3>, 1> A{storage};

    InlineDense<double, 3, 1> x{{1.0}, {2.0}, {3.0}};

    auto y = multiplyBanded(A, x);

    expectNear(6.0, y[0, 0], 1e-10);   // 4*1 + 1*2 = 6
    expectNear(12.0, y[1, 0], 1e-10);  // 1*1 + 4*2 + 1*3 = 1 + 8 + 3 = 12
    expectNear(14.0, y[2, 0], 1e-10);  // 1*2 + 4*3 = 2 + 12 = 14
  };

  "multiplyBanded_diagonal_only"_test = [] {
    // Diagonal matrix (1 band, center=0)
    InlineDense<double, 4, 1> storage{
        {2.0},
        {3.0},
        {4.0},
        {5.0},
    };
    Banded<InlineDense<double, 4, 1>, 0> A{storage};

    InlineDense<double, 4, 1> x{{1.0}, {2.0}, {3.0}, {4.0}};

    auto y = multiplyBanded(A, x);

    expectNear(2.0, y[0, 0], 1e-10);   // 2*1 = 2
    expectNear(6.0, y[1, 0], 1e-10);   // 3*2 = 6
    expectNear(12.0, y[2, 0], 1e-10);  // 4*3 = 12
    expectNear(20.0, y[3, 0], 1e-10);  // 5*4 = 20
  };

  "multiplyBanded_upper_triangular"_test = [] {
    // Upper bidiagonal (2 bands: diagonal + upper)
    InlineDense<double, 3, 2> storage{
        {1.0, 2.0},  // diag=1, upper=2
        {3.0, 4.0},  // diag=3, upper=4
        {5.0, 6.0},  // diag=5, (upper=6 out of bounds)
    };
    Banded<InlineDense<double, 3, 2>, 0> A{storage};

    InlineDense<double, 3, 1> x{{1.0}, {1.0}, {1.0}};

    auto y = multiplyBanded(A, x);

    expectNear(3.0, y[0, 0], 1e-10);  // 1*1 + 2*1 = 3
    expectNear(7.0, y[1, 0], 1e-10);  // 3*1 + 4*1 = 7
    expectNear(5.0, y[2, 0], 1e-10);  // 5*1 = 5
  };

  "multiplyBanded_verification_with_solve"_test = [] {
    // Verify multiplyBanded is consistent with solveTridiagonal
    // Solve Ax=b, then check A(Ax) gives the same as A*b
    InlineDense<double, 3, 3> storage{
        {0.0, 4.0, 1.0},
        {1.0, 4.0, 1.0},
        {1.0, 4.0, 0.0},
    };
    Banded<InlineDense<double, 3, 3>, 1> A{storage};

    InlineDense<double, 3, 1> b{{5.0}, {6.0}, {5.0}};

    auto x = solveTridiagonal(A, b);
    auto Ax = multiplyBanded(A, x);

    // Ax should equal b (within numerical tolerance)
    expectNear(5.0, Ax[0, 0], 1e-9);
    expectNear(6.0, Ax[1, 0], 1e-9);
    expectNear(5.0, Ax[2, 0], 1e-9);
  };

  "multiplyBanded_constexpr"_test = [] {
    constexpr auto test = []() {
      InlineDense<double, 2, 3> storage{
          {0.0, 2.0, 1.0},
          {1.0, 2.0, 0.0},
      };
      Banded<InlineDense<double, 2, 3>, 1> A{storage};

      InlineDense<double, 2, 1> x{{3.0}, {4.0}};

      auto y = multiplyBanded(A, x);
      return y[0, 0] + y[1, 0];  // (2*3 + 1*4) + (1*3 + 2*4) = 10 + 11 = 21
    };

    static_assert(test() == 21.0);
  };

  return TestRegistry::result();
}
