#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <ranges>

namespace tempura {

template <typename T, size_t N>
class Polynomial {
 public:
  constexpr Polynomial() = default;
  constexpr Polynomial(const Polynomial&) = default;
  constexpr Polynomial(Polynomial&&) = default;

  constexpr explicit Polynomial(std::ranges::input_range auto&& coefficients) {
    assert(std::ranges::size(coefficients) == N);
    std::ranges::copy(coefficients, coefficients_.begin());
  }

  constexpr Polynomial(std::initializer_list<T> coefficients) {
    assert(coefficients.size() == N);
    std::ranges::copy(coefficients, coefficients_.begin());
  }

  constexpr auto operator[](this auto&& self, size_t i) -> decltype(auto) {
    return self.coefficients_[i];
  }

  constexpr auto operator()(const auto& x) const -> T {
    T ans = coefficients_[N - 1];
    for (int i = N - 2; i >= 0; --i) {
      ans = ans * x + coefficients_[i];
    }
    return ans;
  }

  constexpr auto begin(this auto&& self) -> decltype(auto) {
    return self.coefficients_.begin();
  }

  constexpr auto end(this auto&& self) -> decltype(auto) {
    return self.coefficients_.end();
  }

  constexpr auto cbegin() const -> decltype(auto) {
    return coefficients_.cbegin();
  }

  constexpr auto cend() const -> decltype(auto) { return coefficients_.cend(); }

  constexpr auto operator==(const Polynomial&) const -> bool = default;

  template <typename U, size_t M>
    requires(N >= M)
  constexpr auto operator+=(const Polynomial<U, M>& other)
      -> Polynomial<T, N>& {
    for (size_t i = 0; i < std::min(N, M); ++i) {
      coefficients_[i] += other[i];
    }
    return *this;
  }

  template <typename U, size_t M>
  constexpr auto operator+(const Polynomial<U, M>& other) const
      -> Polynomial<T, N> {
    using Type = decltype(std::declval<T>() + std::declval<U>());
    if constexpr (N < M) {
      Polynomial<Type, M> ans = other;
      other += *this;
      return ans;
    } else {
      Polynomial<Type, N> ans = *this;
      ans += other;
      return ans;
    }
  }

  template <typename U, size_t M>
    requires(N >= M)
  constexpr auto operator-=(const Polynomial<U, M>& other)
      -> Polynomial<T, N>& {
    for (size_t i = 0; i < std::min(N, M); ++i) {
      coefficients_[i] -= other[i];
    }
    return *this;
  }

  template <typename U, size_t M>
  constexpr auto operator-(const Polynomial<U, M>& other) const
      -> Polynomial<T, N> {
    using Type = decltype(std::declval<T>() - std::declval<U>());
    if constexpr (N < M) {
      Polynomial<Type, M> ans = other;
      other -= *this;
      return ans;
    } else {
      Polynomial<Type, N> ans = *this;
      ans -= other;
      return ans;
    }
  }

  template <typename U, size_t M>
  constexpr auto operator*(const Polynomial<U, M>& other) const
      -> Polynomial<T, N + M - 1> {
    using Type = decltype(std::declval<T>() * std::declval<U>());
    Polynomial<Type, N + M - 1> ans;
    for (size_t i = 0; i < N; ++i) {
      for (size_t j = 0; j < M; ++j) {
        ans[i + j] += coefficients_[i] * other[j];
      }
    }
    return ans;
  }

 private:
  std::array<T, N> coefficients_ = {};
};

}  // namespace tempura
