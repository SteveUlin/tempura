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

// TODO: Support objects where the derivative is not the same type as
// the value.
template <typename T, typename G = T>
struct Dual {
  T value;
  G gradient;

  auto operator<=>(const Dual<T, G>&) const = default;
};

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
  dual.gradient /= 2 * std::sqrt(dual.value);
  dual.value = std::sqrt(dual.value);
  return dual;
}

template <typename T, typename G>
auto exp(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= std::exp(dual.value);
  dual.value = std::exp(dual.value);
  return dual;
}

template <typename T, typename G>
auto log(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient /= dual.value;
  dual.value = std::log(dual.value);
  return dual;
}

template <typename T, typename G>
auto pow(Dual<T, G> dual, const T& exponent) -> Dual<T, G> {
  dual.gradient *= exponent * std::pow(dual.value, exponent - 1);
  dual.value = std::pow(dual.value, exponent);
  return dual;
}

template <typename T, typename G>
auto pow(Dual<T, G> dual, const Dual<T, G>& exponent) -> Dual<T, G> {
  // https://math.stackexchange.com/questions/1914591/dual-number-ab-varepsilon-raised-to-a-dual-power-e-g-ab-varepsilon
  const auto& [a, b] = dual;
  const auto& [c, d] = exponent;
  dual.gradient = c * std::pow(a, c - 1) * b + std::log(a) * std::pow(a, c) * d;
  dual.value = std::pow(a, c);
  return dual;
}

// -- Trigonometric Functions --

template <typename T, typename G>
auto sin(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= std::cos(dual.value);
  dual.value = std::sin(dual.value);
  return dual;
}

template <typename T, typename G>
auto cos(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= -std::sin(dual.value);
  dual.value = std::cos(dual.value);
  return dual;
}

template <typename T, typename G>
auto tan(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= 1 / (std::cos(dual.value) * std::cos(dual.value));
  dual.value = std::tan(dual.value);
  return dual;
}

template <typename T, typename G>
auto asin(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= 1 / std::sqrt(1 - dual.value * dual.value);
  dual.value = std::asin(dual.value);
  return dual;
}

template <typename T, typename G>
auto acos(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= -1 / std::sqrt(1 - dual.value * dual.value);
  dual.value = std::acos(dual.value);
  return dual;
}

template <typename T, typename G>
auto atan(Dual<T, G> dual) -> Dual<T, G> {
  dual.gradient *= 1 / (1 + dual.value * dual.value);
  dual.value = std::atan(dual.value);
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
