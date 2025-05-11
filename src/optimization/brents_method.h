#include <cassert>
#include <cmath>
#include <cstdlib>
#include <print>
#include <type_traits>
#include <utility>

#include "optimization/util.h"

namespace tempura::optimization {

// Brent's method is a one dimension minimization algorithm based on parabolic
// interpolation.
//
// However, naive parabolic interpolation can chose points that fall outside of
// the bracket of interest. To mitigate this we detect parabolic interpolation
// updates that are too small/large and fall back to the bracketing techniques
// laid out in the "golden search" algorithm.
template <typename T, std::invocable<T> Func>
constexpr auto brentsMethod(
    Bracket<Record<T, std::invoke_result_t<Func, T>>> bracket, Func&& func,
    Tolerance<std::invoke_result_t<Func, T>> tol = {})
    -> Bracket<Record<T, std::invoke_result_t<Func, T>>> {
  using std::abs;
  using std::swap;

  auto& [r0, r1, r2] = bracket;
  if (r0.input > r2.input) {
    swap(r0, r2);
  }
  auto& [x0, f0] = r0;
  auto& [x1, f1] = r1;
  auto& [x2, f2] = r2;

  // For a parabolic interpolation step to be accepted the following must
  // be true:
  //   - the next eval point must be within the bracket
  //   - the next eval point must be less than 1/2 of the step before
  //     last. This skip comparison attempts smooth the evaluation criteria,
  //     allowing small increases in step size as long as the increase is
  //     a one off.
  auto prev_step = x2 - x0;
  auto prev_prev_step = prev_step;

  while (true) {
    std::println("{:.6} {:.6} {:.6}", x0, x1, x2);
    const auto scaled_tol =
        tol.value * (abs(x1) + std::numeric_limits<double>::epsilon());
    if (abs(x2 - x0) < scaled_tol) {
      return bracket;
    }
    // parabolic interpolation
    auto proposal = [&] {
      const auto α = (f1 - f0) / (x1 - x0);
      const auto β = (f2 - f0 - α * (x2 - x0)) / ((x2 - x0) * (x2 - x1));
      return (x0 + x1) / 2 - α / (2 * β);
    }();
    constexpr auto ϵ = std::numeric_limits<double>::epsilon();
    const bool in_range = ((x0 + (std::abs(x0) + ϵ) * tol.value / 2) < proposal) &&
                          (proposal < (x2 - (std::abs(x2) + ϵ) * tol.value / 2));
    const bool within_step = (abs(proposal - x1) < prev_prev_step / 2);
    if (!in_range || !within_step) {
      if (proposal < 0.5 * (x2 + x1)) {
        proposal = x0 + (std::numbers::phi - 1.) * (x2 - x0);
      } else {
        proposal = x2 - (std::numbers::phi - 1.) * (x2 - x0);
      }
    } else if (proposal < x1 && x1 - proposal < scaled_tol / 2) {
      proposal = x1 - scaled_tol / 2;
    } else if (proposal > x1 && proposal - x1 < scaled_tol / 2) {
      proposal = x1 + scaled_tol / 2;
    }
    prev_prev_step = prev_step;
    prev_step = abs(x1 - proposal);
    auto fproposal = func(proposal);
    if (proposal < x1) {
      if (fproposal < f1) {
        x2 = x1;
        f2 = f1;
        x1 = proposal;
        f1 = fproposal;
      } else {
        x0 = proposal;
        f0 = fproposal;
      }
    } else {
      if (fproposal < f1) {
        x0 = x1;
        f0 = f1;
        x1 = proposal;
        f1 = fproposal;
      } else {
        x2 = proposal;
        f2 = fproposal;
      }
    }
  }

  assert((void("Maximum iterations reached"), false));
  std::unreachable();
}

}  // namespace tempura::optimization
