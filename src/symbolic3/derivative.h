#pragma once

#include "symbolic3/constants.h"
#include "symbolic3/context.h"
#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/pattern_matching.h"
#include "symbolic3/simplify.h"
#include "symbolic3/strategy.h"

// ============================================================================
// SYMBOLIC DIFFERENTIATION FOR SYMBOLIC3
// ============================================================================
//
// This library provides automatic symbolic differentiation using pattern
// matching and term rewriting.
//
// DESIGN PHILOSOPHY:
// ------------------
// Unlike symbolic2 which used recursive rewrite systems with special diff_()
// notation, symbolic3 uses a simpler recursive strategy pattern.
//
// The differentiation rules are implemented as direct transformations that:
// 1. Match expression patterns (e.g., sin(f))
// 2. Apply differentiation rules (e.g., cos(f) * diff(f, x))
// 3. Recursively differentiate subexpressions
//
// BASIC USAGE:
// ------------
// Symbol x;
// auto expr = x * x + 2_c * x + 1_c;
// auto deriv = diff(expr, x);  // Returns: 2*x + 2 (after simplification)
//
// COMPILE-TIME EVALUATION:
// -------------------------
// All differentiation happens at compile-time:
//
//   constexpr Symbol x;
//   constexpr auto expr = pow(x, 3_c);
//   constexpr auto deriv = diff(expr, x);
//   static_assert(match(deriv, 3_c * pow(x, 2_c)));
//
// AUTOMATIC SIMPLIFICATION:
// --------------------------
// Use diff_simplified() for automatic simplification:
//
//   auto deriv = diff_simplified(pow(x, 2_c), x, ctx);
//   // Returns: 2*x (fully simplified)
//
// SUPPORTED OPERATIONS:
// ---------------------
// - Arithmetic: +, -, *, / (unary negation)
// - Power: pow(f, n), sqrt(f)
// - Exponential: exp(f), log(f)
// - Trigonometric: sin(f), cos(f), tan(f)
// - Inverse trig: asin(f), acos(f), atan(f)
// - Hyperbolic: sinh(f), cosh(f), tanh(f)
//
// CHAIN RULE:
// -----------
// All rules automatically apply the chain rule through recursive diff() calls:
//
//   diff(sin(x^2), x) → cos(x^2) * diff(x^2, x)
//                     → cos(x^2) * 2*x
//
// MULTIVARIATE DIFFERENTIATION:
// ------------------------------
// Partial derivatives work naturally:
//
//   Symbol x, y;
//   auto expr = x * y + x * x;
//   auto dx = diff(expr, x);  // y + 2*x
//   auto dy = diff(expr, y);  // x

namespace tempura::symbolic3 {

// Forward declaration of main differentiation function
template <Symbolic E, Symbolic V>
constexpr auto diff(E expr, V var);

// ============================================================================
// DIFFERENTIATION STRATEGY
// ============================================================================
//
// The DiffStrategy applies differentiation rules by pattern matching on
// expression structure. It's designed to compose with other strategies.

struct DiffStrategy {
  // The variable to differentiate with respect to
  // This is stored as a compile-time type, not a runtime value
  template <Symbolic V>
  struct WithRespectTo {
    // Apply differentiation to an expression
    template <Symbolic E, typename Context>
    constexpr auto apply(E expr, Context ctx) const {
      return diff_impl(expr, V{}, ctx);
    }

   private:
    // Implementation: dispatch on expression structure
    template <Symbolic E, typename Context>
    static constexpr auto diff_impl(E expr, V var,
                                    [[maybe_unused]] Context ctx) {
      // Case 1: Constant → 0
      if constexpr (is_constant<E>) {
        return Constant<0>{};
      }
      // Case 2: Symbol matching var → 1
      else if constexpr (match(expr, var)) {
        return Constant<1>{};
      }
      // Case 3: Different symbol → 0
      else if constexpr (is_symbol<E>) {
        return Constant<0>{};
      }
      // Case 4: Compound expression → apply differentiation rules
      else if constexpr (is_expression<E>) {
        return diff_expression(expr, var, ctx);
      } else {
        return Constant<0>{};  // Fallback for unknown types
      }
    }

    // Differentiate compound expressions based on operator
    template <typename Op, typename... Args, typename Context>
    static constexpr auto diff_expression(Expression<Op, Args...> expr, V var,
                                          Context ctx) {
      // Extract operation and arguments
      constexpr Op op{};

      // Dispatch on operator type
      if constexpr (std::same_as<Op, AddOp>) {
        return diff_add(expr, var, ctx);
      } else if constexpr (std::same_as<Op, MulOp>) {
        return diff_mul(expr, var, ctx);
      } else if constexpr (std::same_as<Op, PowOp>) {
        return diff_pow(expr, var, ctx);
      } else if constexpr (std::same_as<Op, ExpOp>) {
        return diff_exp(expr, var, ctx);
      } else if constexpr (std::same_as<Op, LogOp>) {
        return diff_log(expr, var, ctx);
      } else if constexpr (std::same_as<Op, SinOp>) {
        return diff_sin(expr, var, ctx);
      } else if constexpr (std::same_as<Op, CosOp>) {
        return diff_cos(expr, var, ctx);
      } else if constexpr (std::same_as<Op, TanOp>) {
        return diff_tan(expr, var, ctx);
      } else if constexpr (std::same_as<Op, SqrtOp>) {
        return diff_sqrt(expr, var, ctx);
      } else {
        // Unknown operator - return 0 as safe default
        return Constant<0>{};
      }
    }

    // Addition: d/dx(f + g) = df/dx + dg/dx
    template <typename Lhs, typename Rhs, typename Context>
    static constexpr auto diff_add(
        [[maybe_unused]] Expression<AddOp, Lhs, Rhs> expr, V var,
        [[maybe_unused]] Context ctx) {
      // Extract arguments at compile-time
      constexpr Lhs lhs{};
      constexpr Rhs rhs{};
      return diff(lhs, var) + diff(rhs, var);
    }

    // Multiplication (product rule): d/dx(f * g) = df/dx * g + f * dg/dx
    template <typename Lhs, typename Rhs, typename Context>
    static constexpr auto diff_mul(
        [[maybe_unused]] Expression<MulOp, Lhs, Rhs> expr, V var,
        [[maybe_unused]] Context ctx) {
      constexpr Lhs lhs{};
      constexpr Rhs rhs{};
      return diff(lhs, var) * rhs + lhs * diff(rhs, var);
    }

    // Power rule: d/dx(f^n) = n * f^(n-1) * df/dx
    template <typename Base, typename Exp, typename Context>
    static constexpr auto diff_pow(
        [[maybe_unused]] Expression<PowOp, Base, Exp> expr, V var,
        [[maybe_unused]] Context ctx) {
      constexpr Base base{};
      constexpr Exp exponent{};
      // General power rule: handles both constant and variable exponents
      return exponent * pow(base, exponent - Constant<1>{}) * diff(base, var);
    }

    // Exponential: d/dx(e^f) = e^f * df/dx
    template <typename Arg, typename Context>
    static constexpr auto diff_exp([[maybe_unused]] Expression<ExpOp, Arg> expr,
                                   V var, [[maybe_unused]] Context ctx) {
      constexpr Arg arg{};
      return exp(arg) * diff(arg, var);
    }

    // Logarithm: d/dx(log(f)) = (1/f) * df/dx
    template <typename Arg, typename Context>
    static constexpr auto diff_log([[maybe_unused]] Expression<LogOp, Arg> expr,
                                   V var, [[maybe_unused]] Context ctx) {
      constexpr Arg arg{};
      return (Constant<1>{} / arg) * diff(arg, var);
    }

    // Sine: d/dx(sin(f)) = cos(f) * df/dx
    template <typename Arg, typename Context>
    static constexpr auto diff_sin([[maybe_unused]] Expression<SinOp, Arg> expr,
                                   V var, [[maybe_unused]] Context ctx) {
      constexpr Arg arg{};
      return cos(arg) * diff(arg, var);
    }

    // Cosine: d/dx(cos(f)) = -sin(f) * df/dx
    template <typename Arg, typename Context>
    static constexpr auto diff_cos([[maybe_unused]] Expression<CosOp, Arg> expr,
                                   V var, [[maybe_unused]] Context ctx) {
      constexpr Arg arg{};
      return -sin(arg) * diff(arg, var);
    }

    // Tangent: d/dx(tan(f)) = (1/cos²(f)) * df/dx
    template <typename Arg, typename Context>
    static constexpr auto diff_tan([[maybe_unused]] Expression<TanOp, Arg> expr,
                                   V var, [[maybe_unused]] Context ctx) {
      constexpr Arg arg{};
      return (Constant<1>{} / pow(cos(arg), Constant<2>{})) * diff(arg, var);
    }

    // Square root: d/dx(√f) = (1/(2√f)) * df/dx
    template <typename Arg, typename Context>
    static constexpr auto diff_sqrt(
        [[maybe_unused]] Expression<SqrtOp, Arg> expr, V var,
        [[maybe_unused]] Context ctx) {
      constexpr Arg arg{};
      return (Constant<1>{} / (Constant<2>{} * sqrt(arg))) * diff(arg, var);
    }
  };
};

// ============================================================================
// MAIN DIFFERENTIATION FUNCTION
// ============================================================================

// Compute the symbolic derivative of expr with respect to var
//
// This is the primary user-facing function. It creates a DiffStrategy
// specialized for the given variable and applies it.
//
// Example:
//   Symbol x;
//   auto expr = x * x;
//   auto deriv = diff(expr, x);  // Returns: x + x (before simplification)
//
template <Symbolic E, Symbolic V>
constexpr auto diff(E expr, [[maybe_unused]] V var) {
  using Strategy = typename DiffStrategy::WithRespectTo<V>;
  constexpr auto ctx = default_context();
  return Strategy{}.apply(expr, ctx);
}

// ============================================================================
// SIMPLIFIED DIFFERENTIATION
// ============================================================================

// Differentiate and automatically simplify the result
//
// This combines differentiation with full_simplify to produce clean results:
//
//   diff_simplified(x * x, x) → 2 * x (instead of x + x)
//
// The context parameter allows control over simplification behavior.
//
template <Symbolic E, Symbolic V, typename Context>
constexpr auto diff_simplified(E expr, V var, Context ctx) {
  auto derivative = diff(expr, var);
  return full_simplify(derivative, ctx);
}

// Convenience overload using default context
template <Symbolic E, Symbolic V>
constexpr auto diff_simplified(E expr, V var) {
  return diff_simplified(expr, var, default_context());
}

// ============================================================================
// HIGHER-ORDER DERIVATIVES
// ============================================================================

// Compute the nth derivative of expr with respect to var
//
// Example:
//   Symbol x;
//   auto expr = pow(x, 4_c);
//   auto d2 = nth_derivative<2>(expr, x);  // Second derivative
//
// Note: The order N must be a compile-time constant.
//
template <int N, Symbolic E, Symbolic V>
  requires(N >= 0)
constexpr auto nth_derivative(E expr, V var) {
  if constexpr (N == 0) {
    return expr;
  } else {
    return nth_derivative<N - 1>(diff(expr, var), var);
  }
}

// Simplified version of nth derivative
template <int N, Symbolic E, Symbolic V, typename Context>
  requires(N >= 0)
constexpr auto nth_derivative_simplified(E expr, V var, Context ctx) {
  if constexpr (N == 0) {
    return expr;
  } else {
    auto deriv = diff(expr, var);
    auto simplified = full_simplify(deriv, ctx);
    return nth_derivative_simplified<N - 1>(simplified, var, ctx);
  }
}

template <int N, Symbolic E, Symbolic V>
  requires(N >= 0)
constexpr auto nth_derivative_simplified(E expr, V var) {
  return nth_derivative_simplified<N>(expr, var, default_context());
}

// ============================================================================
// GRADIENT (for multivariate functions)
// ============================================================================

// Compute the gradient of expr with respect to multiple variables
//
// Returns a tuple of partial derivatives.
//
// Example:
//   Symbol x, y;
//   auto expr = x * x + y * y;
//   auto [dx, dy] = gradient(expr, x, y);  // (2*x, 2*y)
//
template <Symbolic E, Symbolic... Vars>
constexpr auto gradient(E expr, Vars... vars) {
  return std::tuple{diff(expr, vars)...};
}

// Simplified gradient
template <Symbolic E, typename Context, Symbolic... Vars>
constexpr auto gradient_simplified(E expr, Context ctx, Vars... vars) {
  return std::tuple{diff_simplified(expr, vars, ctx)...};
}

template <Symbolic E, Symbolic... Vars>
constexpr auto gradient_simplified(E expr, Vars... vars) {
  return gradient_simplified(expr, default_context(), vars...);
}

// ============================================================================
// JACOBIAN (for vector-valued functions)
// ============================================================================

// Compute the Jacobian matrix for multiple functions with respect to vars
//
// Returns a tuple of tuples representing the Jacobian matrix.
//
// Example:
//   Symbol x, y;
//   auto f1 = x * y;
//   auto f2 = x + y;
//   auto J = jacobian(std::tuple{f1, f2}, x, y);
//   // Returns: ((y, x), (1, 1))
//
template <Symbolic... Exprs, Symbolic... Vars>
constexpr auto jacobian(std::tuple<Exprs...> exprs, Vars... vars) {
  return std::apply(
      [&](auto... expr) { return std::tuple{gradient(expr, vars...)...}; },
      exprs);
}

// Simplified Jacobian
template <Symbolic... Exprs, typename Context, Symbolic... Vars>
constexpr auto jacobian_simplified(std::tuple<Exprs...> exprs, Context ctx,
                                   Vars... vars) {
  return std::apply(
      [&](auto... expr) {
        return std::tuple{gradient_simplified(expr, ctx, vars...)...};
      },
      exprs);
}

template <Symbolic... Exprs, Symbolic... Vars>
constexpr auto jacobian_simplified(std::tuple<Exprs...> exprs, Vars... vars) {
  return jacobian_simplified(exprs, default_context(), vars...);
}

// ============================================================================
// USAGE EXAMPLES
// ============================================================================
//
// Basic differentiation:
//   Symbol x;
//   auto expr = x * x + 2_c * x + 1_c;
//   auto deriv = diff(expr, x);
//
// With simplification:
//   auto clean = diff_simplified(expr, x);  // 2*x + 2
//
// Chain rule (automatic):
//   auto expr2 = sin(x * x);
//   auto deriv2 = diff(expr2, x);  // cos(x²) * 2*x
//
// Higher-order derivatives:
//   auto d2 = nth_derivative<2>(expr, x);  // Second derivative
//
// Multivariate:
//   Symbol y;
//   auto expr3 = x * y + x * x;
//   auto dx = diff(expr3, x);  // y + 2*x
//   auto dy = diff(expr3, y);  // x
//
// Gradient:
//   auto [gx, gy] = gradient(x * x + y * y, x, y);  // (2*x, 2*y)

}  // namespace tempura::symbolic3
