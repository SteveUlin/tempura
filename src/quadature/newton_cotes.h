#pragma once

#include <cassert>
#include <concepts>
#include <cstdint>
#include <deque>
#include <utility>

#include "interpolate.h"

// Newton-Cotes quadature are numerical integration methods that approximate
// an integral by evaluating the integrand at equally spaced points.
//
// https://en.wikipedia.org/wiki/Newton%E2%80%93Cotes_formulas

namespace tempura::quadature {

// Trapezoidal Integrator approximates the integral of a function by evaluating
// the function at a bunch of points, then connecting the points with straight
// lines.
template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class TrapazoidalIntegrator {
 public:
  // ThapazoidalIntegrator will always evaluate the function at the endpoints,
  // You make optionally specify the number of points to evaluate the function
  // on the interval [a, b].
  constexpr TrapazoidalIntegrator(F func, T a, T b, int64_t initial_points = 0)
      : func_{std::move(func)},
        a_{std::move(a)},
        b_{std::move(b)},
        num_points_{initial_points},
        result_{(func_(a_) + func_(b_)) / 2.0} {
    assert(num_points_ >= 0);
    // Each endpoint counts as half a point
    ++num_points_;
    const U weight = U{1.0} / num_points_;
    result_ *= weight;

    const T delta = (b_ - a_) / num_points_;
    T curr = a_ + delta;
    // We already evaluated the function at the endpoints so this loop
    // is num_points - 1.
    for (int64_t i = 0; i < num_points_ - 1; ++i) {
      result_ += func_(curr) * weight;
      curr += delta;
    }
  }

  constexpr auto result() const -> U { return result_ * (b_ - a_); }

  constexpr auto refine() {
    result_ /= 2.0;

    const T delta = (b_ - a_) / num_points_;
    const U weight = U{1.0} / (num_points_ * 2);
    T curr = a_ + (delta / 2.0);

    for (int64_t i = 0; i < num_points_; ++i) {
      result_ += func_(curr) * weight;
      curr += delta;
    }

    num_points_ *= 2;
  }

 private:
  F func_;
  T a_;
  T b_;
  int64_t num_points_;
  U result_;
};

template <typename T, std::invocable<T> F>
TrapazoidalIntegrator(F, T, T, int64_t) -> TrapazoidalIntegrator<T, F>;

// Simpson's Rule is a quadrature method that approximates the integral of a
// function with a quadratic polynomial. The error of Simpson's Rule is one
// degree higher than the error of the Trapezoidal Rule for the same number of
// function evaluations.
//
// If Sₙ is the trapezoidal rule and S₂ₙ is Simpson's Rule, then:
// Simpson == 4/3 S₂ₙ - 1/3 Sₙ
//
// See Numerical Recipes, 3rd Edition, Section 4.2

template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class SimpsonIntegrator {
 public:
  // num_points is the initial number of interior evaluation points of the
  // function divided by two since we apply the TrapezoidalIntegrator twice.
  constexpr SimpsonIntegrator(F func, T a, T b, int64_t initial_points = 0)
      : trapazoidal_{std::move(func), a, b, initial_points} {
    prev_ = trapazoidal_.result();
    trapazoidal_.refine();
    curr_ = trapazoidal_.result();
  }

  constexpr auto result() const -> U { return (4.0 * curr_ - prev_) / 3.0; }

  constexpr auto refine() {
    prev_ = curr_;
    trapazoidal_.refine();
    curr_ = trapazoidal_.result();
  }

 private:
  TrapazoidalIntegrator<T, F, U> trapazoidal_;
  U prev_;
  U curr_;
};

template <typename T, std::invocable<T> F>
SimpsonIntegrator(F, T, T, int64_t) -> SimpsonIntegrator<T, F>;

// Romberg's Method evaluates the integral at different levels of fidelity.
// We then fit a polynomial to the results and extrapolate to the limit of
// h -> 0.
//
// We only fit a polynomial to the last K results. (Simpson's Rule can be
// viewed as a special case of Romberg's Method where K = 2).
//
// The error of the Trapezoidal Rule only includes even powers of h, so we
// interpolate in terms of h² instead of h.
//
// See Numerical Recipes, 3rd Edition, Section 4.2
template <typename T, std::invocable<T> F, typename U = std::invoke_result_t<F, T>>
class RombergIntegrator {
 public:
  constexpr RombergIntegrator(int64_t levels, F func, T a, T b, int64_t initial_points = 0)
      : levels_{levels}, trapazoidal_{std::move(func), a, b, initial_points} {
    results_.emplace_back(1.0, trapazoidal_.result());
    for (int64_t i = 1; i < levels; ++i) {
      trapazoidal_.refine();
      // The x coordinate corresponds to a normalized h²
      results_.emplace_back(results_.back().first * 0.25, trapazoidal_.result());
    }
  }

  constexpr auto result() const -> U {
    return PolynomialInterpolator{std::views::all(results_)}(0.0);
  }

  constexpr auto refine() {
    trapazoidal_.refine();
    results_.pop_front();
    results_.emplace_back(results_.back().first * 0.25, trapazoidal_.result());
  }

 private:
  int64_t levels_;
  TrapazoidalIntegrator<T, F, U> trapazoidal_;
  std::deque<std::pair<double, U>> results_;
};

}  // namespace tempura::quadature
