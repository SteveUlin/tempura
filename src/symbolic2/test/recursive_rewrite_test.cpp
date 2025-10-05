#include "symbolic2/recursive_rewrite.h"

#include "symbolic2/constants.h"
#include "symbolic2/operators.h"
#include "unit.h"

using namespace tempura;

// Forward declaration for recursive call
template <Symbolic Expr, Symbolic Var>
constexpr auto diff_recursive(Expr expr, Var var);

// =============================================================================
// DIFFERENTIATION RULES USING RECURSIVE REWRITE SYSTEM
// =============================================================================

// Base case: d/dx(c) = 0 (constant)
constexpr auto DiffConstant = RecursiveRewrite{
    AnyConstant{},  // Pattern: any constant
    1_c             // Replacement: just return 0
};

// Base case: d/dx(x) = 1 (variable matches itself)
// This one is tricky - we need a predicate to check if expr == var
struct MatchesVar {
  template <typename Context, Symbolic Var>
  static constexpr bool operator()(Context, Var var, auto expr) {
    return match(expr, var);
  }
};

// For now, let's try with pattern variables
// Sum rule: d/dx(f + g) = df/dx + dg/dx
constexpr auto DiffSum =
    RecursiveRewrite{x_ + y_,  // Pattern
                     [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto g = get(ctx, y_);
                       return diff_fn(f, var) + diff_fn(g, var);
                     }};

// Product rule: d/dx(f * g) = df/dx * g + f * dg/dx
constexpr auto DiffProduct =
    RecursiveRewrite{x_ * y_,  // Pattern
                     [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto g = get(ctx, y_);
                       return diff_fn(f, var) * g + f * diff_fn(g, var);
                     }};

// Power rule: d/dx(f^n) = n * f^(n-1) * df/dx
constexpr auto DiffPower =
    RecursiveRewrite{pow(x_, y_),  // Pattern
                     [](auto ctx, auto diff_fn, auto var) {
                       constexpr auto f = get(ctx, x_);
                       constexpr auto n = get(ctx, y_);
                       return n * pow(f, n - 1_c) * diff_fn(f, var);
                     }};

// Sine: d/dx(sin(f)) = cos(f) * df/dx
constexpr auto DiffSin = RecursiveRewrite{sin(x_),  // Pattern
                                          [](auto ctx, auto diff_fn, auto var) {
                                            constexpr auto f = get(ctx, x_);
                                            return cos(f) * diff_fn(f, var);
                                          }};

// Create the rewrite system
constexpr auto DiffRules =
    RecursiveRewriteSystem{DiffSum, DiffProduct, DiffPower, DiffSin};

// The main diff function
template <Symbolic Expr, Symbolic Var>
constexpr auto diff_recursive(Expr expr, Var var) {
  // Try base cases first
  if constexpr (match(expr, var)) {
    return 1_c;
  } else if constexpr (match(expr, AnySymbol{})) {
    return 0_c;  // Different symbol
  } else if constexpr (match(expr, AnyConstant{})) {
    return 0_c;
  } else {
    // Try rewrite rules - wrap the recursive call in a lambda
    auto diff_fn = []<Symbolic E, Symbolic V>(E e, V v) {
      return diff_recursive(e, v);
    };
    return DiffRules.apply(expr, diff_fn, var);
  }
}

// =============================================================================
// TESTS
// =============================================================================

auto main() -> int {
  "Recursive: Derivative of constant"_test = [] {
    Symbol x;
    auto expr = 1_c;
    auto d = diff_recursive(expr, x);
    static_assert(match(d, 0_c));
  };

  "Recursive: Derivative of x"_test = [] {
    Symbol x;
    auto d = diff_recursive(x, x);
    static_assert(match(d, 1_c));
  };

  "Recursive: Derivative of x + 1"_test = [] {
    Symbol x;
    auto expr = x + 1_c;
    auto d = diff_recursive(expr, x);
    // d/dx(x + 1) = 1 + 0
    static_assert(match(d, 1_c + 0_c));
  };

  "Recursive: Product rule x * x"_test = [] {
    Symbol x;
    auto expr = x * x;
    auto d = diff_recursive(expr, x);
    // d/dx(x*x) = 1*x + x*1
    static_assert(match(d, 1_c * x + x * 1_c));
  };

  "Recursive: Power rule x^2"_test = [] {
    Symbol x;
    auto expr = pow(x, 2_c);
    auto d = diff_recursive(expr, x);
    // d/dx(x^2) = 2*x^1*1
    static_assert(match(d, 2_c * pow(x, 2_c - 1_c) * 1_c));
  };

  "Recursive: Chain rule sin(x^2)"_test = [] {
    Symbol x;
    auto expr = sin(pow(x, 2_c));
    auto d = diff_recursive(expr, x);
    // d/dx(sin(x^2)) = cos(x^2) * 2*x^1*1
    static_assert(match(d, cos(pow(x, 2_c)) * (2_c * pow(x, 2_c - 1_c) * 1_c)));
  };

  return 0;
}
