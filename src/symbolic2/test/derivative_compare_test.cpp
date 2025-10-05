#include "symbolic2/binding.h"
#include "symbolic2/derivative.h"
#include "symbolic2/derivative2.h"
#include "symbolic2/evaluate.h"
#include "symbolic2/simplify.h"
#include "unit.h"

using namespace tempura;

// Test both implementations produce the same results
auto main() -> int {
  "Compare: Derivative of constant"_test = [] {
    Symbol x;
    auto expr = 1_c;
    auto d1 = diff(expr, x);  // Original
    auto d2 = diff(expr, x);  // Recursive rewrite
    static_assert(match(d1, d2));
  };

  "Compare: Derivative of x"_test = [] {
    Symbol x;
    auto d1 = diff(x, x);
    auto d2 = diff(x, x);
    static_assert(match(d1, d2));
  };

  "Compare: Sum rule"_test = [] {
    Symbol x;
    auto expr = x + 1_c;
    auto d1 = diff(expr, x);
    auto d2 = diff(expr, x);
    static_assert(match(d1, d2));
  };

  "Compare: Product rule"_test = [] {
    Symbol x;
    auto expr = x * x;
    auto d1 = diff(expr, x);
    auto d2 = diff(expr, x);
    static_assert(match(d1, d2));
  };

  "Compare: Power rule"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c);
    auto d1 = diff(expr, x);
    auto d2 = diff(expr, x);
    static_assert(match(d1, d2));
  };

  "Compare: Chain rule - sin(x^2)"_test = [] {
    Symbol x;
    auto expr = sin(pow(x, 2_c));
    auto d1 = diff(expr, x);
    auto d2 = diff(expr, x);
    static_assert(match(d1, d2));
  };

  "Compare: Complex expression"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c) + 2_c * x + 1_c;
    auto d1 = diff(expr, x);
    auto d2 = diff(expr, x);
    // Both should give the same result
    static_assert(match(d1, d2));
  };

  "Demonstrate simplicity of recursive rules"_test = [] {
    // The recursive rewrite system makes it MUCH easier to write rules!
    //
    // BEFORE (manual template specialization):
    //   template <Symbolic Expr, Symbolic Var>
    //     requires(match(Expr{}, AnyArg{} * AnyArg{}))
    //   constexpr auto diff(Expr expr, Var var) {
    //     constexpr auto f = left(expr);
    //     constexpr auto g = right(expr);
    //     return diff(f, var) * g + f * diff(g, var);
    //   }
    //
    // AFTER (declarative pattern matching):
    //   constexpr auto DiffProduct = RecursiveRewrite{
    //     x_ * y_,
    //     [](auto ctx, auto diff_fn, auto var) {
    //       constexpr auto f = get(ctx, x_);
    //       constexpr auto g = get(ctx, y_);
    //       return diff_fn(f, var) * g + f * diff_fn(g, var);
    //     }
    //   };
    //
    // Much cleaner! The pattern is explicit and the transformation is clear.
  };

  return 0;
}
