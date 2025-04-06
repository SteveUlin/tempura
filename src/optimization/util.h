#pragma once

#include <cmath>

namespace tempura::optimization {

// A record of the input and output of a function
template <typename T, typename U>
struct Record {
  T input;
  U output;
};

template <typename T, std::invocable<T> Func>
[[gnu::always_inline]] constexpr auto mkRecord(const T& input, Func&& func) {
  using U = std::invoke_result_t<Func, T>;
  return Record<T, U>{input, func(input)};
}

// A bracket is defined to be thee function evaluations such that
// - b's input is between a and c
// - b's output is less than both a and c
template <typename T>
struct Bracket {
  T a;
  T b;
  T c;
};

// A helper struct that wraps an optional tolerance parameter for most
// optimization methods.
template <typename T>
struct Tolerance {
  // Minimization problems are only solvable up to the sqrt of the precision
  // of the underlying number system. We take the tolerance to be slightly
  // higher than that.
  //
  // This result comes from the Taylor expansion of a function near a minimum
  // `b`
  // f(x) = f(b) + ϵ >= f(b) + 0 + f''(x - b) * (x - b)² / 2
  // ϵ >= f''(x - b) * δ² / 2
  // |x - b| <= |b|√ϵ * √(2 |f(b)| / (b² f''(b)))
  //
  // As our input representation precision maxes out at ϵ, we can only get
  // a ranges as accurate as the √ϵ.
  T value = [] {
    using std::sqrt;
    using std::numeric_limits;
     // A bit larger than the best theoretical precision
     return 4 * sqrt(numeric_limits<T>::epsilon());
  }();
};

}  // namespace tempura::optimization
