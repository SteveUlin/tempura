#pragma once

#include <cmath>
#include <ostream>

#include "function_traits.h"

namespace tempura::autodiff {

// https://en.wikipedia.org/wiki/Dual_number
//
// Dual number have the form: a + bε with ε² = 0
//
// The fun thing about dual numbers is that if you tailor series expand
// a function f(x), you get f(a + bε) = f(a) + f'(a)bε.
//
// In this way, we can use dual numbers to calculate the value of a function
// and its derivative at the same time. Sometimes called the forward
// differentiation.

template <typename T, typename G = T>
struct Dual {
  Dual() = default;
  Dual(const auto& value) : value(value), gradient(0) {}
  Dual(const auto& value, const G& gradient)
      : value(value), gradient(gradient) {}

  T value;
  G gradient;

  auto operator<=>(const Dual<T, G>&) const = default;
};
template <typename T>
Dual(T) -> Dual<T>;

template <typename T, typename G>
Dual(T, G) -> Dual<T, G>;

template <typename T>
struct IsDual : std::false_type {};
template <typename T, typename G>
struct IsDual<Dual<T, G>> : std::true_type {};

// Evaluate a function by setting all Dual gradients to zero except the
// provided index, which is set to one.
template <size_t N, typename F, typename... Inputs>
auto evalWrt(const F& func, Inputs&&... inputs) {
  auto map_arg_impl = [&]<size_t index>(auto&& arg) {
    using ArgT =
        std::remove_cvref_t<typename FunctionTraits<F>::template ArgT<index>>;
    static_assert(N != index or IsDual<ArgT>::value,
                  "You can only eval with respect to a Dual number.");
    if constexpr (IsDual<ArgT>::value) {
      return ArgT{.value = arg, .gradient = (index == N ? 1 : 0)};
    } else {
      return std::forward<decltype(arg)>(arg);
    }
  };

  return [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
    return func(
        map_arg_impl.template operator()<I>(std::forward<Inputs>(inputs))...);
  }(std::make_index_sequence<sizeof...(Inputs)>{});
}

// template <typename F, typename... Inputs>
// auto jacobian(const F& func, Inputs&&... inputs) {
//   return [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
//     return std::array{evalWrt<I>(func, inputs...)...};
//   }(std::make_index_sequence<sizeof...(Inputs)>{});
// }

// -- Relational Operators --

// For calculations, we typically only work with the value of the dual number
// not the derivative

// --- Arithmetic Operators ---

template <typename T, typename G>
constexpr auto operator+=(Dual<T, G>& lhs,
                          const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.gradient += rhs.gradient;
  lhs.value += rhs.value;
  return lhs;
}

template <typename T, typename G>
constexpr auto operator+(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  lhs += rhs;
  return lhs;
}

template <typename T, typename G>
constexpr auto operator+(const Dual<T, G>& dual) -> Dual<T, G> {
  return dual;
}

template <typename T, typename G>
constexpr auto operator-=(Dual<T, G>& lhs,
                          const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.gradient -= rhs.gradient;
  lhs.value -= rhs.value;
  return lhs;
}

template <typename T, typename G>
constexpr auto operator-(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  lhs -= rhs;
  return lhs;
}

template <typename T, typename G, typename U>
constexpr auto operator-(const Dual<T, G>& lhs, const U& rhs) {
  return lhs - Dual<U, G>{rhs};
}

template <typename T, typename G, typename U, typename H>
constexpr auto operator-(const Dual<T, G>& lhs, const Dual<U, H>& rhs) {
  return Dual{lhs.value - rhs.value, lhs.gradient - rhs.gradient};
}

template <typename T, typename G>
constexpr auto operator-(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient = -dual.gradient;
  dual.value = -dual.value;
  return dual;
}

template <typename T, typename G>
constexpr auto operator*=(Dual<T, G>& lhs,
                          const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.gradient = lhs.gradient * rhs.value + lhs.value * rhs.gradient;
  lhs.value *= rhs.value;
  return lhs;
}

template <typename T, typename U, typename G>
constexpr auto operator*(T lhs, const Dual<U, G>& rhs) -> Dual<T, G> {
  return Dual{lhs * rhs.value, lhs * rhs.gradient};
}

template <typename T, typename G>
constexpr auto operator*(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  lhs *= rhs;
  return lhs;
}

template <typename T, typename G>
constexpr auto operator/=(Dual<T, G>& lhs,
                          const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.gradient = (lhs.gradient * rhs.value - lhs.value * rhs.gradient) /
                 (rhs.value * rhs.value);
  lhs.value /= rhs.value;
  return lhs;
}

template <typename T, typename G>
constexpr auto operator/(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  lhs /= rhs;
  return lhs;
}

// -- Power Functions --

template <typename T, typename G>
auto sqrt(Dual<T, G> dual) -> Dual<T, G> {
  using std::sqrt;
  dual.gradient /= 2 * sqrt(dual.value);
  dual.value = sqrt(dual.value);
  return dual;
}

template <typename T, typename G>
auto exp(Dual<T, G> dual) -> Dual<T, G> {
  using std::exp;
  dual.gradient *= exp(dual.value);
  dual.value = exp(dual.value);
  return dual;
}

template <typename T, typename G>
auto log(Dual<T, G> dual) -> Dual<T, G> {
  using std::log;
  dual.gradient /= dual.value;
  dual.value = log(dual.value);
  return dual;
}

template <typename T, typename G>
auto pow(Dual<T, G> dual, const T& exponent) -> Dual<T, G> {
  using std::pow;
  dual.gradient *= exponent * pow(dual.value, exponent - 1);
  dual.value = pow(dual.value, exponent);
  return dual;
}

template <typename T, typename G>
auto pow(Dual<T, G> dual, const Dual<T, G>& exponent) -> Dual<T, G> {
  // https://math.stackexchange.com/questions/1914591/dual-number-ab-varepsilon-raised-to-a-dual-power-e-g-ab-varepsilon
  using std::log;
  using std::pow;
  const auto& [a, b] = dual;
  const auto& [c, d] = exponent;
  dual.gradient = c * pow(a, c - 1) * b + log(a) * pow(a, c) * d;
  dual.value = pow(a, c);
  return dual;
}

// -- Trigonometric Functions --

template <typename T, typename G>
auto sin(Dual<T, G> dual) -> Dual<T, G> {
  using std::cos;
  using std::sin;
  dual.gradient *= cos(dual.value);
  dual.value = sin(dual.value);
  return dual;
}

template <typename T, typename G>
auto cos(Dual<T, G> dual) -> Dual<T, G> {
  using std::cos;
  using std::sin;
  dual.gradient *= -sin(dual.value);
  dual.value = cos(dual.value);
  return dual;
}

template <typename T, typename G>
auto tan(Dual<T, G> dual) -> Dual<T, G> {
  using std::cos;
  using std::tan;
  dual.gradient *= 1 / (cos(dual.value) * cos(dual.value));
  dual.value = tan(dual.value);
  return dual;
}

template <typename T, typename G>
auto asin(Dual<T, G> dual) -> Dual<T, G> {
  using std::asin;
  using std::sqrt;
  dual.gradient *= 1 / sqrt(1 - dual.value * dual.value);
  dual.value = asin(dual.value);
  return dual;
}

template <typename T, typename G>
auto acos(Dual<T, G> dual) -> Dual<T, G> {
  using std::acos;
  using std::sqrt;
  dual.gradient *= -1 / sqrt(1 - dual.value * dual.value);
  dual.value = acos(dual.value);
  return dual;
}

template <typename T, typename G>
auto atan(Dual<T, G> dual) -> Dual<T, G> {
  using std::atan;
  dual.gradient *= 1 / (1 + dual.value * dual.value);
  dual.value = atan(dual.value);
  return dual;
}

};  // namespace tempura::autodiff

namespace std {
template <typename T, typename G>
auto operator<<(std::ostream& os,
                const tempura::autodiff::Dual<T, G>& dual) -> std::ostream& {
  os << dual.value << " + " << dual.gradient << "ε";
  return os;
}
};  // namespace std
