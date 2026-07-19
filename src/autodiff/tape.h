#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include "weighted_dag.h"

namespace tempura::autodiff {

// v ← (I−L)⁻¹·v: forward substitution in node order; nodes gather from parents.
template <typename T>
auto forwardSubstitute(const WeightedDag<T>& dag, std::vector<T> v)
    -> std::vector<T> {
  assert(v.size() == dag.nodeCount() && "vector must cover every node");
  for (std::size_t i = 0; i < dag.nodeCount(); ++i) {
    for (const auto& [dep, weight] : dag.edges(i)) {
      assert(dep < i && "edge must point to an earlier node");
      v[i] += weight * v[dep];
    }
  }
  return v;
}

// v ← (I−L)⁻ᵀ·v: back-substitution in reverse order; nodes scatter to parents.
template <typename T>
auto backwardSubstitute(const WeightedDag<T>& dag, std::vector<T> v)
    -> std::vector<T> {
  assert(v.size() == dag.nodeCount() && "vector must cover every node");
  for (std::size_t i = dag.nodeCount(); i-- > 0;) {
    for (const auto& [dep, weight] : dag.edges(i)) {
      assert(dep < i && "edge must point to an earlier node");
      v[dep] += v[i] * weight;
    }
  }
  return v;
}

// A value handle into a recording: each operation appends one node whose edge
// weights are the local partials ∂op/∂input at the current values, so the
// recording is f's linearization. Borrows the dag, which must outlive every
// Var.
template <typename T>
struct Var {
  WeightedDag<T>* dag;
  std::size_t id;
  T value;
};

// A recording session: owns the dag and hands out the leaves; operators derive
// the rest.
template <typename T>
class Tape {
 public:
  auto variable(const T& value) -> Var<T> {
    return {.dag = &dag_, .id = dag_.addNode({}), .value = value};
  }

  // Reverse sweep (the VJP primitive): result[n] = ∂(seed·output)/∂n.
  auto backward(const Var<T>& output, const T& seed = T{1}) const
      -> std::vector<T> {
    assert(output.dag == &dag_ && "output was recorded on another tape");
    return backward(output.id, seed);
  }

  // By id — usable after a tape move invalidates the Vars' dag pointers.
  auto backward(std::size_t output, const T& seed = T{1}) const
      -> std::vector<T> {
    auto adj = std::vector<T>(dag_.nodeCount(), T{});
    adj[output] = seed;
    return backwardSubstitute(dag_, std::move(adj));
  }

  // Invalidates outstanding Vars: their ids alias the next recording.
  auto clear() -> void { dag_.clear(); }

  auto dag() -> WeightedDag<T>& { return dag_; }

  auto dag() const -> const WeightedDag<T>& { return dag_; }

 private:
  WeightedDag<T> dag_;
};

template <typename T>
auto operator+(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.dag == b.dag && "Vars must share one dag");
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, T{1}}, {b.id, T{1}}}),
          .value = a.value + b.value};
}

template <typename T>
auto operator-(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.dag == b.dag && "Vars must share one dag");
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, T{1}}, {b.id, T{-1}}}),
          .value = a.value - b.value};
}

template <typename T>
auto operator*(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.dag == b.dag && "Vars must share one dag");
  // ∂(ab)/∂a = b, ∂(ab)/∂b = a
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, b.value}, {b.id, a.value}}),
          .value = a.value * b.value};
}

template <typename T>
auto operator/(const Var<T>& a, const Var<T>& b) -> Var<T> {
  assert(a.dag == b.dag && "Vars must share one dag");
  const T inv = T{1} / b.value;  // ∂(a/b)/∂a = 1/b, ∂/∂b = −a/b²
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, inv}, {b.id, -a.value * inv * inv}}),
          .value = a.value * inv};
}

template <typename T>
auto operator-(const Var<T>& a) -> Var<T> {
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, T{-1}}}),
          .value = -a.value};
}

// A scalar operand is a constant: it adds no node — it folds into the value or
// scales the partial through a unary node on the variable operand. Scalars are
// exactly what constructs into T (the same gate as Dual): on a Dual<U>
// recording a bare double lifts to a zero-tangent Dual, a lossy mix fails to
// compile.
template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator+(const Var<T>& a, const U& s) -> Var<T> {
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, T{1}}}),
          .value = a.value + static_cast<T>(s)};
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator+(const U& s, const Var<T>& a) -> Var<T> {
  return a + s;
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator-(const Var<T>& a, const U& s) -> Var<T> {
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, T{1}}}),
          .value = a.value - static_cast<T>(s)};
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator-(const U& s, const Var<T>& a) -> Var<T> {
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, T{-1}}}),
          .value = static_cast<T>(s) - a.value};
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator*(const Var<T>& a, const U& s) -> Var<T> {
  const T c = static_cast<T>(s);
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, c}}),
          .value = a.value * c};
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator*(const U& s, const Var<T>& a) -> Var<T> {
  return a * s;
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator/(const Var<T>& a, const U& s) -> Var<T> {
  const T inv = T{1} / static_cast<T>(s);
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, inv}}),
          .value = a.value * inv};
}

template <typename T, typename U>
  requires(std::constructible_from<T, const U&>)
auto operator/(const U& s, const Var<T>& a) -> Var<T> {
  // d(c/x) = −(c/x)/x: x² never forms.
  const T q = static_cast<T>(s) / a.value;
  return {.dag = a.dag,
          .id = a.dag->addNode({{a.id, -q / a.value}}),
          .value = q};
}

// Elementary functions: unqualified calls, so on Var<Dual> the partials
// compose with the dual overloads via ADL.
template <typename T>
auto sin(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::sin;
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, cos(x.value)}}),
          .value = sin(x.value)};
}

template <typename T>
auto cos(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::sin;
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, -sin(x.value)}}),
          .value = cos(x.value)};
}

template <typename T>
auto exp(const Var<T>& x) -> Var<T> {
  using std::exp;
  const T e = exp(x.value);
  return {.dag = x.dag, .id = x.dag->addNode({{x.id, e}}), .value = e};
}

template <typename T>
auto log(const Var<T>& x) -> Var<T> {
  using std::log;
  assert(x.value > T{} && "log domain: value must be > 0");
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, T{1} / x.value}}),
          .value = log(x.value)};
}

template <typename T>
auto sqrt(const Var<T>& x) -> Var<T> {
  using std::sqrt;
  assert(x.value >= T{} && "sqrt domain: value must be ≥ 0");
  const T s = sqrt(x.value);
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, T{1} / (T{2} * s)}}),
          .value = s};
}

template <typename T>
auto tan(const Var<T>& x) -> Var<T> {
  using std::cos;
  using std::tan;
  const T c = cos(x.value);
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, T{1} / (c * c)}}),
          .value = tan(x.value)};
}

template <typename T>
auto asin(const Var<T>& x) -> Var<T> {
  using std::asin;
  using std::sqrt;
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, T{1} / sqrt(T{1} - x.value * x.value)}}),
          .value = asin(x.value)};
}

template <typename T>
auto acos(const Var<T>& x) -> Var<T> {
  using std::acos;
  using std::sqrt;
  return {.dag = x.dag,
          .id = x.dag->addNode(
              {{x.id, T{-1} / sqrt(T{1} - x.value * x.value)}}),
          .value = acos(x.value)};
}

template <typename T>
auto atan(const Var<T>& x) -> Var<T> {
  using std::atan;
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, T{1} / (T{1} + x.value * x.value)}}),
          .value = atan(x.value)};
}

// Non-deduced n so pow(x, 2) binds to T instead of deducing int and clashing
// with T=double.
template <typename T>
auto pow(const Var<T>& x, const std::type_identity_t<T>& n) -> Var<T> {
  using std::pow;
  return {.dag = x.dag,
          .id = x.dag->addNode({{x.id, n * pow(x.value, n - T{1})}}),
          .value = pow(x.value, n)};
}

// d(uᵛ) = uᵛ·(v·u′/u + ln u·v′).
template <typename T>
auto pow(const Var<T>& x, const Var<T>& e) -> Var<T> {
  assert(x.dag == e.dag && "Vars must share one dag");
  using std::log;
  using std::pow;
  const T uv = pow(x.value, e.value);
  return {.dag = x.dag,
          .id = x.dag->addNode(
              {{x.id, uv * e.value / x.value}, {e.id, uv * log(x.value)}}),
          .value = uv};
}

// The value keeps std::fma's single rounding; the partials b, a, 1 are exact
// regardless.
template <typename T>
auto fma(const Var<T>& a, const Var<T>& b, const Var<T>& c) -> Var<T> {
  assert(a.dag == b.dag && a.dag == c.dag && "Vars must share one dag");
  using std::fma;
  return {.dag = a.dag,
          .id = a.dag->addNode(
              {{a.id, b.value}, {b.id, a.value}, {c.id, T{1}}}),
          .value = fma(a.value, b.value, c.value)};
}

}  // namespace tempura::autodiff
