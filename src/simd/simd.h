#pragma once

#include <immintrin.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <print>
#include <stdexcept>

#include "chebyshev.h"
#include "meta/vector_to_array.h"

namespace tempura {

// A 512-bit vector of 8 64-bit doubles.
//
// This class overloads the arithmetic operators to allow for SIMD operations
// on the vector. It uses AVX-512 intrinsics to perform the operations.
class Vec8d {
 public:
  explicit Vec8d(__m512d data) : data_(data) {}

  // Set all elements to the same value
  explicit Vec8d(double val) : data_(_mm512_set1_pd(val)) {}

  // Precondition:
  //   - data cannot be null
  //   - data must point to an array of at least 8 doubles
  //   - data must be aligned to 64 bytes
  explicit Vec8d(const double* data) { data_ = _mm512_loadu_pd(data); }

  Vec8d(double v0, double v1, double v2, double v3, double v4, double v5,
        double v6, double v7)
      : data_(_mm512_set_pd(v7, v6, v5, v4, v3, v2, v1, v0)) {}

  // Precondition:
  //   - data cannot be null
  //   - data must point to an array of at least 8 doubles
  //   - data must be aligned to 64 bytes
  void store(double* data) const { _mm512_storeu_pd(data, data_); }

  // Precondition:
  //   - index must be in the range [0, 7]
  auto operator[](std::size_t index) const -> double {
    return reinterpret_cast<const double*>(&data_)[index];
  }

  auto operator==(const Vec8d other) const -> bool {
    // Compare all elements for equality
    __mmask8 mask = _mm512_cmp_pd_mask(data_, other.data_, _CMP_EQ_OQ);
    return mask == 0xFF;  // All bits set means all elements are equal
  }

  auto operator!=(const Vec8d other) const -> bool { return !(*this == other); }

  auto operator+(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_add_pd(data_, other.data_)};
  }

  auto operator+=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_add_pd(data_, other.data_);
    return *this;
  }

  auto operator-(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_sub_pd(data_, other.data_)};
  }

  auto operator-=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_sub_pd(data_, other.data_);
    return *this;
  }

  auto operator*(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_mul_pd(data_, other.data_)};
  }

  auto operator*=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_mul_pd(data_, other.data_);
    return *this;
  }

  auto operator/(const Vec8d other) const -> Vec8d {
    return Vec8d{_mm512_div_pd(data_, other.data_)};
  }

  auto operator/=(const Vec8d other) -> Vec8d& {
    data_ = _mm512_div_pd(data_, other.data_);
    return *this;
  }

 private:
  friend auto fma(Vec8d a, Vec8d b, Vec8d c) -> Vec8d {
    return Vec8d{_mm512_fmadd_pd(a.data_, b.data_, c.data_)};
  }

  friend auto round(Vec8d v) -> Vec8d {
    return Vec8d{_mm512_roundscale_pd(v.data_, _MM_FROUND_TO_NEAREST_INT)};
  }

  __m512d data_;
};

class Vec8i64 {
 public:
  Vec8i64() = default;

  explicit Vec8i64(__m512i data) : data_(data) {}


  explicit Vec8i64(uint64_t val)
      : data_(_mm512_set1_epi64(static_cast<int64_t>(val))) {}

  // Precondition:
  //   - data cannot be null
  //   - data must point to an array of at least 8 int64_t
  //   - data must be aligned to 64 bytes
  explicit Vec8i64(const int64_t* data) { data_ = _mm512_loadu_si512(data); }

  Vec8i64(int64_t v0, int64_t v1, int64_t v2, int64_t v3, int64_t v4,
           int64_t v5, int64_t v6, int64_t v7)
      : data_(_mm512_set_epi64(v7, v6, v5, v4, v3, v2, v1, v0)) {}

  constexpr static auto size() -> std::size_t {
    return 8;  // Number of elements in the vector
  }

  // Precondition:
  //   - index must be in the range [0, 7]
  auto operator[](std::size_t index) const -> int64_t {
    // Optimized extraction using 256-bit sub-registers
    // AVX-512 is composed of two 256-bit halves (lower and upper)
    if (index < 4) {
      // Extract lower 256 bits and use AVX2 extract
      __m256i lower = _mm512_castsi512_si256(data_);
      switch (index) {
        case 0: return _mm256_extract_epi64(lower, 0);
        case 1: return _mm256_extract_epi64(lower, 1);
        case 2: return _mm256_extract_epi64(lower, 2);
        case 3: return _mm256_extract_epi64(lower, 3);
      }
    } else {
      // Extract upper 256 bits and use AVX2 extract
      __m256i upper = _mm512_extracti64x4_epi64(data_, 1);
      switch (index) {
        case 4: return _mm256_extract_epi64(upper, 0);
        case 5: return _mm256_extract_epi64(upper, 1);
        case 6: return _mm256_extract_epi64(upper, 2);
        case 7: return _mm256_extract_epi64(upper, 3);
      }
    }

    // Unreachable, but keeps compiler happy
    return 0;
  }

  auto operator==(const Vec8i64 other) const -> bool {
    // Compare all elements for equality
    __mmask8 mask = _mm512_cmpeq_epi64_mask(data_, other.data_);
    return mask == 0xFF;  // All bits set means all elements are equal
  }

  auto operator!=(const Vec8i64 other) const -> bool {
    return !(*this == other);
  }

  auto operator+(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_add_epi64(data_, other.data_)};
  }

  auto operator+=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_add_epi64(data_, other.data_);
    return *this;
  }

  auto operator-(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_sub_epi64(data_, other.data_)};
  }

  auto operator-=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_sub_epi64(data_, other.data_);
    return *this;
  }

  auto operator*(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_mullo_epi64(data_, other.data_)};
  }

  auto operator*=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_mullo_epi64(data_, other.data_);
    return *this;
  }

  auto operator&(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_and_si512(data_, other.data_)};
  }

  auto operator&=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_and_si512(data_, other.data_);
    return *this;
  }

  auto operator^(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_xor_si512(data_, other.data_)};
  }

  auto operator^=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_xor_si512(data_, other.data_);
    return *this;
  }

  auto operator>>(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_srlv_epi64(data_, other.data_)};
  }

  auto operator>>=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_srlv_epi64(data_, other.data_);
    return *this;
  }

  auto operator<<(const Vec8i64 other) const -> Vec8i64 {
    return Vec8i64{_mm512_sllv_epi64(data_, other.data_)};
  }

  auto operator<<=(const Vec8i64 other) -> Vec8i64& {
    data_ = _mm512_sllv_epi64(data_, other.data_);
    return *this;
  }

 private:
  __m512i data_;
};

// A 128-bit vector of 16 8-bit integers (SSE2).
//
// Used for Swiss Table control byte matching - compare 16 bytes in parallel.
class Vec16i8 {
 public:
  static constexpr std::size_t kWidth = 16;

  Vec16i8() = default;

  explicit Vec16i8(__m128i data) : data_(data) {}

  // Broadcast single byte to all 16 positions
  explicit Vec16i8(std::uint8_t val)
      : data_(_mm_set1_epi8(static_cast<char>(val))) {}

  // Load 16 bytes from memory (unaligned)
  static auto load(const std::uint8_t* data) -> Vec16i8 {
    return Vec16i8{_mm_loadu_si128(reinterpret_cast<const __m128i*>(data))};
  }

  // Compare all bytes for equality, returns mask where bit i set if byte i matches
  auto match(std::uint8_t byte) const -> std::uint16_t {
    __m128i pattern = _mm_set1_epi8(static_cast<char>(byte));
    __m128i cmp = _mm_cmpeq_epi8(data_, pattern);
    return static_cast<std::uint16_t>(_mm_movemask_epi8(cmp));
  }

  // Match bytes with high bit set (empty/deleted in Swiss Tables)
  // Returns mask where bit i set if byte i has high bit set
  auto matchHighBitSet() const -> std::uint16_t {
    return static_cast<std::uint16_t>(_mm_movemask_epi8(data_));
  }

  // Access individual byte
  auto operator[](std::size_t index) const -> std::uint8_t {
    alignas(16) std::uint8_t bytes[16];
    _mm_store_si128(reinterpret_cast<__m128i*>(bytes), data_);
    return bytes[index];
  }

 private:
  __m128i data_;
};

// A 256-bit vector of 32 8-bit integers (AVX2).
//
// Used for Swiss Table control byte matching - compare 32 bytes in parallel.
class Vec32i8 {
 public:
  static constexpr std::size_t kWidth = 32;

  Vec32i8() = default;

  explicit Vec32i8(__m256i data) : data_(data) {}

  // Broadcast single byte to all 32 positions
  explicit Vec32i8(std::uint8_t val)
      : data_(_mm256_set1_epi8(static_cast<char>(val))) {}

  // Load 32 bytes from memory (unaligned)
  static auto load(const std::uint8_t* data) -> Vec32i8 {
    return Vec32i8{_mm256_loadu_si256(reinterpret_cast<const __m256i*>(data))};
  }

  // Compare all bytes for equality, returns 32-bit mask where bit i set if byte i matches
  auto match(std::uint8_t byte) const -> std::uint32_t {
    __m256i pattern = _mm256_set1_epi8(static_cast<char>(byte));
    __m256i cmp = _mm256_cmpeq_epi8(data_, pattern);
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(cmp));
  }

  // Match bytes with high bit set (empty/deleted in Swiss Tables)
  // Returns mask where bit i set if byte i has high bit set
  auto matchHighBitSet() const -> std::uint32_t {
    return static_cast<std::uint32_t>(_mm256_movemask_epi8(data_));
  }

  // Access individual byte
  auto operator[](std::size_t index) const -> std::uint8_t {
    alignas(32) std::uint8_t bytes[32];
    _mm256_store_si256(reinterpret_cast<__m256i*>(bytes), data_);
    return bytes[index];
  }

 private:
  __m256i data_;
};

// A 512-bit vector of 64 8-bit integers (AVX-512BW).
//
// For processing 4 Swiss Table groups at once (64 control bytes).
class Vec64i8 {
 public:
  static constexpr std::size_t kWidth = 64;

  Vec64i8() = default;

  explicit Vec64i8(__m512i data) : data_(data) {}

  // Broadcast single byte to all 64 positions
  explicit Vec64i8(std::uint8_t val)
      : data_(_mm512_set1_epi8(static_cast<char>(val))) {}

  // Load 64 bytes from memory (unaligned)
  static auto load(const std::uint8_t* data) -> Vec64i8 {
    return Vec64i8{_mm512_loadu_si512(data)};
  }

  // Compare all bytes for equality, returns 64-bit mask
  auto match(std::uint8_t byte) const -> std::uint64_t {
    __m512i pattern = _mm512_set1_epi8(static_cast<char>(byte));
    return _mm512_cmpeq_epi8_mask(data_, pattern);
  }

  // Match bytes with high bit set
  auto matchHighBitSet() const -> std::uint64_t {
    // Extract the most significant bit of each byte directly
    return _mm512_movepi8_mask(data_);
  }

  // Access individual byte
  auto operator[](std::size_t index) const -> std::uint8_t {
    alignas(64) std::uint8_t bytes[64];
    _mm512_store_si512(bytes, data_);
    return bytes[index];
  }

 private:
  __m512i data_;
};

// ============================================================================
// Advanced SIMD Operations (leveraging Zen 5 capabilities)
// ============================================================================

// Vectorized popcount per byte (AVX-512 BITALG)
// Returns vector where each byte contains popcount of corresponding input byte
inline auto popcntPerByte(__m512i v) -> __m512i {
  return _mm512_popcnt_epi8(v);
}

// Vectorized popcount per 64-bit element (AVX-512 VPOPCNTDQ)
inline auto popcntPer64(__m512i v) -> __m512i {
  return _mm512_popcnt_epi64(v);
}

// Count total set bits across all 64 bytes
inline auto totalPopcount(__m512i v) -> int {
  // Sum popcounts: bytes -> 64-bit elements -> scalar
  __m512i byte_counts = _mm512_popcnt_epi8(v);
  __m512i sad = _mm512_sad_epu8(byte_counts, _mm512_setzero_si512());
  return static_cast<int>(_mm512_reduce_add_epi64(sad));
}

// Compress bytes based on mask (AVX-512 VBMI2)
// Packs selected bytes contiguously at the start
inline auto compressBytes(__m512i data, std::uint64_t mask) -> __m512i {
  return _mm512_maskz_compress_epi8(mask, data);
}

// Expand bytes based on mask (AVX-512 VBMI2)
// Spreads contiguous bytes to masked positions
inline auto expandBytes(__m512i data, std::uint64_t mask) -> __m512i {
  return _mm512_maskz_expand_epi8(mask, data);
}

// Hardware CRC32 (useful for hashing)
inline auto crc32(std::uint64_t crc, std::uint64_t data) -> std::uint64_t {
  return _mm_crc32_u64(crc, data);
}

// Hardware random number generation
inline auto hardwareRandom64() -> std::uint64_t {
  unsigned long long result;
  while (!_rdseed64_step(&result)) {}
  return result;
}

// AES single round (AES-NI) - useful for fast hashing
inline auto aesEnc(__m128i data, __m128i key) -> __m128i {
  return _mm_aesenc_si128(data, key);
}

// Vectorized AES (VAES) - 4 AES rounds in parallel
inline auto vaesEnc(__m512i data, __m512i key) -> __m512i {
  return _mm512_aesenc_epi128(data, key);
}

// Galois field affine transformation (GFNI)
// Applies 8x8 bit matrix transformation - useful for exotic hash functions
inline auto gfniAffine(__m512i data, __m512i matrix, int imm8) -> __m512i {
  return _mm512_gf2p8affine_epi64_epi8(data, matrix, imm8);
}

} // namespace tempura
