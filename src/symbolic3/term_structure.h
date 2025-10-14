#pragma once

#include "symbolic3/core.h"
#include "symbolic3/operators.h"
#include "symbolic3/ordering.h"

// Term structure analysis for sophisticated sorting in canonical forms
// Extracts algebraic meaning from expressions to enable like-term grouping
//
// PURPOSE:
// Standard lexicographic ordering would sort x, 2*x, 3*x separately
// This module extracts structure (coefficient, base) to group them together
//
// ADDITION TERMS: coefficient * base
//   Term      Coefficient  Base
//   ----      -----------  ----
//   x         1            x
//   3*x       3            x
//   x*a       1            x*a
//   2*(x*a)   2            x*a
//
// MULTIPLICATION TERMS: base ^ exponent
//   Term      Base  Exponent
//   ----      ----  --------
//   x         x     1
//   x^2       x     2
//   3         3     1
//
// USAGE:
// compareAdditionTerms(a, b) sorts terms to group like bases
// compareMultiplicationTerms(a, b) sorts terms to group like bases

namespace tempura::symbolic3 {

// ============================================================================
// Helper: Check if expression is a specific operation
// ============================================================================

template <typename T, typename Op>
struct IsOp : std::false_type {};

template <typename Op, Symbolic... Args>
struct IsOp<Expression<Op, Args...>, Op> : std::true_type {};

template <typename T, typename Op>
constexpr bool is_op = IsOp<T, Op>::value;

// ============================================================================
// Addition Term Structure: coefficient * base
// ============================================================================

namespace detail {

// Extract coefficient from a multiplication expression
// For Mul(a, b, c, ...): if first arg is constant, it's the coefficient
template <Symbolic Term>
struct GetCoefficientImpl {
  using type = Constant<1>;  // Default: implicit coefficient 1
  using base = Term;         // Whole term is the base
};

// Specialization for constants: coefficient is the constant itself, base is 1
template <auto val>
struct GetCoefficientImpl<Constant<val>> {
  using type = Constant<val>;
  using base = Constant<1>;
};

// Specialization for fractions
template <long long N, long long D>
struct GetCoefficientImpl<Fraction<N, D>> {
  using type = Fraction<N, D>;
  using base = Constant<1>;
};

// Specialization for multiplication: check if first arg is a constant
template <Symbolic First, Symbolic... Rest>
struct GetCoefficientImpl<Expression<MulOp, First, Rest...>> {
  // If first argument is a constant/fraction, it's the coefficient
  using type = std::conditional_t<is_constant<First> || is_fraction<First>,
                                  First, Constant<1>>;

  // Base is the rest (if we extracted a coefficient) or the whole term
  using base = std::conditional_t<
      is_constant<First> || is_fraction<First>,
      // We have a coefficient: base is the rest of the multiplication
      // If only two args, base is just the second arg
      // If more args, base is Mul(Rest...)
      std::conditional_t<
          sizeof...(Rest) == 1,
          // Binary multiplication: base is just the second argument
          typename std::tuple_element<0, std::tuple<Rest...>>::type,
          // Multiple args: base is Mul of the rest
          Expression<MulOp, Rest...>>,
      // No coefficient: whole term is the base
      Expression<MulOp, First, Rest...>>;
};

}  // namespace detail

// Public interface for coefficient extraction
template <Symbolic Term>
using GetCoefficient = typename detail::GetCoefficientImpl<Term>::type;

template <Symbolic Term>
using GetBase = typename detail::GetCoefficientImpl<Term>::base;

// ============================================================================
// Multiplication Term Structure: base ^ exponent
// ============================================================================

namespace detail {

// Extract base and exponent from a power expression
template <Symbolic Term>
struct GetPowerStructure {
  using base = Term;
  using exponent = Constant<1>;  // Default: implicit exponent 1
};

// Specialization for Power expressions: extract base and exponent directly
template <Symbolic Base, Symbolic Exp>
struct GetPowerStructure<Expression<PowOp, Base, Exp>> {
  using base = Base;
  using exponent = Exp;
};

}  // namespace detail

template <Symbolic Term>
using GetPowerBase = typename detail::GetPowerStructure<Term>::base;

template <Symbolic Term>
using GetPowerExponent = typename detail::GetPowerStructure<Term>::exponent;

// ============================================================================
// Comparison using term structure
// ============================================================================

// Forward declaration (from ordering.h)
template <Symbolic Lhs, Symbolic Rhs>
constexpr auto compare(Lhs lhs, Rhs rhs) -> Ordering;

// Compare two addition terms by their algebraic structure
// Groups terms with the same base together, then sorts by coefficient
//
// SORTING STRATEGY:
// 1. Constants (pure numbers) first
// 2. Terms with same base grouped together
// 3. Within same-base group, sort by coefficient
//
// EXAMPLE:
//   Input:  x + 3*y + 2 + 5*x + 1
//   Groups: [2, 1] [x, 5*x] [3*y]
//   Output: 1 + 2 + x + 5*x + 3*y
template <Symbolic A, Symbolic B>
constexpr auto compareAdditionTerms(A, B) -> Ordering {
  using A_coeff = GetCoefficient<A>;
  using A_base = GetBase<A>;
  using B_coeff = GetCoefficient<B>;
  using B_base = GetBase<B>;

  // Strategy: constants (base=1) first, then sort by base, then by coefficient

  // Check if either is a pure constant (base = Constant<1>)
  constexpr bool A_is_pure_const = std::is_same_v<A_base, Constant<1>>;
  constexpr bool B_is_pure_const = std::is_same_v<B_base, Constant<1>>;

  if constexpr (A_is_pure_const && !B_is_pure_const) {
    return Ordering::Less;  // Constants come first
  } else if constexpr (!A_is_pure_const && B_is_pure_const) {
    return Ordering::Greater;
  } else if constexpr (A_is_pure_const && B_is_pure_const) {
    // Both are constants: compare the coefficients (which are the values)
    return compare(A_coeff{}, B_coeff{});
  } else {
    // Neither is a pure constant: compare bases first to group like terms
    constexpr auto base_cmp = compare(A_base{}, B_base{});
    if constexpr (base_cmp != Ordering::Equal) {
      return base_cmp;
    } else {
      // Same base: compare coefficients (smaller coefficient first)
      // This enables factoring rules: x + 2*x + 3*x → (1+2+3)*x → 6*x
      return compare(A_coeff{}, B_coeff{});
    }
  }
}

// Compare two multiplication terms by their algebraic structure
// Groups terms with the same base together, then sorts by exponent
//
// SORTING STRATEGY:
// 1. Constants (numbers) first
// 2. Terms with same base grouped together
// 3. Within same-base group, sort by exponent (ascending)
//
// EXAMPLE:
//   Input:  x * 3 * y^2 * x^2 * 2
//   Groups: [3, 2] [x, x^2] [y^2]
//   Output: 2 * 3 * x * x^2 * y^2  (then constant folding → 6 * x * x^2 * y^2)
//   Then power combining: 6 * x^3 * y^2
template <Symbolic A, Symbolic B>
constexpr auto compareMultiplicationTerms(A, B) -> Ordering {
  using A_base = GetPowerBase<A>;
  using A_exp = GetPowerExponent<A>;
  using B_base = GetPowerBase<B>;
  using B_exp = GetPowerExponent<B>;

  // Strategy: constants first, then sort by base, then by exponent

  constexpr bool A_is_const = is_constant<A_base> || is_fraction<A_base>;
  constexpr bool B_is_const = is_constant<B_base> || is_fraction<B_base>;

  if constexpr (A_is_const && !B_is_const) {
    return Ordering::Less;  // Constants come first
  } else if constexpr (!A_is_const && B_is_const) {
    return Ordering::Greater;
  } else if constexpr (A_is_const && B_is_const) {
    // Both constants: compare values directly
    return compare(A{}, B{});
  } else {
    // Neither is constant: compare bases first to group like terms
    constexpr auto base_cmp = compare(A_base{}, B_base{});
    if constexpr (base_cmp != Ordering::Equal) {
      return base_cmp;
    } else {
      // Same base: compare exponents (lower exponent first)
      // This enables power combining: x * x^2 → x^(1+2) → x^3
      return compare(A_exp{}, B_exp{});
    }
  }
}

// ============================================================================
// Comparator types for use with sorting algorithms
// ============================================================================

struct AdditionTermComparator {
  template <typename A, typename B>
  constexpr bool operator()(A, B) const {
    return compareAdditionTerms(A{}, B{}) == Ordering::Less;
  }
};

struct MultiplicationTermComparator {
  template <typename A, typename B>
  constexpr bool operator()(A, B) const {
    return compareMultiplicationTerms(A{}, B{}) == Ordering::Less;
  }
};

}  // namespace tempura::symbolic3
