#include "simd/simd.h"
#include <cassert>

#include "unit.h"

using namespace tempura;

auto main() -> int {

  // --- Mask8 Tests ---

  "Mask8 And"_test = [] {
    Mask8 mask1(0b11001100);
    Mask8 mask2(0b10101010);
    Mask8 result = mask1 & mask2;

    expectEq(result, Mask8(0b10001000));
  };

  "Mask8 Or"_test = [] {
    Mask8 mask1(0b11001100);
    Mask8 mask2(0b10101010);
    Mask8 result = mask1 | mask2;

    expectEq(result, Mask8(0b11101110));
  };

  "Mask8 Xor"_test = [] {
    Mask8 mask1(0b11001100);
    Mask8 mask2(0b10101010);
    Mask8 result = mask1 ^ mask2;

    expectEq(result, Mask8(0b01100110));
  };

  "Mask8 Not"_test = [] {
    Mask8 mask(0b11001100);
    Mask8 result = ~mask;

    expectEq(result, Mask8(0b00110011));
  };

  "Mask8 Equality"_test = [] {
    Mask8 mask1(0b11001100);
    Mask8 mask2(0b11001100);
    Mask8 mask3(0b10101010);

    expectEq(mask1, mask2);
    expectNeq(mask1, mask3);
  };

  "Mask8 All"_test = [] {
    Mask8 mask(0xFF);
    expectTrue(mask.all());

    Mask8 mask2(0x00);
    expectTrue(!mask2.all());
  };

  "Mask8 None"_test = [] {
    Mask8 mask(0x00);
    expectTrue(mask.none());

    Mask8 mask2(0xFF);
    expectTrue(!mask2.none());
  };

  "Mask8 Any"_test = [] {
    Mask8 mask(0x01);
    expectTrue(mask.any());

    Mask8 mask2(0x00);
    expectTrue(!mask2.any());
  };

  // --- Mask16 Tests ---

  "Mask16 And"_test = [] {
    Mask16 mask1(0b1100110011001100);
    Mask16 mask2(0b1010101010101010);
    Mask16 result = mask1 & mask2;

    expectEq(result, Mask16(0b1000100010001000));
  };

  "Mask16 Or"_test = [] {
    Mask16 mask1(0b1100110011001100);
    Mask16 mask2(0b1010101010101010);
    Mask16 result = mask1 | mask2;

    expectEq(result, Mask16(0b1110111011101110));
  };

  "Mask16 Xor"_test = [] {
    Mask16 mask1(0b1100110011001100);
    Mask16 mask2(0b1010101010101010);
    Mask16 result = mask1 ^ mask2;

    expectEq(result, Mask16(0b0110011001100110));
  };

  "Mask16 Not"_test = [] {
    Mask16 mask(0b1100110011001100);
    Mask16 result = ~mask;

    expectEq(result, Mask16(0b0011001100110011));
  };

  "Mask16 Equality"_test = [] {
    Mask16 mask1(0b1100110011001100);
    Mask16 mask2(0b1100110011001100);
    Mask16 mask3(0b1010101010101010);

    expectEq(mask1, mask2);
    expectNeq(mask1, mask3);
  };

  "Mask16 All"_test = [] {
    Mask16 mask(0xFFFF);
    expectTrue(mask.all());

    Mask16 mask2(0x0000);
    expectTrue(!mask2.all());
  };

  "Mask16 None"_test = [] {
    Mask16 mask(0x0000);
    expectTrue(mask.none());

    Mask16 mask2(0xFFFF);
    expectTrue(!mask2.none());
  };

  "Mask16 Any"_test = [] {
    Mask16 mask(0x0001);
    expectTrue(mask.any());

    Mask16 mask2(0x0000);
    expectTrue(!mask2.any());
  };

  // --- Vec512f Tests ---

  "Vec512f Initialization"_test = [] {
    Vec512f vec1(1.0F);
    Vec512f vec2(2.0F);
    Vec512f vec3(vec1 + vec2);

    expectEq(vec3[0], 3.0F);
    expectEq(vec3[15], 3.0F);
  };

  "Vec512f Load/Store"_test = [] {
    std::array<float, 16> data = {1.0F, 2.0F, 3.0F, 4.0F,
                                  5.0F, 6.0F, 7.0F, 8.0F,
                                  9.0F, 10.0F, 11.0F, 12.0F,
                                  13.0F, 14.0F, 15.0F, 16.0F};

    auto vec = Vec512f(data);
    auto stored_data = vec.storeArray();

    for (size_t i = 0; i < data.size(); ++i) {
      expectEq(stored_data[i], data[i]);
    }
  };

  "Vec512f Addition"_test = [] {
    Vec512f vec1(1.0F);
    Vec512f vec2(2.0F);
    Vec512f result = vec1 + vec2;

    for (size_t i = 0; i < Vec512f::size(); ++i) {
      expectEq(result[i], 3.0F);
    }
  };

  "Vec512f Subtraction"_test = [] {
    Vec512f vec1(3.0F);
    Vec512f vec2(1.0F);
    Vec512f result = vec1 - vec2;

    for (size_t i = 0; i < Vec512f::size(); ++i) {
      expectEq(result[i], 2.0F);
    }
  };

  "Vec512f Multiplication"_test = [] {
    Vec512f vec1(2.0F);
    Vec512f vec2(3.0F);
    Vec512f result = vec1 * vec2;

    for (size_t i = 0; i < Vec512f::size(); ++i) {
      expectEq(result[i], 6.0F);
    }
  };

  "Vec512f Division"_test = [] {
    Vec512f vec1(6.0F);
    Vec512f vec2(2.0F);
    Vec512f result = vec1 / vec2;

    for (size_t i = 0; i < Vec512f::size(); ++i) {
      expectEq(result[i], 3.0F);
    }
  };

  "Vec512f Negation"_test = [] {
    Vec512f vec(1.0F);
    Vec512f negated = -vec;

    for (size_t i = 0; i < Vec512f::size(); ++i) {
      expectEq(negated[i], -1.0F);
    }
  };

  "Vec512f Equality"_test = [] {
    Vec512f vec1(1.0F);
    Vec512f vec2(1.0F);
    Vec512f vec3(2.0F);

    expectTrue(vec1 == vec2);
    expectTrue(vec1 != vec3);
  };

  "Vec512i64 Initialization"_test = [] {
    Vec512i64 vec1(1);
    Vec512i64 vec2(2);
    Vec512i64 vec3(vec1 + vec2);

    expectEq(vec3[0], 3);
    expectEq(vec3[7], 3);
  };

  "Vec512i64 Load/Store"_test = [] {
    std::array<int64_t, 8> data = {1, 2, 3, 4, 5, 6, 7, 8};

    auto vec = Vec512i64(data);
    auto stored_data = vec.storeArray();

    for (size_t i = 0; i < data.size(); ++i) {
      expectEq(stored_data[i], data[i]);
    }
  };

  "Vec512i64 Addition"_test = [] {
    Vec512i64 vec1(1);
    Vec512i64 vec2(2);
    Vec512i64 result = vec1 + vec2;

    for (size_t i = 0; i < Vec512i64::size(); ++i) {
      expectEq(result[i], 3);
    }
  };

  "Vec512i64 Subtraction"_test = [] {
    Vec512i64 vec1(3);
    Vec512i64 vec2(1);
    Vec512i64 result = vec1 - vec2;

    for (size_t i = 0; i < Vec512i64::size(); ++i) {
      expectEq(result[i], 2);
    }
  };

  "Vec512i64 Multiplication"_test = [] {
    Vec512i64 vec1(2);
    Vec512i64 vec2(3);
    Vec512i64 result = vec1 * vec2;

    for (size_t i = 0; i < Vec512i64::size(); ++i) {
      expectEq(result[i], 6);
    }
  };

  "Vec512i64 Right Shift"_test = [] {
    Vec512i64 vec1(8);
    Vec512i64 vec2(1);
    Vec512i64 result = vec1 >> vec2;

    for (size_t i = 0; i < Vec512i64::size(); ++i) {
      expectEq(result[i], 4);
    }
  };

  "Vec512i64 Left Shift"_test = [] {
    Vec512i64 vec1(2);
    Vec512i64 vec2(1);
    Vec512i64 result = vec1 << vec2;

    for (size_t i = 0; i < Vec512i64::size(); ++i) {
      expectEq(result[i], 4);
    }
  };

  return 0;
}
