#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>

namespace tempura::autodiff {

// Higher-order forward AD via truncated Taylor series (a "jet"). A Jet<T,N> holds the N+1
// COEFFICIENTS cₖ of f(x₀ + t) ≈ Σ cₖ tᵏ, with the convention cₖ = f⁽ᵏ⁾(x₀)/k!. That
// convention is the whole point: products become a plain Cauchy convolution (no binomial
// factors), so the recurrences stay clean and constexpr. Seed a variable as [x, 1, 0, …];
// after evaluating f, the k-th derivative is k!·cₖ (nthDerivative).
//
// Replaces taylor.h, which nested first-order AD (combinatorially redundant), mis-defined
// its binomial table, and had no #pragma once. Pure forward + value-semantic ⇒ constexpr.
template <typename T, std::size_t N>
struct Jet {
  std::array<T, N + 1> c{};  // c[k] = f⁽ᵏ⁾/k!
};

// A variable x: [x, 1, 0, …] so f(jet) yields the Taylor expansion of f at x.
template <typename T, std::size_t N>
constexpr auto jetVariable(const T& x) -> Jet<T, N> {
  Jet<T, N> j{};
  j.c[0] = x;
  if constexpr (N >= 1) j.c[1] = T{1};
  return j;
}
template <typename T, std::size_t N>
constexpr auto jetConstant(const T& x) -> Jet<T, N> {
  Jet<T, N> j{};
  j.c[0] = x;
  return j;
}

// The k-th derivative f⁽ᵏ⁾(x₀) = k!·cₖ.
template <typename T, std::size_t N>
constexpr auto nthDerivative(const Jet<T, N>& j, std::size_t k) -> T {
  assert(k <= N && "derivative order exceeds the jet's order");
  T fact{1};
  for (std::size_t i = 2; i <= k; ++i) fact *= static_cast<T>(i);
  return fact * j.c[k];
}

// ── arithmetic ──
template <typename T, std::size_t N>
constexpr auto operator+(const Jet<T, N>& a, const Jet<T, N>& b) -> Jet<T, N> {
  Jet<T, N> r{};
  for (std::size_t k = 0; k <= N; ++k) r.c[k] = a.c[k] + b.c[k];
  return r;
}
template <typename T, std::size_t N>
constexpr auto operator-(const Jet<T, N>& a, const Jet<T, N>& b) -> Jet<T, N> {
  Jet<T, N> r{};
  for (std::size_t k = 0; k <= N; ++k) r.c[k] = a.c[k] - b.c[k];
  return r;
}
template <typename T, std::size_t N>
constexpr auto operator-(const Jet<T, N>& a) -> Jet<T, N> {
  Jet<T, N> r{};
  for (std::size_t k = 0; k <= N; ++k) r.c[k] = -a.c[k];
  return r;
}
// product = Cauchy convolution (clean BECAUSE of the /k! convention)
template <typename T, std::size_t N>
constexpr auto operator*(const Jet<T, N>& a, const Jet<T, N>& b) -> Jet<T, N> {
  Jet<T, N> r{};
  for (std::size_t k = 0; k <= N; ++k) {
    T s{};
    for (std::size_t i = 0; i <= k; ++i) s += a.c[i] * b.c[k - i];
    r.c[k] = s;
  }
  return r;
}
// quotient: from a = b·r, cₖ = (aₖ − Σ_{i≥1} bᵢ r_{k−i}) / b₀
template <typename T, std::size_t N>
constexpr auto operator/(const Jet<T, N>& a, const Jet<T, N>& b) -> Jet<T, N> {
  assert(b.c[0] != T{} && "jet division by a zero constant term");
  Jet<T, N> r{};
  for (std::size_t k = 0; k <= N; ++k) {
    T s = a.c[k];
    for (std::size_t i = 1; i <= k; ++i) s -= b.c[i] * r.c[k - i];
    r.c[k] = s / b.c[0];
  }
  return r;
}

// scalar mixed (constant ⇒ only the c₀ term / a uniform scale)
template <typename T, std::size_t N>
constexpr auto operator+(Jet<T, N> a, const T& s) -> Jet<T, N> { a.c[0] += s; return a; }
template <typename T, std::size_t N>
constexpr auto operator+(const T& s, Jet<T, N> a) -> Jet<T, N> { a.c[0] += s; return a; }
template <typename T, std::size_t N>
constexpr auto operator-(Jet<T, N> a, const T& s) -> Jet<T, N> { a.c[0] -= s; return a; }
template <typename T, std::size_t N>
constexpr auto operator*(Jet<T, N> a, const T& s) -> Jet<T, N> {
  for (auto& ck : a.c) ck *= s;
  return a;
}
template <typename T, std::size_t N>
constexpr auto operator*(const T& s, Jet<T, N> a) -> Jet<T, N> {
  for (auto& ck : a.c) ck *= s;
  return a;
}

// ── elementary functions via Taylor recurrences ──
template <typename T, std::size_t N>
constexpr auto exp(const Jet<T, N>& u) -> Jet<T, N> {
  using std::exp;
  Jet<T, N> y{};
  y.c[0] = exp(u.c[0]);
  for (std::size_t k = 1; k <= N; ++k) {  // y' = u'y  ⇒  k·yₖ = Σ i·uᵢ·y_{k−i}
    T s{};
    for (std::size_t i = 1; i <= k; ++i) s += static_cast<T>(i) * u.c[i] * y.c[k - i];
    y.c[k] = s / static_cast<T>(k);
  }
  return y;
}
template <typename T, std::size_t N>
constexpr auto log(const Jet<T, N>& u) -> Jet<T, N> {
  using std::log;
  assert(u.c[0] > T{} && "log domain: constant term must be > 0");
  Jet<T, N> y{};
  y.c[0] = log(u.c[0]);
  for (std::size_t k = 1; k <= N; ++k) {  // u·y' = u'  ⇒  yₖ = (uₖ − (1/k)Σ_{j<k} j·yⱼ·u_{k−j})/u₀
    T s = u.c[k];
    for (std::size_t j = 1; j < k; ++j)
      s -= (static_cast<T>(j) / static_cast<T>(k)) * y.c[j] * u.c[k - j];
    y.c[k] = s / u.c[0];
  }
  return y;
}
template <typename T, std::size_t N>
constexpr auto sqrt(const Jet<T, N>& u) -> Jet<T, N> {
  using std::sqrt;
  assert(u.c[0] >= T{} && "sqrt domain: constant term must be ≥ 0");
  Jet<T, N> y{};
  y.c[0] = sqrt(u.c[0]);
  for (std::size_t k = 1; k <= N; ++k) {  // y² = u  ⇒  yₖ = (uₖ − Σ_{0<i<k} yᵢ y_{k−i})/(2y₀)
    T s = u.c[k];
    for (std::size_t i = 1; i < k; ++i) s -= y.c[i] * y.c[k - i];
    y.c[k] = s / (T{2} * y.c[0]);
  }
  return y;
}
// sin and cos share one lockstep recurrence (each appears in the other's derivative).
template <typename T, std::size_t N>
constexpr auto sinCos(const Jet<T, N>& u) -> std::pair<Jet<T, N>, Jet<T, N>> {
  using std::cos;
  using std::sin;
  Jet<T, N> s{};
  Jet<T, N> co{};
  s.c[0] = sin(u.c[0]);
  co.c[0] = cos(u.c[0]);
  for (std::size_t k = 1; k <= N; ++k) {  // s' = c·u', c' = −s·u'
    T ss{};
    T cc{};
    for (std::size_t i = 1; i <= k; ++i) {
      ss += static_cast<T>(i) * u.c[i] * co.c[k - i];
      cc += static_cast<T>(i) * u.c[i] * s.c[k - i];
    }
    s.c[k] = ss / static_cast<T>(k);
    co.c[k] = -cc / static_cast<T>(k);
  }
  return {s, co};
}
template <typename T, std::size_t N>
constexpr auto sin(const Jet<T, N>& u) -> Jet<T, N> { return sinCos(u).first; }
template <typename T, std::size_t N>
constexpr auto cos(const Jet<T, N>& u) -> Jet<T, N> { return sinCos(u).second; }
// uᵖ = exp(p·log u) — reuses the log/exp recurrences (requires u₀ > 0).
template <typename T, std::size_t N>
constexpr auto pow(const Jet<T, N>& u, const T& p) -> Jet<T, N> {
  return exp(p * log(u));
}

}  // namespace tempura::autodiff
