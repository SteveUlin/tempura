#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <utility>

namespace tempura::autodiff {

// In a standard Taylor series expansion, you divide by the factorial of the
// order of the derivative:
// f(x) = f(a) + f'(a)(x - a) + f''(a)(x - a)²/2! + f'''(a)(x - a)³/3! + ...

// The general idea is to eval the nth derivative at "a" then spread that value
// out with polynomial expansion

// TODO: JAX has a paper on how to implement "faster?" methods
// https://openreview.net/pdf?id=SkxEF3FNPH

// Higher order chain rule:
// f(g(x))⁽ⁿ⁾ = [f'(g(x)) * g'(x)]⁽ⁿ⁻¹⁾ = ∑ (n-1)Ci * [f'(g(x))]⁽ⁱ⁾ * g⁽ⁿ⁻ⁱ⁾(x)
//
// The general strategy is to create an aux Taylor object to model [f'(g(x))]⁽ⁿ⁾
// and use that to calculate the nth derivative of the composition.
//
// Sometimes we can forget about the aux Taylor object and just calculate the
// derivative directly.

template <size_t i, size_t j>
inline constexpr int64_t kBinomialCoefficient = [] {
  if constexpr (i == 0 or j == 0) {
    return 1;
  } else {
    return kBinomialCoefficient<i - 1, j> + kBinomialCoefficient<i - 1, j - 1>;
  }
}();

// Compile time loops
template <int64_t... Ints, typename F>
constexpr auto forSequence(std::integer_sequence<int64_t, Ints...> /*unused*/,
                           F f) -> void {
  (f(std::integral_constant<int64_t, Ints>{}), ...);
}

template <int64_t N, typename F>
constexpr auto forSequence(F f) -> void {
  forSequence(std::make_integer_sequence<decltype(N), N>{}, f);
}

template <int64_t S, int64_t E, typename F>
constexpr auto forSequence(F f) -> void {
  [&]<int64_t... Is>(std::integer_sequence<int64_t, Is...> /*unused*/) {
    (f(std::integral_constant<int64_t, S + Is>{}), ...);
  }(std::make_integer_sequence<decltype(E - S), E - S>{});
}

template <typename T, std::size_t N>
using Taylor = std::array<T, N>;

template <typename T, std::size_t N>
constexpr auto operator+=(Taylor<T, N>& lhs,
                          const Taylor<T, N>& rhs) -> Taylor<T, N>& {
  for (std::size_t i = 0; i < N; ++i) {
    lhs[i] += rhs[i];
  }
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator+(Taylor<T, N> lhs,
                         const Taylor<T, N>& rhs) -> Taylor<T, N> {
  lhs += rhs;
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator+(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  return taylor;
}

template <typename T, std::size_t N>
constexpr auto operator-=(Taylor<T, N>& lhs,
                          const Taylor<T, N>& rhs) -> Taylor<T, N>& {
  forSequence<N>([&](auto i) constexpr { lhs[i] -= rhs[i]; });
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator-(Taylor<T, N> lhs,
                         const Taylor<T, N>& rhs) -> Taylor<T, N> {
  lhs -= rhs;
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator-(Taylor<T, N> taylor) -> Taylor<T, N> {
  for (std::size_t i = 0; i < N; ++i) {
    taylor[i] = -taylor[i];
  }
  return taylor;
}

template <typename T, std::size_t N>
constexpr auto operator*=(Taylor<T, N>& lhs,
                          const Taylor<T, N>& rhs) -> Taylor<T, N>& {
  forSequence<N>([&](auto i) constexpr {
    constexpr auto index = N - 1 - i;
    lhs[index] *= rhs[0];
    forSequence<index>([&](auto j) {
      lhs[index] +=
          kBinomialCoefficient<index, j> * lhs[index - 1 - j] * rhs[j];
    });
  });
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator*(Taylor<T, N> lhs,
                         const Taylor<T, N>& rhs) -> Taylor<T, N> {
  lhs *= rhs;
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator/=(Taylor<T, N>& lhs,
                          const Taylor<T, N>& rhs) -> Taylor<T, N>& {
  forSequence<N>([&](auto i) constexpr {
    forSequence<i>([&](auto j) constexpr {
      lhs[i] -= kBinomialCoefficient<j, i - j> * lhs[j] * rhs[i - j];
    });
    lhs[i] /= rhs[0];
  });
  return lhs;
}

template <typename T, std::size_t N>
constexpr auto operator/(Taylor<T, N> lhs,
                         const Taylor<T, N>& rhs) -> Taylor<T, N> {
  lhs /= rhs;
  return lhs;
}

// -- Power Functions --

template <typename T, std::size_t N>
constexpr auto exp(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  Taylor<T, N> result = {};
  using std::exp;
  result[0] = exp(taylor[0]);
  forSequence<1, N>([&](auto i) constexpr {
    forSequence<i>([&](auto j) constexpr {
      constexpr auto c = kBinomialCoefficient<i - 1, j>;
      result[i] += c * taylor[i - j] * result[j];
    });
  });
  return result;
}

template <typename T, std::size_t N>
constexpr auto log(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  Taylor<T, N> result = {};
  using std::log;
  result[0] = log(taylor[0]);
  forSequence<1, N>([&](auto i) constexpr {
    result[i] = taylor[i];
    forSequence<1, i>([&](auto j) constexpr {
      result[i] -=
          kBinomialCoefficient<i - 1, j - 1> * taylor[j] * result[i - j];
    });
    result[i] /= taylor[0];
  });
  return result;
}

template <typename T, std::size_t N>
constexpr auto pow(const Taylor<T, N>& taylor,
                   const Taylor<T, N>& exponent) -> Taylor<T, N> {
  return exp(exponent * log(taylor));
}

template <typename T, std::size_t N>
constexpr auto sqrt(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  return pow(taylor, Taylor<T, N>{0.5});
}

// -- Trig Functions --

template <typename T, std::size_t N>
auto sincos(const Taylor<T, N>& taylor)
    -> std::pair<Taylor<T, N>, Taylor<T, N>> {
  Taylor<T, N> sin_result = {};
  Taylor<T, N> cos_result = {};
  using std::cos;
  using std::sin;
  sin_result[0] = sin(taylor[0]);
  cos_result[0] = cos(taylor[0]);
  forSequence<1, N>([&](auto i) constexpr {
    forSequence<i>([&](auto j) constexpr {
      sin_result[i] =
          kBinomialCoefficient<i - 1, j> * taylor[j] * cos_result[i - j];
      cos_result[i] -=
          kBinomialCoefficient<i - 1, j> * taylor[j] * sin_result[i - j];
    });
  });
  return {sin_result, cos_result};
}

template <typename T, std::size_t N>
auto sin(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  return sincos(taylor).first;
}

template <typename T, std::size_t N>
auto cos(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  return sincos(taylor).second;
}

template <typename T, std::size_t N>
auto tan(const Taylor<T, N>& taylor) -> Taylor<T, N> {
  Taylor<T, N> result = {};
  using std::tan;
  result[0] = tan(taylor[0]);
  if constexpr (N > 1) {
    Taylor<T, N> aux = {};
    aux[0] = 1 + result[0] * result[0];
    forSequence<1, N>([&](auto i) constexpr {
      forSequence<i>([&](auto j) constexpr {
        constexpr auto c = kBinomialCoefficient<i - 1, j>;
        result[i] += c * aux[j] * result[i - j];
        aux[i] += c * result[j] * result[i - j];
      });
      aux[i] *= 2;
    });
  }
  return result;
}

};  // namespace tempura::autodiff

namespace std {
template <typename T, std::size_t N>
auto operator<<(std::ostream& os, const tempura::autodiff::Taylor<T, N>& taylor)
    -> std::ostream& {
  os << "Taylor: [";
  for (const auto& value : taylor) {
    os << value << ", ";
  }
  os << "]";
  return os;
}

}  // namespace std
