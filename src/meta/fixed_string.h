#pragma once

// FixedString - A compile-time string for non-type template parameters
// Based on https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3094r0.html

#include "meta/utility.h"

namespace tempura {

// For NTTP usage, all data members must be public (literal type requirement).
template <typename CharT, SizeT N>
struct FixedString {
  CharT data_[N + 1] = {};

  // --- Constructors ---

  constexpr FixedString() = default;

  template <SameAs<CharT>... Chars>
    requires(sizeof...(Chars) == N)
  constexpr explicit FixedString(Chars... chars) : data_{chars..., CharT{}} {}

  consteval explicit(false) FixedString(const CharT (&str)[N + 1]) {
    for (SizeT i = 0; i < N; ++i) {
      data_[i] = str[i];
    }
  }

  constexpr FixedString(const CharT* begin, const CharT* end) {
    SizeT i = 0;
    for (const CharT* ptr = begin; ptr != end && i < N; ++ptr, ++i) {
      data_[i] = *ptr;
    }
  }

  // --- Element access ---

  constexpr auto operator[](SizeT idx) const -> const CharT& {
    return data_[idx];
  }
  constexpr auto operator[](SizeT idx) -> CharT& { return data_[idx]; }

  constexpr auto data() const -> const CharT* { return data_; }
  constexpr auto data() -> CharT* { return data_; }
  constexpr auto c_str() const -> const CharT* { return data_; }

  // --- Iterators ---

  constexpr auto begin() const -> const CharT* { return data_; }
  constexpr auto begin() -> CharT* { return data_; }
  constexpr auto end() const -> const CharT* { return data_ + N; }
  constexpr auto end() -> CharT* { return data_ + N; }
  constexpr auto cbegin() const -> const CharT* { return data_; }
  constexpr auto cend() const -> const CharT* { return data_ + N; }

  // --- Capacity ---

  static constexpr auto size() -> SizeT { return N; }
  static constexpr auto length() -> SizeT { return N; }
  static constexpr auto empty() -> bool { return N == 0; }

  // --- Comparison ---

  template <SizeT M>
  constexpr auto operator==(const FixedString<CharT, M>& rhs) const -> bool {
    if constexpr (N != M) return false;
    else {
      for (SizeT i = 0; i < N; ++i) {
        if (data_[i] != rhs.data_[i]) return false;
      }
      return true;
    }
  }

  template <SizeT M>
  constexpr auto operator!=(const FixedString<CharT, M>& rhs) const -> bool {
    return !(*this == rhs);
  }

  template <SizeT M>
  constexpr auto operator<(const FixedString<CharT, M>& rhs) const -> bool {
    SizeT min_len = N < M ? N : M;
    for (SizeT i = 0; i < min_len; ++i) {
      if (data_[i] < rhs.data_[i]) return true;
      if (data_[i] > rhs.data_[i]) return false;
    }
    return N < M;
  }

  template <SizeT M>
  constexpr auto operator>(const FixedString<CharT, M>& rhs) const -> bool {
    return rhs < *this;
  }

  template <SizeT M>
  constexpr auto operator<=(const FixedString<CharT, M>& rhs) const -> bool {
    return !(rhs < *this);
  }

  template <SizeT M>
  constexpr auto operator>=(const FixedString<CharT, M>& rhs) const -> bool {
    return !(*this < rhs);
  }

  // --- Concatenation ---

  template <SizeT M>
  constexpr friend auto operator+(const FixedString& lhs,
                                  const FixedString<CharT, M>& rhs)
      -> FixedString<CharT, N + M> {
    FixedString<CharT, N + M> result;
    for (SizeT i = 0; i < N; ++i) result.data_[i] = lhs.data_[i];
    for (SizeT i = 0; i < M; ++i) result.data_[N + i] = rhs.data_[i];
    return result;
  }

  // Concat with string literal on right: str + "literal"
  template <SizeT M>
  constexpr friend auto operator+(const FixedString& lhs,
                                  const CharT (&rhs)[M])
      -> FixedString<CharT, N + M - 1> {
    return lhs + FixedString<CharT, M - 1>(rhs);
  }

  // Concat with string literal on left: "literal" + str
  template <SizeT M>
  constexpr friend auto operator+(const CharT (&lhs)[M],
                                  const FixedString& rhs)
      -> FixedString<CharT, M - 1 + N> {
    return FixedString<CharT, M - 1>(lhs) + rhs;
  }
};

// Deduction guide: deduce N from array size (size - 1 for null terminator)
template <typename CharT, SizeT N>
FixedString(const CharT (&)[N]) -> FixedString<CharT, N - 1>;

}  // namespace tempura
