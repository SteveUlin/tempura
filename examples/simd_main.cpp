#include <immintrin.h>

#include <print>
#include <string>
#include <string_view>

#include "profiler.h"

using namespace tempura;

auto toString(const __m256& value) -> std::string {
  std::string result;
  const auto* ptr = reinterpret_cast<const float*>(&value);
  result += "[";
  for (int i = 0; i < 8; ++i) {
    result += std::to_string(ptr[i]);
    result += (i == 7) ? "" : ", ";
  }
  result += "]";
  return result;
}

auto toString(const __m512d& value) -> std::string {
  std::string result;
  const auto* ptr = reinterpret_cast<const double*>(&value);
  result += "[";
  for (int i = 0; i < 8; ++i) {
    result += std::to_string(ptr[i]);
    result += (i == 7) ? "" : ", ";
  }
  result += "]";
  return result;
}

auto main() -> int {
  std::println("=== SIMD Example ===\n");

  {
    std::println("Example 1: Sum of two vectors");

    // Create a vector with 8 floats: 32bits * 8 => 256
    __m256 a = _mm256_set_ps(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F);
    __m256 b = _mm256_set_ps(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F);

    __m256 c = _mm256_add_ps(a, b);
    std::println("a: {}", toString(a));
    std::println("b: {}", toString(b));
    std::println("a + b = c: {}", toString(c));
  }

  {
    std::println("\nExample 2: Storing SIMD results");
    __m256 a = _mm256_set_ps(1.0F, 2.0F, 3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F);
    alignas(256) std::array<float, 8> result = {};

    _mm256_store_ps(result.data(), a);

    std::print("[");
    for (int i = 0; i < 8; ++i) {
      std::print("{}", result[i]);
      if (i != 7) {
        std::print(", ");
      }
    }
    std::print("]\n");
  }

  {
    std::println("\nExample 3: SIMD with doubles");
    __m512d a = _mm512_set_pd(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);
    __m512d b = _mm512_set_pd(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0);

    __m512d c = _mm512_add_pd(a, b);
    std::println("a: {}", toString(a));
    std::println("b: {}", toString(b));
    std::println("a + b = c: {}", toString(c));
  }

  constexpr int N = 1'000'000;

  {
    std::println("Initializing a vector with 8 doubles");
    std::array<double, 8> data = {};
    TEMPURA_TRACE()
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < 8; ++j) {
        data[j] = 10.;
      }
    }
  }

  {
    std::println("Initializing a vector with 8 doubles using SIMD");
    __m512d data = _mm512_set1_pd(10.);
    TEMPURA_TRACE()
    for (int i = 0; i < N; ++i) {
      // data = _mm512_set1_pd(10.);
    }
  }

  return 0;
}
