#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace tempura {

namespace internal {

consteval auto bufLength(std::integral auto N) -> size_t {
  if (N == 0) {
    // '0', \0
    return 2;
  }

  // \0
  size_t len = 1;

  if (N < 0) {
    ++len;
    N *= -1;
  }
  while (N > 0) {
    ++len;
    N /= 10;
  }
  return len;
}

} // namespace internal

template <size_t N>
struct CompileTimeString {
  consteval CompileTimeString(const char (&str)[N]) { std::copy_n(str, N, value.begin()); }
  consteval CompileTimeString(const std::array<char, N>& str) { std::copy_n(str.begin(), N, value.begin()); }

  std::array<char, N> value = {};
};

template <CompileTimeString cts>
consteval auto operator""_cts() {
  return cts;
}

template <size_t N, size_t M>
consteval auto operator==(CompileTimeString<N> lhs, CompileTimeString<M> rhs) -> bool {
  return std::ranges::equal(lhs.value, rhs.value);
}

template <size_t N, size_t M>
consteval auto operator!=(CompileTimeString<N> lhs, CompileTimeString<M> rhs) -> bool {
  return !(lhs == rhs);
}

template <size_t N, size_t M>
consteval auto operator+(CompileTimeString<N> lhs,
                         CompileTimeString<M> rhs) -> CompileTimeString<N + M - 1> {
  std::array<char, N + M - 1> result = {};

  std::copy_n(lhs.value.begin(), N, result.begin());
  std::copy_n(rhs.value.begin(), M, result.begin() + N - 1);

  return { result };
}

template <std::integral auto VALUE>
consteval auto toCTS() {
  if constexpr (VALUE == 0) {
    return "0"_cts;
  } else {
    std::array<char, internal::bufLength(VALUE)> result = {};
    auto N = VALUE;
    if (N < 0) {
      result[0] = '-';
      N *= -1;
    }

    auto itr = ++result.rbegin();
    while (N > 0) {
      *itr = '0' + N % 10;
      N /= 10;
      ++itr;
    }
    return CompileTimeString{ result };
  }
}

template <std::floating_point auto VALUE>
consteval auto toCTS() {
  constexpr int64_t INT_PART = static_cast<int64_t>(VALUE);
  constexpr int64_t DECIMAL_PART = static_cast<int64_t>(std::abs((VALUE - INT_PART) * 1'000));
  return toCTS<INT_PART>() + "."_cts + toCTS<DECIMAL_PART>();
}


}  // namespace tempura

