#include "simd/simd.h"

#include <cassert>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "Vec8d initialization"_test = [] {
    Vec8d v1{1.0};
    Vec8d v2{2.0};
    Vec8d v3{3.0};

    assert(v1[0] == 1.0);
    assert(v2[0] == 2.0);
    assert(v3[0] == 3.0);
  };

  "Vec8d addition"_test = [] {
    Vec8d v1{1.0};
    Vec8d v2{2.0};
    Vec8d v3 = v1 + v2;

    assert(v3[0] == 3.0);
    assert(v3[1] == 3.0);
    assert(v3[2] == 3.0);
    assert(v3[3] == 3.0);
    assert(v3[4] == 3.0);
    assert(v3[5] == 3.0);
    assert(v3[6] == 3.0);
    assert(v3[7] == 3.0);
  };

  "Vec8d subtraction"_test = [] {
    Vec8d v1{3.0};
    Vec8d v2{2.0};
    Vec8d v3 = v1 - v2;

    assert(v3[0] == 1.0);
    assert(v3[1] == 1.0);
    assert(v3[2] == 1.0);
    assert(v3[3] == 1.0);
    assert(v3[4] == 1.0);
    assert(v3[5] == 1.0);
    assert(v3[6] == 1.0);
    assert(v3[7] == 1.0);
  };

  "Vec8d multiplication"_test = [] {
    Vec8d v1{2.0};
    Vec8d v2{3.0};
    Vec8d v3 = v1 * v2;

    assert(v3[0] == 6.0);
    assert(v3[1] == 6.0);
    assert(v3[2] == 6.0);
    assert(v3[3] == 6.0);
    assert(v3[4] == 6.0);
    assert(v3[5] == 6.0);
    assert(v3[6] == 6.0);
    assert(v3[7] == 6.0);
  };

  "Vec8d division"_test = [] {
    Vec8d v1{6.0};
    Vec8d v2{3.0};
    Vec8d v3 = v1 / v2;

    assert(v3[0] == 2.0);
    assert(v3[1] == 2.0);
    assert(v3[2] == 2.0);
    assert(v3[3] == 2.0);
    assert(v3[4] == 2.0);
    assert(v3[5] == 2.0);
    assert(v3[6] == 2.0);
    assert(v3[7] == 2.0);
  };

  "Vec8d equality"_test = [] {
    Vec8d v1{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    Vec8d v2{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    Vec8d v3{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 9.0};

    assert(v1 == v2);
    assert(v1 != v3);
    assert(v2 != v3);
  };

  "Vec8d round"_test = [] {
    Vec8d v1{0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5};
    Vec8d rounded = round(v1);

    expectNear(0.0, rounded[0]);
    expectNear(0.0, rounded[1]);
    expectNear(1.0, rounded[2]);
    expectNear(2.0, rounded[3]);
    expectNear(2.0, rounded[4]);
    expectNear(2.0, rounded[5]);
    expectNear(3.0, rounded[6]);
    expectNear(4.0, rounded[7]);
  };

  "Vec8d fma"_test = [] {
    Vec8d a{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    Vec8d b{2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    Vec8d c{10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0};
    Vec8d result = fma(a, b, c);

    assert(result[0] == 12.0);
    assert(result[1] == 26.0);
    assert(result[2] == 42.0);
    assert(result[3] == 60.0);
    assert(result[4] == 80.0);
    assert(result[5] == 102.0);
    assert(result[6] == 126.0);
    assert(result[7] == 152.0);
  };

  "Vec8i64 initialization"_test = [] {
    Vec8i64 v1{1, 2, 3, 4, 5, 6, 7, 8};

    assert(v1[0] == 1);
    assert(v1[1] == 2);
    assert(v1[2] == 3);
    assert(v1[3] == 4);
    assert(v1[4] == 5);
    assert(v1[5] == 6);
    assert(v1[6] == 7);
    assert(v1[7] == 8);
  };

  "Vec8i64 addition"_test = [] {
    Vec8i64 v1{1, 2, 3, 4, 5, 6, 7, 8};
    Vec8i64 v2{8, 7, 6, 5, 4, 3, 2, 1};
    Vec8i64 v3 = v1 + v2;

    assert(v3[0] == 9);
    assert(v3[1] == 9);
    assert(v3[2] == 9);
    assert(v3[3] == 9);
    assert(v3[4] == 9);
    assert(v3[5] == 9);
    assert(v3[6] == 9);
    assert(v3[7] == 9);
  };

  "Vec8i64 subtraction"_test = [] {
    Vec8i64 v1{10, 20, 30, 40, 50, 60, 70, 80};
    Vec8i64 v2{1, 2, 3, 4, 5, 6, 7, 8};
    Vec8i64 v3 = v1 - v2;

    assert(v3[0] == 9);
    assert(v3[1] == 18);
    assert(v3[2] == 27);
    assert(v3[3] == 36);
    assert(v3[4] == 45);
    assert(v3[5] == 54);
    assert(v3[6] == 63);
    assert(v3[7] == 72);
  };

  "Vec8i64 multiplication"_test = [] {
    Vec8i64 v1{1, 2, 3, 4, 5, 6, 7, 8};
    Vec8i64 v2{2, 3, 4, 5, 6, 7, 8, 9};
    Vec8i64 v3 = v1 * v2;

    assert(v3[0] == 2);
    assert(v3[1] == 6);
    assert(v3[2] == 12);
    assert(v3[3] == 20);
    assert(v3[4] == 30);
    assert(v3[5] == 42);
    assert(v3[6] == 56);
    assert(v3[7] == 72);
  };

  "Vec8i64 xor"_test = [] {
    Vec8i64 v1{1, 2, 3, 4, 5, 6, 7, 8};
    Vec8i64 v2{8, 7, 6, 5, 4, 3, 2, 1};
    Vec8i64 v3 = v1 ^ v2;

    assert(v3[0] == 9);
    assert(v3[1] == 5);
    assert(v3[2] == 5);
    assert(v3[3] == 1);
    assert(v3[4] == 1);
    assert(v3[5] == 5);
    assert(v3[6] == 5);
    assert(v3[7] == 9);
  };

  "Vec8i64 right shift"_test = [] {
    Vec8i64 v1{8, 16, 32, 64, 128, 256, 512, 1024};
    Vec8i64 v2 = v1 >> Vec8i64{2};

    assert(v2[0] == 2);
    assert(v2[1] == 4);
    assert(v2[2] == 8);
    assert(v2[3] == 16);
    assert(v2[4] == 32);
    assert(v2[5] == 64);
    assert(v2[6] == 128);
    assert(v2[7] == 256);
  };

  "Vec8i64 left shift"_test = [] {
    Vec8i64 v1{1, 2, 3, 4, 5, 6, 7, 8};
    Vec8i64 v2 = v1 << Vec8i64{2};

    assert(v2[0] == 4);
    assert(v2[1] == 8);
    assert(v2[2] == 12);
    assert(v2[3] == 16);
    assert(v2[4] == 20);
    assert(v2[5] == 24);
    assert(v2[6] == 28);
    assert(v2[7] == 32);
  };

  return 0;
}
