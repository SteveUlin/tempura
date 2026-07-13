#pragma once

#include <cmath>
#include <compare>
#include <concepts>
#include <numbers>
#include <type_traits>
#include <utility>

namespace tempura::autodiff {

// Forward-mode AD core.
//
// A dual number a + bε with ε² = 0: Taylor-expanding any f gives
// f(a + bε) = f(a) + f'(a)·b·ε, so one evaluation yields both the value and the
// directional derivative along the seed b.

template <typename T, typename G = T>
struct Dual {
  T value{};
  G gradient{};

  // Value-only: the gradient never steers control flow, so a differentiated
  // branch matches the primal's.
  friend constexpr auto operator<=>(const Dual& a, const Dual& b)
    requires std::three_way_comparable<T>
  {
    return a.value <=> b.value;
  }
  friend constexpr auto operator==(const Dual& a, const Dual& b) -> bool
    requires std::equality_comparable<T>
  {
    return a.value == b.value;
  }

  // The constructible_from gate keeps a lossy cross-type mix (Dual<float> vs
  // Dual<double>) from compiling.
  template <typename U>
    requires(std::constructible_from<T, const U&> &&
             requires(const T& t, const U& u) { t <=> u; })
  friend constexpr auto operator<=>(const Dual& a, const U& s) {
    return a.value <=> s;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&> &&
             requires(const T& t, const U& u) {
               { t == u } -> std::convertible_to<bool>;
             })
  friend constexpr auto operator==(const Dual& a, const U& s) -> bool {
    return a.value == s;
  }

  // Compound assignment holds the primary arithmetic; the binary operators derive.

  constexpr auto operator+=(const Dual& rhs) -> Dual& {
    value += rhs.value;
    gradient += rhs.gradient;
    return *this;
  }
  constexpr auto operator-=(const Dual& rhs) -> Dual& {
    value -= rhs.value;
    gradient -= rhs.gradient;
    return *this;
  }
  constexpr auto operator*=(const Dual& rhs) -> Dual& {
    G cross = value * rhs.gradient;  // read all old state before any write (x *= x safe)
    gradient *= rhs.value;
    gradient += cross;  // product rule: g·w + v·g′
    value *= rhs.value;
    return *this;
  }
  constexpr auto operator/=(const Dual& rhs) -> Dual& {
    // Quotient rule u′/w − (u/w²)·w′, folded /w/w so w² never forms (overflows near 1.3e154).
    G cross = (value / rhs.value / rhs.value) * rhs.gradient;
    gradient /= rhs.value;
    gradient -= cross;
    value /= rhs.value;
    return *this;
  }

  // Scalar-mixed forms. The scalars of Dual<T,G> are exactly what constructs into
  // T: an inner Dual lifts losslessly, a mismatched Dual (float vs double) fails loud.
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  constexpr auto operator+=(const U& s) -> Dual& {
    value += static_cast<T>(s);
    return *this;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  constexpr auto operator-=(const U& s) -> Dual& {
    value -= static_cast<T>(s);
    return *this;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  constexpr auto operator*=(const U& s) -> Dual& {
    const T c = static_cast<T>(s);
    value *= c;
    gradient *= c;
    return *this;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  constexpr auto operator/=(const U& s) -> Dual& {
    const T c = static_cast<T>(s);
    value /= c;
    gradient /= c;
    return *this;
  }

  // An rvalue dual operand donates its buffer. Two statements, never
  // `return a += b`: that returns an lvalue and copies; a bare `return a;` moves.

  friend constexpr auto operator+(Dual a, const Dual& b) -> Dual {
    a += b;
    return a;
  }
  friend constexpr auto operator+(const Dual& a, Dual&& b) -> Dual {
    b += a;
    return std::move(b);
  }
  friend constexpr auto operator-(Dual a, const Dual& b) -> Dual {
    a -= b;
    return a;
  }
  friend constexpr auto operator-(const Dual& a, Dual&& b) -> Dual {
    // −(b−a), not (−b)+a: the latter doubles the tangent for x − move(x); alias-safe.
    b.value = a.value - b.value;
    b.gradient -= a.gradient;
    b.gradient *= T{-1};
    return std::move(b);
  }
  friend constexpr auto operator*(Dual a, const Dual& b) -> Dual {
    a *= b;
    return a;
  }
  friend constexpr auto operator*(const Dual& a, Dual&& b) -> Dual {
    b *= a;
    return std::move(b);
  }
  friend constexpr auto operator/(Dual a, const Dual& b) -> Dual {
    a /= b;
    return a;
  }
  friend constexpr auto operator/(const Dual& a, Dual&& b) -> Dual {
    // Hoist a′/w before mutating (x / move(x) safe); fold /w/w so w² never forms.
    G t = a.gradient / b.value;
    b.gradient *= -(a.value / b.value / b.value);
    b.gradient += t;
    b.value = a.value / b.value;
    return std::move(b);
  }

  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator+(Dual a, const U& s) -> Dual {
    a += s;
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator+(const U& s, Dual a) -> Dual {
    a += s;
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator-(Dual a, const U& s) -> Dual {
    a -= s;
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator-(const U& s, Dual a) -> Dual {
    a.value = static_cast<T>(s) - a.value;
    a.gradient *= T{-1};
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator*(Dual a, const U& s) -> Dual {
    a *= s;
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator*(const U& s, Dual a) -> Dual {
    a *= s;
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator/(Dual a, const U& s) -> Dual {
    a /= s;
    return a;
  }
  template <typename U>
    requires(std::constructible_from<T, const U&>)
  friend constexpr auto operator/(const U& s, Dual a) -> Dual {
    const T q = static_cast<T>(s) / a.value;
    a.gradient *= -q / a.value;  // d/dx (c/x) = −c/x² = −(c/x)/x, no w² overflow
    a.value = q;
    return a;
  }

  friend constexpr auto operator+(const Dual& d) -> Dual { return d; }
  friend constexpr auto operator-(const Dual& d) -> Dual {
    return {.value = -d.value, .gradient = -d.gradient};
  }
};

template <typename T>
Dual(T) -> Dual<T>;
template <typename T, typename G>
Dual(T, G) -> Dual<T, G>;

// fma: the value keeps std::fma's single rounding.
template <typename T, typename G>
constexpr auto fma(const Dual<T, G>& a, const Dual<T, G>& b, Dual<T, G> c)
    -> Dual<T, G> {
  using std::fma;
  // Sink the addend by value: its tangent is the accumulator.
  c.gradient += a.gradient * b.value;
  c.gradient += a.value * b.gradient;
  c.value = fma(a.value, b.value, c.value);
  return c;
}
// Constant (zero-tangent) addend.
template <typename T, typename G, typename U>
  requires(std::constructible_from<T, const U&>)
constexpr auto fma(const Dual<T, G>& a, const Dual<T, G>& b, const U& c)
    -> Dual<T, G> {
  using std::fma;
  G g = a.gradient * b.value;
  g += a.value * b.gradient;
  return {.value = fma(a.value, b.value, static_cast<T>(c)),
          .gradient = std::move(g)};
}

// Elementary functions: chain rule on the value; a domain violation propagates
// as NaN through value and gradient.

template <typename T, typename G>
constexpr auto sqrt(const Dual<T, G>& d) -> Dual<T, G> {
  using std::sqrt;
  const T s = sqrt(d.value);
  return {.value = s, .gradient = d.gradient / (T{2} * s)};
}
template <typename T, typename G>
constexpr auto exp(const Dual<T, G>& d) -> Dual<T, G> {
  using std::exp;
  const T e = exp(d.value);
  return {.value = e, .gradient = d.gradient * e};
}
template <typename T, typename G>
constexpr auto log(const Dual<T, G>& d) -> Dual<T, G> {
  using std::log;
  return {.value = log(d.value), .gradient = d.gradient / d.value};
}
// pow, constant exponent: d/dx xⁿ = n·xⁿ⁻¹. Non-deduced n so pow(x, 2) binds to
// T instead of deducing int and clashing with T=double.
template <typename T, typename G>
constexpr auto pow(const Dual<T, G>& d, const std::type_identity_t<T>& n)
    -> Dual<T, G> {
  using std::pow;
  return {.value = pow(d.value, n),
          .gradient = d.gradient * (n * pow(d.value, n - T{1}))};
}
// pow, dual exponent: d(uᵛ) = uᵛ·(v′·ln u + v·u′/u).
template <typename T, typename G>
constexpr auto pow(const Dual<T, G>& base, const Dual<T, G>& expo)
    -> Dual<T, G> {
  using std::log;
  using std::pow;
  const T uv = pow(base.value, expo.value);
  const T c1 = uv * log(base.value);
  const T c2 = uv * expo.value / base.value;
  return {.value = uv, .gradient = c1 * expo.gradient + c2 * base.gradient};
}

template <typename T, typename G>
constexpr auto sin(const Dual<T, G>& d) -> Dual<T, G> {
  using std::cos;
  using std::sin;
  return {.value = sin(d.value), .gradient = d.gradient * cos(d.value)};
}
template <typename T, typename G>
constexpr auto cos(const Dual<T, G>& d) -> Dual<T, G> {
  using std::cos;
  using std::sin;
  return {.value = cos(d.value), .gradient = d.gradient * (-sin(d.value))};
}
template <typename T, typename G>
constexpr auto tan(const Dual<T, G>& d) -> Dual<T, G> {
  using std::cos;
  using std::tan;
  const T c = cos(d.value);
  return {.value = tan(d.value), .gradient = d.gradient / (c * c)};  // sec²
}
template <typename T, typename G>
constexpr auto asin(const Dual<T, G>& d) -> Dual<T, G> {
  using std::asin;
  using std::sqrt;
  return {.value = asin(d.value),
          .gradient = d.gradient / sqrt(T{1} - d.value * d.value)};
}
template <typename T, typename G>
constexpr auto acos(const Dual<T, G>& d) -> Dual<T, G> {
  using std::acos;
  using std::sqrt;
  return {.value = acos(d.value),
          .gradient = d.gradient * (-T{1} / sqrt(T{1} - d.value * d.value))};
}
template <typename T, typename G>
constexpr auto atan(const Dual<T, G>& d) -> Dual<T, G> {
  using std::atan;
  return {.value = atan(d.value),
          .gradient = d.gradient / (T{1} + d.value * d.value)};
}

// Digamma ψ(x) = d/dx ln Γ(x): recurrence-shift to large x, then the Bernoulli
// asymptotic series.
template <std::floating_point T>
constexpr auto digamma(T x) -> T {
  if (x < T{0.5}) {  // reflection ψ(1−x) − ψ(x) = π·cot(πx)
    using std::tan;
    return digamma(T{1} - x) -
           std::numbers::pi_v<T> / tan(std::numbers::pi_v<T> * x);
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
auto lgamma(const Dual<T, G>& d)
    -> Dual<T, G> {  // not constexpr: std::lgamma isn't
  using std::lgamma;
  return {.value = lgamma(d.value), .gradient = d.gradient * digamma(d.value)};
}

}  // namespace tempura::autodiff
