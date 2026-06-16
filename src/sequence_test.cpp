#include "sequence.h"

#include <array>
#include <numbers>
#include <ranges>
#include <utility>
#include <vector>

#include "unit.h"

using namespace tempura;

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
    const double total = Generate{[t = 1.0]() mutable {
                           const double cur = t;
                           t *= 0.5;
                           return cur;
                         }} |
                         SumUntilConverged{.tol = {.rtol = 1e-12}};
    expectClose(total, 2.0, {.rtol = 1e-10});
  };

  "sum until converged: a finite range exhausts to its total"_test = [] {
    const std::array terms{1.0, 2.0, 3.0};  // never "converges" → runs to the end
    expectEq(terms | SumUntilConverged{.tol = {.atol = 0.0}}, 6.0);
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
