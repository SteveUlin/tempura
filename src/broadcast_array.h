#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <ranges>

namespace tempura {

template <typename T, size_t N>
class BroadcastArray {
 public:
  constexpr BroadcastArray() = default;

  constexpr BroadcastArray(const std::array<T, N>& data) : data_{data} {}

  constexpr BroadcastArray(std::array<T, N>&& data)
      : data_{std::forward<std::array<T, N>&&>(data)} {}

  constexpr BroadcastArray(T value) {
    for (auto& element : data_) {
      element = value;
    }
  }

  template <typename... Args>
    requires(sizeof...(Args) == N) && (std::convertible_to<Args, T> && ...)
  constexpr BroadcastArray(Args&&... values)
      : data_{std::forward<Args&&>(values)...} {}

  constexpr auto operator[](size_t i) -> T& { return data_[i]; }

  constexpr auto operator[](size_t i) const -> const T& { return data_[i]; }

  constexpr auto begin() -> T* { return data_.begin(); }

  constexpr auto begin() const -> const T* { return data_.begin(); }

  constexpr auto end() -> T* { return data_.end(); }

  constexpr auto end() const -> const T* { return data_.end(); }

  constexpr auto size() const -> size_t { return N; }

 private:
  std::array<T, N> data_;
};

template <size_t I, typename T, size_t N>
auto get(BroadcastArray<T, N>& array) -> T& {
  return array[I];
}

template <size_t I, typename T, size_t N>
auto get(const BroadcastArray<T, N>& array) -> const T& {
  return array[I];
}

// deduction guide
template <typename... Args>
BroadcastArray(Args&&...)
    -> BroadcastArray<std::common_type_t<Args...>, sizeof...(Args)>;

template <typename T, typename U, size_t N>
constexpr auto operator==(const BroadcastArray<T, N>& left,
                          const BroadcastArray<U, N>& right) -> bool {
  return std::ranges::equal(left, right);
}

template <typename T, typename U, size_t N>
auto operator+=(BroadcastArray<T, N>& left,
                const BroadcastArray<U, N>& right) -> BroadcastArray<T, N>& {
  for (auto [l, r] : std::views::zip(left, right)) {
    l += r;
  }
  return left;
}

template <typename T, typename U, size_t N>
auto operator+=(BroadcastArray<T, N>& left,
                const U& right) -> BroadcastArray<T, N>& {
  for (auto& value : left) {
    value += right;
  }
  return left;
}

template <typename T, typename U, size_t N>
constexpr auto operator+(const BroadcastArray<T, N>& left,
                         const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() + std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, r, res] : std::views::zip(left, right, result)) {
    res = l + r;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator+(const BroadcastArray<T, N>& left, const U& right) {
  using V = decltype(std::declval<T>() + std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, res] : std::views::zip(left, result)) {
    res = l + right;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator+(const T& left, const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() + std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [r, res] : std::views::zip(right, result)) {
    res = left + r;
  }
  return result;
}

template <typename T, typename U, size_t N>
auto operator-=(BroadcastArray<T, N>& left,
                const BroadcastArray<U, N>& right) -> BroadcastArray<T, N>& {
  for (auto [l, r] : std::views::zip(left, right)) {
    l -= r;
  }
  return left;
}

template <typename T, typename U, size_t N>
auto operator-=(BroadcastArray<T, N>& left,
                const U& right) -> BroadcastArray<T, N>& {
  for (auto& value : left) {
    value -= right;
  }
  return left;
}

template <typename T, typename U, size_t N>
constexpr auto operator-(const BroadcastArray<T, N>& left,
                         const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() - std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, r, res] : std::views::zip(left, right, result)) {
    res = l - r;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator-(const BroadcastArray<T, N>& left, const U& right) {
  using V = decltype(std::declval<T>() - std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, res] : std::views::zip(left, result)) {
    res = l - right;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator-(const T& left, const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() - std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [r, res] : std::views::zip(right, result)) {
    res = left - r;
  }
  return result;
}

template <typename T, typename U, size_t N>
auto operator*=(BroadcastArray<T, N>& left,
                const BroadcastArray<U, N>& right) -> BroadcastArray<T, N>& {
  for (auto [l, r] : std::views::zip(left, right)) {
    l *= r;
  }
  return left;
}

template <typename T, typename U, size_t N>
auto operator*=(BroadcastArray<T, N>& left,
                const U& right) -> BroadcastArray<T, N>& {
  for (auto& value : left) {
    value *= right;
  }
  return left;
}

template <typename T, typename U, size_t N>
constexpr auto operator*(const BroadcastArray<T, N>& left,
                         const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() * std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, r, res] : std::views::zip(left, right, result)) {
    res = l * r;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator*(const BroadcastArray<T, N>& left, const U& right) {
  using V = decltype(std::declval<T>() * std::declval<U>());
  BroadcastArray<V, N> result;
  for (int i = 0; i < N; ++i) {
    result[i] = left[i] * right;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator*(const T& left, const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() * std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [r, res] : std::views::zip(right, result)) {
    res = left * r;
  }
  return result;
}

template <typename T, typename U, size_t N>
auto operator/=(BroadcastArray<T, N>& left,
                const BroadcastArray<U, N>& right) -> BroadcastArray<T, N>& {
  for (auto [l, r] : std::views::zip(left, right)) {
    l /= r;
  }
  return left;
}

template <typename T, typename U, size_t N>
auto operator/=(BroadcastArray<T, N>& left,
                const U& right) -> BroadcastArray<T, N>& {
  for (auto& value : left) {
    value /= right;
  }
  return left;
}

template <typename T, typename U, size_t N>
constexpr auto operator/(const BroadcastArray<T, N>& left,
               const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() / std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, r, res] : std::views::zip(left, right, result)) {
    res = l / r;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator/(const BroadcastArray<T, N>& left, const U& right) {
  using V = decltype(std::declval<T>() / std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [l, res] : std::views::zip(left, result)) {
    res = l / right;
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto operator/(const T& left, const BroadcastArray<U, N>& right) {
  using V = decltype(std::declval<T>() / std::declval<U>());
  BroadcastArray<V, N> result;
  for (auto [r, res] : std::views::zip(right, result)) {
    res = left / r;
  }
  return result;
}

template <typename T, size_t N>
constexpr auto operator-(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    res = -a;
  }
  return result;
}

template <typename T, size_t N>
constexpr auto operator+(const BroadcastArray<T, N>& array) {
  return array;
}

template <typename T, typename U, size_t N>
constexpr auto pow(const BroadcastArray<T, N>& base,
                   const BroadcastArray<U, N>& exponent) {
  BroadcastArray<T, N> result;
  for (auto [b, e, res] : std::views::zip(base, exponent, result)) {
    using std::pow;
    res = pow(b, e);
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto pow(const BroadcastArray<T, N>& base, const U& exponent) {
  BroadcastArray<T, N> result;
  for (auto [b, res] : std::views::zip(base, result)) {
    using std::pow;
    res = pow(b, exponent);
  }
  return result;
}

template <typename T, typename U, size_t N>
constexpr auto pow(const T& base, const BroadcastArray<U, N>& exponent) {
  BroadcastArray<T, N> result;
  for (auto [e, res] : std::views::zip(exponent, result)) {
    using std::pow;
    res = pow(base, e);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto exp(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::exp;
    res = exp(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto log(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::log;
    res = log(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto sqrt(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::sqrt;
    res = sqrt(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto sin(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::sin;
    res = sin(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto cos(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::cos;
    res = cos(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto tan(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::tan;
    res = tan(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto asin(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::asin;
    res = asin(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto acos(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::acos;
    res = acos(a);
  }
  return result;
}

template <typename T, size_t N>
constexpr auto atan(const BroadcastArray<T, N>& array) {
  BroadcastArray<T, N> result;
  for (auto [a, res] : std::views::zip(array, result)) {
    using std::atan;
    res = atan(a);
  }
  return result;
}

}  // namespace tempura
