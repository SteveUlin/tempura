#pragma once

#include <cassert>
#include <cmath>
#include <compare>
#include <concepts>
#include <numbers>
#include <type_traits>

namespace tempura::autodiff {

// Forward-mode AD core (the JVP primitive). A dual number a + bε with ε² = 0: Taylor-
// expanding any f gives f(a + bε) = f(a) + f'(a)·b·ε, so ONE evaluation yields both the
// value and the directional derivative along the seed b.
//
// T is the value scalar; G is the TANGENT carry — and G need NOT equal T. G = T seeds one
// direction; G = a vector seeds K directions at once (the basis of jacfwd); G = Dual gives
// second order (forward-over-forward). The arithmetic only asks that G be a module over T
// (addable to itself, scalable by T), so it stays generic without a concept for now.
template <typename T, typename G = T>
struct Dual {
  T value{};
  G gradient{};

  constexpr Dual() = default;
  constexpr Dual(const T& value) : value{value}, gradient{} {}
  constexpr Dual(const T& value, const G& gradient) : value{value}, gradient{gradient} {}

  // Order by VALUE ONLY. A defaulted spaceship would fold the tangent into the order, so
  // two duals with equal value but different derivative would compare unequal — and a
  // branch like `if (x < y)` inside a differentiated function could then depend on a
  // derivative. A dual must compare like its real part.
  constexpr auto operator<=>(const Dual& o) const { return value <=> o.value; }
  constexpr auto operator==(const Dual& o) const -> bool { return value == o.value; }
};

template <typename T>
Dual(T) -> Dual<T>;
template <typename T, typename G>
Dual(T, G) -> Dual<T, G>;

template <typename T>
struct IsDual : std::false_type {};
template <typename T, typename G>
struct IsDual<Dual<T, G>> : std::true_type {};
template <typename T>
inline constexpr bool kIsDual = IsDual<std::remove_cvref_t<T>>::value;

// ── Arithmetic: dual ⊕ dual (same type) ──

template <typename T, typename G>
constexpr auto operator+=(Dual<T, G>& lhs, const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.value += rhs.value;
  lhs.gradient += rhs.gradient;
  return lhs;
}
template <typename T, typename G>
constexpr auto operator+(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  return lhs += rhs;
}
template <typename T, typename G>
constexpr auto operator+(const Dual<T, G>& d) -> Dual<T, G> {
  return d;
}

template <typename T, typename G>
constexpr auto operator-=(Dual<T, G>& lhs, const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.value -= rhs.value;
  lhs.gradient -= rhs.gradient;
  return lhs;
}
template <typename T, typename G>
constexpr auto operator-(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  return lhs -= rhs;
}
template <typename T, typename G>
constexpr auto operator-(Dual<T, G> d) -> Dual<T, G> {
  d.value = -d.value;
  d.gradient = -d.gradient;
  return d;
}

template <typename T, typename G>
constexpr auto operator*=(Dual<T, G>& lhs, const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.gradient = lhs.gradient * rhs.value + lhs.value * rhs.gradient;  // product rule
  lhs.value *= rhs.value;
  return lhs;
}
template <typename T, typename G>
constexpr auto operator*(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  return lhs *= rhs;
}

template <typename T, typename G>
constexpr auto operator/=(Dual<T, G>& lhs, const Dual<T, G>& rhs) -> Dual<T, G>& {
  lhs.gradient = (lhs.gradient * rhs.value - lhs.value * rhs.gradient) /  // quotient rule
                 (rhs.value * rhs.value);
  lhs.value /= rhs.value;
  return lhs;
}
template <typename T, typename G>
constexpr auto operator/(Dual<T, G> lhs, const Dual<T, G>& rhs) -> Dual<T, G> {
  return lhs /= rhs;
}

// ── Arithmetic: dual ⊕ scalar (the scalar is a constant — zero tangent) ──
// The result keeps the DUAL's value type T; the scalar is cast into it. The previous
// `operator*(T, Dual<U,G>) -> Dual<T,G>` deduced the result's T from the SCALAR, so
// `2 * Dual<double>` became Dual<int> and silently truncated the value. Hence U is a
// separate, non-dual parameter and the dual's T always wins.

template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator+(const Dual<T, G>& d, const U& s) -> Dual<T, G> {
  return {d.value + static_cast<T>(s), d.gradient};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator+(const U& s, const Dual<T, G>& d) -> Dual<T, G> {
  return {static_cast<T>(s) + d.value, d.gradient};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator-(const Dual<T, G>& d, const U& s) -> Dual<T, G> {
  return {d.value - static_cast<T>(s), d.gradient};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator-(const U& s, const Dual<T, G>& d) -> Dual<T, G> {
  return {static_cast<T>(s) - d.value, -d.gradient};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator*(const Dual<T, G>& d, const U& s) -> Dual<T, G> {
  return {d.value * static_cast<T>(s), d.gradient * static_cast<T>(s)};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator*(const U& s, const Dual<T, G>& d) -> Dual<T, G> {
  return {static_cast<T>(s) * d.value, static_cast<T>(s) * d.gradient};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator/(const Dual<T, G>& d, const U& s) -> Dual<T, G> {
  return {d.value / static_cast<T>(s), d.gradient / static_cast<T>(s)};
}
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto operator/(const U& s, const Dual<T, G>& d) -> Dual<T, G> {
  const T c = static_cast<T>(s);
  return {c / d.value, -c * d.gradient / (d.value * d.value)};  // d/dx (c/x) = −c/x²
}

// ── fma: the fused primitive. Value keeps a single rounding (std::fma); the tangent follows
// the product+sum rule via the module ops, so it stays generic in G — matching operator*. ──
template <typename T, typename G>
constexpr auto fma(const Dual<T, G>& a, const Dual<T, G>& b, const Dual<T, G>& c) -> Dual<T, G> {
  using std::fma;
  return {fma(a.value, b.value, c.value),
          a.gradient * b.value + a.value * b.gradient + c.gradient};
}
// Constant (zero-tangent) addend — the coefficient in a Horner step.
template <typename T, typename G, typename U>
  requires(!kIsDual<U>)
constexpr auto fma(const Dual<T, G>& a, const Dual<T, G>& b, const U& c) -> Dual<T, G> {
  using std::fma;
  return {fma(a.value, b.value, static_cast<T>(c)),
          a.gradient * b.value + a.value * b.gradient};
}

// ── Elementary functions: chain rule on the value, fail loud on domain violations ──
// constexpr throughout (C++26 P0533/P1383 make <cmath> constexpr); found by ADL so a
// templated f(Dual) and f(double) write the same `using std::sin; sin(x)`.

template <typename T, typename G>
constexpr auto sqrt(Dual<T, G> d) -> Dual<T, G> {
  using std::sqrt;
  assert(d.value >= T{} && "sqrt domain: value must be ≥ 0");
  const T s = sqrt(d.value);
  d.gradient = d.gradient / (T{2} * s);
  d.value = s;
  return d;
}
template <typename T, typename G>
constexpr auto exp(Dual<T, G> d) -> Dual<T, G> {
  using std::exp;
  const T e = exp(d.value);
  d.gradient = d.gradient * e;
  d.value = e;
  return d;
}
template <typename T, typename G>
constexpr auto log(Dual<T, G> d) -> Dual<T, G> {
  using std::log;
  assert(d.value > T{} && "log domain: value must be > 0");
  d.gradient = d.gradient / d.value;
  d.value = log(d.value);
  return d;
}
// pow with a constant exponent: d/dx xⁿ = n·xⁿ⁻¹.
template <typename T, typename G>
constexpr auto pow(Dual<T, G> d, const T& n) -> Dual<T, G> {
  using std::pow;
  d.gradient = d.gradient * (n * pow(d.value, n - T{1}));
  d.value = pow(d.value, n);
  return d;
}
// pow with a dual exponent: d(uᵛ) = uᵛ·(v'·ln u + v·u'/u).
template <typename T, typename G>
constexpr auto pow(const Dual<T, G>& base, const Dual<T, G>& expo) -> Dual<T, G> {
  using std::log;
  using std::pow;
  assert(base.value > T{} && "pow domain: base must be > 0 for a dual exponent");
  const T uv = pow(base.value, expo.value);
  return {uv, uv * (expo.gradient * log(base.value) +
                    expo.value * base.gradient / base.value)};
}

template <typename T, typename G>
constexpr auto sin(Dual<T, G> d) -> Dual<T, G> {
  using std::cos;
  using std::sin;
  d.gradient = d.gradient * cos(d.value);
  d.value = sin(d.value);
  return d;
}
template <typename T, typename G>
constexpr auto cos(Dual<T, G> d) -> Dual<T, G> {
  using std::cos;
  using std::sin;
  d.gradient = d.gradient * (-sin(d.value));
  d.value = cos(d.value);
  return d;
}
template <typename T, typename G>
constexpr auto tan(Dual<T, G> d) -> Dual<T, G> {
  using std::cos;
  using std::tan;
  const T c = cos(d.value);
  d.gradient = d.gradient / (c * c);  // sec²
  d.value = tan(d.value);
  return d;
}
template <typename T, typename G>
constexpr auto asin(Dual<T, G> d) -> Dual<T, G> {
  using std::asin;
  using std::sqrt;
  assert(d.value * d.value <= T{1} && "asin domain: |value| must be ≤ 1");
  d.gradient = d.gradient / sqrt(T{1} - d.value * d.value);
  d.value = asin(d.value);
  return d;
}
template <typename T, typename G>
constexpr auto acos(Dual<T, G> d) -> Dual<T, G> {
  using std::acos;
  using std::sqrt;
  assert(d.value * d.value <= T{1} && "acos domain: |value| must be ≤ 1");
  d.gradient = d.gradient * (-T{1} / sqrt(T{1} - d.value * d.value));
  d.value = acos(d.value);
  return d;
}
template <typename T, typename G>
constexpr auto atan(Dual<T, G> d) -> Dual<T, G> {
  using std::atan;
  d.gradient = d.gradient / (T{1} + d.value * d.value);
  d.value = atan(d.value);
  return d;
}

// Digamma ψ(x) = d/dx ln Γ(x): recurrence-shift to large x, then the Bernoulli asymptotic
// series. The derivative rule for lgamma below.
template <std::floating_point T>
constexpr auto digamma(T x) -> T {
  if (x < T{0.5}) {  // reflection ψ(1−x) − ψ(x) = π·cot(πx)
    using std::tan;
    return digamma(T{1} - x) - std::numbers::pi_v<T> / tan(std::numbers::pi_v<T> * x);
  }
  T result{};
  while (x < T{7}) {  // ψ(x+1) = ψ(x) + 1/x
    result -= T{1} / x;
    x += T{1};
  }
  using std::log;
  const T inv = T{1} / x;
  const T inv2 = inv * inv;
  result += log(x);
  result -= inv / T{2};
  result -= inv2 / T{12};
  result += inv2 * inv2 / T{120};
  result -= inv2 * inv2 * inv2 / T{252};
  result += inv2 * inv2 * inv2 * inv2 / T{240};
  return result;
}
template <typename T, typename G>
auto lgamma(Dual<T, G> d) -> Dual<T, G> {  // not constexpr: std::lgamma isn't
  using std::lgamma;
  d.gradient = d.gradient * digamma(d.value);
  d.value = lgamma(d.value);
  return d;
}

}  // namespace tempura::autodiff
