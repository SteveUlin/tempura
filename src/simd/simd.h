#include <immintrin.h>

#include <array>
#include <cstdint>
#include <stdexcept>

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

  auto operator~() const -> Mask8 { return {_knot_mask8(value_)}; }

  auto operator==(const Mask8& other) const -> bool {
    return value_ == other.value_;
  }

  auto all() const -> bool { return value_ == 0xFF; }

  auto none() const -> bool { return value_ == 0x00; }

  auto any() const -> bool { return value_ != 0x00; }

 private:
  __mmask8 value_;
};

class Mask16 {
 public:
  Mask16(__mmask16 mask) : value_(mask) {}

  operator __mmask16() const { return value_; }

  auto operator&(const Mask16& other) const -> Mask16 {
    return {_kand_mask16(value_, other.value_)};
  }

  auto operator|(const Mask16& other) const -> Mask16 {
    return {_kor_mask16(value_, other.value_)};
  }

  auto operator^(const Mask16& other) const -> Mask16 {
    return {_kxor_mask16(value_, other.value_)};
  }

  auto operator~() const -> Mask16 { return {_knot_mask16(value_)}; }

  auto operator==(const Mask16& other) const -> bool {
    return value_ == other.value_;
  }

  auto all() const -> bool { return value_ == 0xFFFF; }

  auto none() const -> bool { return value_ == 0x0000; }

  auto any() const -> bool { return value_ != 0x0000; }

 private:
  __mmask16 value_;
};

// --- Vec512f: 512 bit vector of 16 single-precision floats ---

class Vec512f {
 public:
  Vec512f() : value_(_mm512_setzero_ps()) {}
  explicit Vec512f(__m512 v) : value_(v) {}
  explicit Vec512f(float f) : value_(_mm512_set1_ps(f)) {}
  explicit Vec512f(const float* ptr) : value_(_mm512_loadu_ps(ptr)) {}
  explicit Vec512f(const std::array<float, 16>& arr)
      : value_(_mm512_loadu_ps(arr.data())) {}

  static constexpr auto size() -> std::size_t {
    return 16;  // 512 bits / 32 bits per float
  }

  void store(float* ptr) const { _mm512_storeu_ps(ptr, value_); }

  auto storeArray() const -> std::array<float, 16> {
    std::array<float, 16> arr;
    _mm512_storeu_ps(arr.data(), value_);
    return arr;
  }

  auto operator[](std::size_t index) const -> float {
    if (index >= 16) {
      throw std::out_of_range("Index out of range for Vec512f");
    }
    return reinterpret_cast<const float*>(&value_)[index];
  }

  auto operator+(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_add_ps(value_, other.value_)};
  }

  auto operator+=(const Vec512f& other) -> Vec512f& {
    value_ = _mm512_add_ps(value_, other.value_);
    return *this;
  }

  auto operator-(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_sub_ps(value_, other.value_)};
  }

  auto operator-=(const Vec512f& other) -> Vec512f& {
    value_ = _mm512_sub_ps(value_, other.value_);
    return *this;
  }

  auto operator*(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_mul_ps(value_, other.value_)};
  }

  auto operator*=(const Vec512f& other) -> Vec512f& {
    value_ = _mm512_mul_ps(value_, other.value_);
    return *this;
  }

  auto operator/(const Vec512f& other) const -> Vec512f {
    return Vec512f{_mm512_div_ps(value_, other.value_)};
  }

  auto operator/=(const Vec512f& other) -> Vec512f& {
    value_ = _mm512_div_ps(value_, other.value_);
    return *this;
  }

  auto operator-() const -> Vec512f {
    return Vec512f{_mm512_sub_ps(_mm512_setzero_ps(), value_)};
  }

  auto operator==(const Vec512f& other) const -> bool {
    return _mm512_cmp_ps_mask(value_, other.value_, _CMP_EQ_OQ) == 0xFFFF;
  }

  auto operator!=(const Vec512f& other) const -> bool {
    return !(*this == other);
  }

 private:
  __m512 value_;
};

class Vec512i64 {
 public:
  Vec512i64() : value_(_mm512_setzero_si512()) {}
  explicit Vec512i64(__m512i v) : value_(v) {}
  explicit Vec512i64(uint64_t i) : value_(_mm512_set1_epi64(i)) {}
  explicit Vec512i64(const uint64_t* ptr) : value_(_mm512_loadu_si512(ptr)) {}
  explicit Vec512i64(const std::array<uint64_t, 8>& arr)
      : value_(_mm512_loadu_si512(arr.data())) {}

  static constexpr auto size() -> std::size_t {
    return 8;  // 512 bits / 64 bits per int64_t
  }

  void store(uint64_t* ptr) const { _mm512_storeu_si512(ptr, value_); }

  auto storeArray() const -> std::array<uint64_t, 8> {
    std::array<uint64_t, 8> arr;
    _mm512_storeu_si512(arr.data(), value_);
    return arr;
  }

  auto operator[](std::size_t index) const -> uint64_t {
    if (index >= 8) {
      throw std::out_of_range("Index out of range for Vec512i64");
    }
    return reinterpret_cast<const uint64_t*>(&value_)[index];
  }

  auto operator+(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_add_epi64(value_, other.value_)};
  }

  auto operator+=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_add_epi64(value_, other.value_);
    return *this;
  }

  auto operator-(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_sub_epi64(value_, other.value_)};
  }

  auto operator-=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_sub_epi64(value_, other.value_);
    return *this;
  }

  auto operator*(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_mullo_epi64(value_, other.value_)};
  }

  auto operator*=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_mullo_epi64(value_, other.value_);
    return *this;
  }

  auto operator&(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_and_epi64(value_, other.value_)};
  }

  auto operator&=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_and_epi64(value_, other.value_);
    return *this;
  }

  auto operator&(const long long other) const -> Vec512i64 {
    return Vec512i64{_mm512_and_epi64(value_, _mm512_set1_epi64(other))};
  }

  auto operator&=(const long long other) -> Vec512i64& {
    value_ = _mm512_and_epi64(value_, _mm512_set1_epi64(other));
    return *this;
  }

  auto operator^(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_xor_epi64(value_, other.value_)};
  }

  auto operator^=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_xor_epi64(value_, other.value_);
    return *this;
  }

  auto operator>>(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_srlv_epi64(value_, other.value_)};
  }

  auto operator>>=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_srlv_epi64(value_, other.value_);
    return *this;
  }

  auto operator>>(const long long other) const -> Vec512i64 {
    return Vec512i64{_mm512_srlv_epi64(value_, _mm512_set1_epi64(other))};
  }

  auto operator>>=(const long long other) -> Vec512i64& {
    value_ = _mm512_srlv_epi64(value_, _mm512_set1_epi64(other));
    return *this;
  }

  auto operator<<(const Vec512i64 other) const -> Vec512i64 {
    return Vec512i64{_mm512_sllv_epi64(value_, other.value_)};
  }

  auto operator<<=(const Vec512i64 other) -> Vec512i64& {
    value_ = _mm512_sllv_epi64(value_, other.value_);
    return *this;
  }

 private:
  __m512i value_;
};

}  // namespace tempura
