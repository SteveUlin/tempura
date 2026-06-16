#include "sequence.h"

#include <array>
#include <numbers>
#include <ranges>
#include <utility>
#include <vector>

#include "unit.h"

using namespace tempura;

// Compensation is constexpr-evaluable: the recovered 1.0 is a compile-time value.
static_assert(compensatedSum(std::array{1e16, 1.0, -1e16}) == 1.0);

auto main() -> int {
  "Generate yields successive function calls"_test = [] {
    constexpr int first = Generate{[i = 0]() mutable { return ++i; }} | TakeFirst{};
    static_assert(first == 1);
  };

  "continued fraction converges to sqrt2"_test = [] {
    // √2 = 1 + 1/(2 + 1/(2 + 1/(2 + …))) — every (aₖ, bₖ) is (1, 2).
    const double cf = std::views::repeat(std::pair{1.0, 2.0}) | continuedFraction() |
                      Converges{.tol = {.rtol = 1e-15}};
    expectClose(1.0 + cf, std::numbers::sqrt2, {.rtol = 1e-12});
  };

  "sum until converged: geometric series → 2"_test = [] {
    // 1 + 1/2 + 1/4 + … = 2, generated as a recurrence (each term is the last × ½).
    // take() bounds the infinite generator — the budget lives at the call site.
    const double total = Generate{[t = 1.0]() mutable {
                           const double cur = t;
                           t *= 0.5;
                           return cur;
                         }} |
                         std::views::take(64) | SumUntilConverged{.tol = {.rtol = 1e-12}};
    expectClose(total, 2.0, {.rtol = 1e-10});
  };

  "compensated sum recovers what a naive sum drops"_test = [] {
    const std::array terms{1e16, 1.0, -1e16};  // true sum is 1.0
    double naive = 0.0;                         // 1e16 + 1 == 1e16, then − 1e16 == 0
    for (double t : terms) naive += t;
    expectEq(naive, 0.0);
    expectEq(compensatedSum(terms), 1.0);
  };

  "inclusive scan over reference data (constexpr)"_test = [] {
    constexpr static std::array data{1, 2, 3, 4, 5};
    constexpr InclusiveScanView view{data};
    constexpr static std::array expected{1, 3, 6, 10, 15};
    static_assert(std::ranges::equal(expected, view));
  };

  "inclusive scan over a temporary"_test = [] {
    InclusiveScanView view{std::vector<int>{1, 2, 3, 4, 5}};
    const std::array expected{1, 3, 6, 10, 15};
    expectRangeEq(expected, view);
  };

  "inclusive scan, pipe syntax"_test = [] {
    const std::array data{1, 2, 3, 4, 5};
    const std::array expected{1, 3, 6, 10, 15};
    expectRangeEq(expected, data | inclusiveScan());
  };

  return TestRegistry::result();
}
