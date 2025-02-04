#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <deque>
#include <utility>

#include "interpolate.h"

namespace tempura::quadature {

// Routines for integrating when there is an infinity at the endpoints a or b:

// Midpoint rule
//
// The Second Euler-Maclaurin formula estimates the error when using the
// midpoint rule. It works out to averaging the interior points.
//
// In order to reuse the points from the previous iteration, we need to triple
// the points rather than double them. Think of the intervals at the end of
// each side. If you double the points, then the new leftmost point will no longer
// be a "midpoint". We need to triple the points to keep the leftmost point in
// the middle of the new interval length.

template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class MidpointIntegrator {
 public:
  constexpr MidpointIntegrator(F func, T a, T b, int64_t initial_points = 1)
      : func_{std::move(func)}, a_{std::move(a)}, b_{std::move(b)}, num_points_{initial_points} {
    assert(num_points_ > 0);
    const T delta = (b_ - a_) / (num_points_ + 1);
    T curr = a_ + delta;
    for (int64_t i = 0; i < num_points_; ++i) {
      result_ += func_(curr);
      curr += delta;
    }
    result_ /= num_points_;
  }

  constexpr auto result() const -> U { return result_ * (b_ - a_); }

  constexpr auto refine() {
    const T delta = (b_ - a_) / (num_points_ * 3);

    U sum{0.0};
    T curr = a_ + (0.5 * delta);
    for (int64_t i = 0; i < num_points_; ++i) {
      sum += func_(curr);
      sum += func_(curr + delta + delta);
      curr += delta + delta + delta;
    }

    result_ = (result_ + sum / num_points_) / 3;
    num_points_ = num_points_ * 3;
  }

 private:
  F func_;
  T a_;
  T b_;
  int64_t num_points_;
  U result_{0.0};
};

// See newton_cotes.h
template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class RombergMidpointIntegrator {
 public:
  constexpr RombergMidpointIntegrator(int64_t levels, F func, T a, T b, int64_t initial_points = 1)
      : levels_{levels}, midpoint_{std::move(func), a, b, initial_points} {
    results_.emplace_back(1.0, midpoint_.result());
    for (int64_t i = 1; i < levels; ++i) {
      midpoint_.refine();
      // The x coordinate corresponds to a normalized h²
      results_.emplace_back(results_.back().first / 9, midpoint_.result());
    }
  }

  constexpr auto result() const -> U {
    return PolynomialInterpolator{std::views::all(results_)}(0.0);
  }

  constexpr auto refine() {
    midpoint_.refine();
    results_.pop_front();
    results_.emplace_back(results_.back().first / 9, midpoint_.result());
  }

 private:
  int64_t levels_;
  MidpointIntegrator<T, F, U> midpoint_;
  std::deque<std::pair<double, U>> results_;
};

// Integration to infinity ∞

// If you need to integrate from a to ∞, you can use the substitution
// x = 1/t, dx = -1/t² dt
// ∫ f(x) dx = ∫ f(1/t) (-1/t²) dt
// => ∫ f(1/t) / t² dt from 0 to 1/a
//
// Note, this only works if f(x) decays faster than 1/x² as x -> ∞ and for a >
// 0. If you need to solve for a < 0, you can break up the integral into two:
// - ∫ f(x) dx from a to a'
// - ∫ f(x) dx from a' to ∞
//
// a' should be large enough that f(x) starts to decay to ∞. We can probably
// calculate the integral a to a' more accurately and efficiently with other
// algorithms, so we want to maximize this range without including too many
// small values.
template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class MidpointInfIntegrator {
 public:
  constexpr MidpointInfIntegrator(F func, T a, int64_t initial_points = 1)
      : midpoint_{transform(std::move(func)), 0.0, 1 / a,
                  initial_points} {}

  static constexpr auto transform(F f) {
    return [f = std::move(f)](T x) { return f(1 / x) * 1 / (x * x); };
  }
  constexpr auto result() const -> U { return midpoint_.result(); }

  constexpr auto refine() { midpoint_.refine(); }

 private:
  MidpointIntegrator<T, decltype(transform(std::declval<F>())), U> midpoint_;
};

// Singularities

// Integration of 1 / sqrt(x) singularity
//
// The following transform will integrate a function whose lower bound
// acts like 1 / sqrt(x).
//
// x = a + t², dx = 2t dt
// => ∫ f(x) dx = ∫ f(a + t²) 2t dt from 0 to sqrt(b - a)
template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class MidpointSqrtIntegrator {
 public:
  constexpr MidpointSqrtIntegrator(F func, T a, T b, int64_t initial_points = 1)
      : midpoint_{transform(std::move(func), a), 0.0, std::sqrt(b - a),
                  initial_points} {}
  static constexpr auto transform(F f, T a) {
    return [f = std::move(f), a](T x) { return f(a + x * x) * 2 * x; };
  }
  constexpr auto result() const -> U { return midpoint_.result(); }
  constexpr auto refine() { midpoint_.refine(); }
 private:
  MidpointIntegrator<T, decltype(transform(std::declval<F>(), std::declval<T>())), U> midpoint_;
};

}  // namespace tempura::quadature
