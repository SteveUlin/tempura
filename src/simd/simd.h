#include <array>
#include <stdexcept>
#include <immintrin.h>

// My personal implementation of avx512 simd data types

namespace tempura {

// --- Mask Types ---

class Mask8 {
public:
  Mask8(__mmask8 mask) : value_(mask) {}

  auto operator&(const Mask8& other) const -> Mask8 {
    return {_kand_mask8(value_, other.value_)};
  }

  auto operator|(const Mask8& other) const -> Mask8 {
    return {_kor_mask8(value_, other.value_)};
  }

  auto operator^(const Mask8& other) const -> Mask8 {
    return {_kxor_mask8(value_, other.value_)};
  }

  auto operator~() const -> Mask8 {
    return {_knot_mask8(value_)};
  }

  auto operator==(const Mask8& other) const -> bool {
    return value_ == other.value_;
  }

  auto all() const -> bool {
    return value_ == 0xFF;
  }

  auto none() const -> bool {
    return value_ == 0x00;
  }

  auto any() const -> bool {
    return value_ != 0x00;
  }

private:
  __mmask8 value_;
};

class Mask16 {
public:
  Mask16(__mmask16 mask) : value(mask) {}

  operator __mmask16() const { return value; }

  auto operator&(const Mask16& other) const -> Mask16 {
    return {_kand_mask16(value, other.value)};
  }

  auto operator|(const Mask16& other) const -> Mask16 {
    return {_kor_mask16(value, other.value)};
  }

  auto operator^(const Mask16& other) const -> Mask16 {
    return {_kxor_mask16(value, other.value)};
  }

  auto operator~() const -> Mask16 {
    return {_knot_mask16(value)};
  }

  auto operator==(const Mask16& other) const -> bool {
    return value == other.value;
  }

  auto all() const -> bool {
    return value == 0xFFFF;
  }

  auto none() const -> bool {
    return value == 0x0000;
  }

  auto any() const -> bool {
    return value != 0x0000;
  }

private:
  __mmask16 value;
};

// --- Vec512f: 512 bit vector of 16 single-precision floats ---

class Vec512f {
public:
  Vec512f() : value(_mm512_setzero_ps()) {}
  explicit Vec512f(__m512 v) : value(v) {}
  explicit Vec512f(float f) : value(_mm512_set1_ps(f)) {}
  explicit Vec512f(const float* ptr) : value(_mm512_loadu_ps(ptr)) {}
  explicit Vec512f(const std::array<float, 16>& arr)
      : value(_mm512_loadu_ps(arr.data())) {}

  static constexpr auto size() -> std::size_t {
    return 16;  // 512 bits / 32 bits per float
  }

  void store(float* ptr) const {
    _mm512_storeu_ps(ptr, value);
  }

  auto storeArray() const -> std::array<float, 16> {
    std::array<float, 16> arr;
    _mm512_storeu_ps(arr.data(), value);
    return arr;
  }

  auto operator[](std::size_t index) const -> float {
    if (index >= 16) {
      throw std::out_of_range("Index out of range for Vec512f");
    }
    return reinterpret_cast<const float*>(&value)[index];
  }

  auto operator+(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_add_ps(value, other.value)};
  }

  auto operator+=(const Vec512f& other) -> Vec512f& {
    value = _mm512_add_ps(value, other.value);
    return *this;
  }

  auto operator-(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_sub_ps(value, other.value)};
  }

  auto operator-=(const Vec512f& other) -> Vec512f& {
    value = _mm512_sub_ps(value, other.value);
    return *this;
  }

  auto operator*(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_mul_ps(value, other.value)};
  }

  auto operator*=(const Vec512f& other) -> Vec512f& {
    value = _mm512_mul_ps(value, other.value);
    return *this;
  }

  auto operator/(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_div_ps(value, other.value)};
  }

  auto operator/=(const Vec512f& other) -> Vec512f& {
    value = _mm512_div_ps(value, other.value);
    return *this;
  }

  auto operator==(const Vec512f& other) const -> bool {
    return _mm512_cmp_ps_mask(value, other.value, _CMP_EQ_OQ) == 0xFFFF;
  }

  auto operator!=(const Vec512f& other) const -> bool {
    return !(*this == other);
  }

private:
  __m512 value;
};

}  // namespace tempura
