#pragma once

#include <cmath>
#include <ostream>

#include "function_traits.h"

namespace tempura {

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

// TODO: Support objects where the derivative is not the same type as
// the value.
template <typename T>
struct Dual {
  T value;
  T gradient;

  auto operator<=>(const Dual<T>&) const = default;
};

template <typename T>
struct IsDual : std::false_type {};
template <typename T>
struct IsDual<Dual<T>> : std::true_type {};

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

template <typename F, typename... Inputs>
auto jacobian(const F& func, Inputs&&... inputs) {
  return [&]<size_t... I>(std::index_sequence<I...> /*unused*/) {
    return std::array{evalWrt<I>(func, inputs...)...};
  }(std::make_index_sequence<sizeof...(Inputs)>{});
}

// -- Relational Operators --

// For calculations, we typically only work with the value of the dual number
// not the derivative

// --- Arithmetic Operators ---

template <typename T>
constexpr auto operator+=(Dual<T>& lhs, const Dual<T>& rhs) -> Dual<T>& {
  lhs.gradient += rhs.gradient;
  lhs.value += rhs.value;
  return lhs;
}

template <typename T>
constexpr auto operator+(Dual<T> lhs, const Dual<T>& rhs) -> Dual<T> {
  lhs += rhs;
  return lhs;
}

template <typename T>
constexpr auto operator+(const Dual<T>& dual) -> Dual<T> {
  return dual;
}

template <typename T>
constexpr auto operator-=(Dual<T>& lhs, const Dual<T>& rhs) -> Dual<T>& {
  lhs.gradient -= rhs.gradient;
  lhs.value -= rhs.value;
  return lhs;
}

template <typename T>
constexpr auto operator-(Dual<T> lhs, const Dual<T>& rhs) -> Dual<T> {
  lhs -= rhs;
  return lhs;
}

template <typename T>
constexpr auto operator-(Dual<T> dual) -> Dual<T> {
  dual.gradient = -dual.gradient;
  dual.value = -dual.value;
  return dual;
}

template <typename T>
constexpr auto operator*=(Dual<T>& lhs, const Dual<T>& rhs) -> Dual<T>& {
  lhs.gradient = lhs.gradient * rhs.value + lhs.value * rhs.gradient;
  lhs.value *= rhs.value;
  return lhs;
}

template <typename T>
constexpr auto operator*(Dual<T> lhs, const Dual<T>& rhs) -> Dual<T> {
  lhs *= rhs;
  return lhs;
}

template <typename T>
constexpr auto operator/=(Dual<T>& lhs, const Dual<T>& rhs) -> Dual<T>& {
  lhs.gradient = (lhs.gradient * rhs.value - lhs.value * rhs.gradient) /
                 (rhs.value * rhs.value);
  lhs.value /= rhs.value;
  return lhs;
}

template <typename T>
constexpr auto operator/(Dual<T> lhs, const Dual<T>& rhs) -> Dual<T> {
  lhs /= rhs;
  return lhs;
}

// -- Power Functions --

template <typename T>
auto sqrt(Dual<T> dual) -> Dual<T> {
  dual.gradient /= 2 * std::sqrt(dual.value);
  dual.value = std::sqrt(dual.value);
  return dual;
}

template <typename T>
auto exp(Dual<T> dual) -> Dual<T> {
  dual.gradient *= std::exp(dual.value);
  dual.value = std::exp(dual.value);
  return dual;
}

template <typename T>
auto log(Dual<T> dual) -> Dual<T> {
  dual.gradient /= dual.value;
  dual.value = std::log(dual.value);
  return dual;
}

template <typename T>
auto pow(Dual<T> dual, const T& exponent) -> Dual<T> {
  dual.gradient *= exponent * std::pow(dual.value, exponent - 1);
  dual.value = std::pow(dual.value, exponent);
  return dual;
}

template <typename T>
auto pow(Dual<T> dual, const Dual<T>& exponent) -> Dual<T> {
  // https://math.stackexchange.com/questions/1914591/dual-number-ab-varepsilon-raised-to-a-dual-power-e-g-ab-varepsilon
  const auto& [a, b] = dual;
  const auto& [c, d] = exponent;
  dual.gradient = c * std::pow(a, c - 1) * b + std::log(a) * std::pow(a, c) * d;
  dual.value = std::pow(a, c);
  return dual;
}

// -- Trigonometric Functions --

template <typename T>
auto sin(Dual<T> dual) -> Dual<T> {
  dual.gradient *= std::cos(dual.value);
  dual.value = std::sin(dual.value);
  return dual;
}

template <typename T>
auto cos(Dual<T> dual) -> Dual<T> {
  dual.gradient *= -std::sin(dual.value);
  dual.value = std::cos(dual.value);
  return dual;
}

template <typename T>
auto tan(Dual<T> dual) -> Dual<T> {
  dual.gradient *= 1 / (std::cos(dual.value) * std::cos(dual.value));
  dual.value = std::tan(dual.value);
  return dual;
}

template <typename T>
auto asin(Dual<T> dual) -> Dual<T> {
  dual.gradient *= 1 / std::sqrt(1 - dual.value * dual.value);
  dual.value = std::asin(dual.value);
  return dual;
}

template <typename T>
auto acos(Dual<T> dual) -> Dual<T> {
  dual.gradient *= -1 / std::sqrt(1 - dual.value * dual.value);
  dual.value = std::acos(dual.value);
  return dual;
}

template <typename T>
auto atan(Dual<T> dual) -> Dual<T> {
  dual.gradient *= 1 / (1 + dual.value * dual.value);
  dual.value = std::atan(dual.value);
  return dual;
}

};  // namespace tempura

namespace std {
template <typename T>
auto operator<<(std::ostream& os,
                const tempura::Dual<T>& dual) -> std::ostream& {
  os << dual.value << " + " << dual.gradient << "ε";
  return os;
}
};  // namespace std
