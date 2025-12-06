#pragma once

#include "math/ratio.h"
#include "meta/fixed_string.h"
#include "meta/tags.h"
#include "meta/type_list.h"
#include "meta/utility.h"

// Dimension - Compile-time dimensional analysis
//
// REPRESENTATION:
// - Base dimensions: DimLength, DimTime, etc. (exponent = 1)
// - Powers: Power<DimLength, 2> for Length² (positive exponents > 1)
// - Denominators: Per<...> contains all terms with negative exponents
//
// Examples:
//   Velocity = Dimension<DimLength, Per<DimTime>>           (L/T)
//   Area     = Dimension<Power<DimLength, 2>>               (L²)
//   Force    = Dimension<DimLength, DimMass, Per<Power<DimTime, 2>>>  (LM/T²)

namespace tempura {

// ============================================================================
// Exponent Type
// ============================================================================

using Exp = Ratio<long long>;

// ============================================================================
// Base Dimensions with FixedString symbols for ordering
// ============================================================================

template <FixedString Symbol>
struct BaseDim final {
  static constexpr auto kSymbol = Symbol;
};

using DimLength = BaseDim<"L">;
using DimTime = BaseDim<"T">;
using DimMass = BaseDim<"M">;
using DimCurrent = BaseDim<"I">;
using DimTemperature = BaseDim<"Θ">;
using DimAmount = BaseDim<"N">;
using DimLuminosity = BaseDim<"J">;

template <typename T>
concept BaseDimension = requires {
  { T::kSymbol } -> SameAs<const decltype(T::kSymbol)&>;
};

// ============================================================================
// Power - For exponents other than 1
// ============================================================================

template <BaseDimension B, int Num, int... Den>
  requires(Num > 0 && !(Num == 1 && sizeof...(Den) == 0))
struct Power final {
  using base = B;
  static constexpr Exp exponent{Num, Den...};
};

// ============================================================================
// Per - Denominator terms
// ============================================================================

template <typename... Ts>
struct Per final {};

// ============================================================================
// Dimension - Forward declaration
// ============================================================================

template <typename... Terms>
struct Dimension;

// ============================================================================
// Type traits
// ============================================================================

template <typename T>
constexpr bool kIsPower = false;

template <BaseDimension B, int N, int... D>
constexpr bool kIsPower<Power<B, N, D...>> = true;

template <typename T>
constexpr bool kIsPer = false;

template <typename... Ts>
constexpr bool kIsPer<Per<Ts...>> = true;

template <typename T>
constexpr bool kIsDimension = false;

template <typename... Ts>
constexpr bool kIsDimension<Dimension<Ts...>> = true;

template <typename T>
concept DimensionType = kIsDimension<T>;

// Get base dimension from a term (base dim or Power)
template <typename T>
struct GetBase { using Type = T; };

template <BaseDimension B, int N, int... D>
struct GetBase<Power<B, N, D...>> { using Type = B; };

// Get exponent from a term
template <typename T>
constexpr Exp getExp() {
  if constexpr (BaseDimension<T>) return Exp{1};
  else return T::exponent;
}

// ============================================================================
// Internal: DimPair for computation
// ============================================================================

namespace detail {

template <BaseDimension B, Exp E>
struct DimPair {};

template <typename T, typename List>
struct Prepend;
template <typename T, typename... Ts>
struct Prepend<T, TypeList<Ts...>> { using Type = TypeList<T, Ts...>; };

// ============================================================================
// Term to DimPair conversion
// ============================================================================

template <typename T>
struct TermToPairs { using Type = TypeList<DimPair<T, Exp{1}>>; };

template <BaseDimension B, int N, int... D>
struct TermToPairs<Power<B, N, D...>> {
  using Type = TypeList<DimPair<B, Exp{N, D...}>>;
};

template <typename... Ts>
struct TermToPairs<Per<Ts...>> {
  // Collect all terms with negated exponents
  template <typename Acc, typename... Us>
  struct Collect { using Type = Acc; };

  template <typename... Acc, BaseDimension B, typename... Rest>
  struct Collect<TypeList<Acc...>, B, Rest...> {
    using Type = typename Collect<TypeList<Acc..., DimPair<B, Exp{-1}>>, Rest...>::Type;
  };

  template <typename... Acc, BaseDimension B, int N, int... D, typename... Rest>
  struct Collect<TypeList<Acc...>, Power<B, N, D...>, Rest...> {
    using Type = typename Collect<TypeList<Acc..., DimPair<B, Exp{-N, D...}>>, Rest...>::Type;
  };

  using Type = typename Collect<TypeList<>, Ts...>::Type;
};

// ============================================================================
// Merge sorted DimPair lists
// ============================================================================

template <typename L1, typename L2>
struct MergePairs;

template <typename... Bs>
struct MergePairs<TypeList<>, TypeList<Bs...>> { using Type = TypeList<Bs...>; };

template <typename... As>
struct MergePairs<TypeList<As...>, TypeList<>> { using Type = TypeList<As...>; };

template <>
struct MergePairs<TypeList<>, TypeList<>> { using Type = TypeList<>; };

template <bool IsZero, BaseDimension B, Exp Sum, typename Rest>
struct PrependIfNonZero { using Type = typename Prepend<DimPair<B, Sum>, Rest>::Type; };

template <BaseDimension B, Exp Sum, typename Rest>
struct PrependIfNonZero<true, B, Sum, Rest> { using Type = Rest; };

// B1 < B2
template <BaseDimension B1, Exp E1, typename... R1, BaseDimension B2, Exp E2, typename... R2>
  requires(B1::kSymbol < B2::kSymbol)
struct MergePairs<TypeList<DimPair<B1, E1>, R1...>, TypeList<DimPair<B2, E2>, R2...>> {
  using Type = typename Prepend<DimPair<B1, E1>,
      typename MergePairs<TypeList<R1...>, TypeList<DimPair<B2, E2>, R2...>>::Type>::Type;
};

// B1 > B2
template <BaseDimension B1, Exp E1, typename... R1, BaseDimension B2, Exp E2, typename... R2>
  requires(B2::kSymbol < B1::kSymbol)
struct MergePairs<TypeList<DimPair<B1, E1>, R1...>, TypeList<DimPair<B2, E2>, R2...>> {
  using Type = typename Prepend<DimPair<B2, E2>,
      typename MergePairs<TypeList<DimPair<B1, E1>, R1...>, TypeList<R2...>>::Type>::Type;
};

// B1 == B2
template <BaseDimension B1, Exp E1, typename... R1, BaseDimension B2, Exp E2, typename... R2>
  requires(!(B1::kSymbol < B2::kSymbol) && !(B2::kSymbol < B1::kSymbol))
struct MergePairs<TypeList<DimPair<B1, E1>, R1...>, TypeList<DimPair<B2, E2>, R2...>> {
  static constexpr Exp sum = E1 + E2;
  using rest = typename MergePairs<TypeList<R1...>, TypeList<R2...>>::Type;
  using Type = typename PrependIfNonZero<sum.num == 0, B1, sum, rest>::Type;
};

// ============================================================================
// Negate exponents (for division)
// ============================================================================

template <typename List>
struct NegatePairs;

template <>
struct NegatePairs<TypeList<>> { using Type = TypeList<>; };

template <BaseDimension B, Exp E, typename... Rest>
struct NegatePairs<TypeList<DimPair<B, E>, Rest...>> {
  using Type = typename Prepend<DimPair<B, Exp{-E.num, E.den}>,
                         typename NegatePairs<TypeList<Rest...>>::Type>::Type;
};

// ============================================================================
// Scale exponents (for pow)
// ============================================================================

template <typename List, Exp Scale>
struct ScalePairs;

template <Exp Scale>
struct ScalePairs<TypeList<>, Scale> { using Type = TypeList<>; };

template <BaseDimension B, Exp E, typename... Rest, Exp Scale>
struct ScalePairs<TypeList<DimPair<B, E>, Rest...>, Scale> {
  static constexpr Exp scaled = E * Scale;
  using rest = typename ScalePairs<TypeList<Rest...>, Scale>::Type;
  using Type = typename PrependIfNonZero<scaled.num == 0, B, scaled, rest>::Type;
};

// ============================================================================
// Convert DimPairs to external form
// ============================================================================

template <BaseDimension B, Exp E>
struct PairToTerm;

template <BaseDimension B>
struct PairToTerm<B, Exp{1}> { using Type = B; };

template <BaseDimension B, Exp E>
  requires(E.num > 1 && E.den == 1)
struct PairToTerm<B, E> { using Type = Power<B, static_cast<int>(E.num)>; };

template <BaseDimension B, Exp E>
  requires(E.num > 0 && E.den > 1)
struct PairToTerm<B, E> { using Type = Power<B, static_cast<int>(E.num), static_cast<int>(E.den)>; };

// Separate positive and negative exponents
template <typename PairList>
struct SplitPairs {
  template <typename Nums, typename Dens, typename List>
  struct Impl;

  template <typename... Nums, typename... Dens>
  struct Impl<TypeList<Nums...>, TypeList<Dens...>, TypeList<>> {
    using Numerator = TypeList<Nums...>;
    using Denominator = TypeList<Dens...>;
  };

  template <typename... Nums, typename... Dens, BaseDimension B, Exp E, typename... Rest>
    requires(E.num > 0)
  struct Impl<TypeList<Nums...>, TypeList<Dens...>, TypeList<DimPair<B, E>, Rest...>> {
    using Next = Impl<TypeList<Nums..., typename PairToTerm<B, E>::Type>, TypeList<Dens...>, TypeList<Rest...>>;
    using Numerator = typename Next::Numerator;
    using Denominator = typename Next::Denominator;
  };

  template <typename... Nums, typename... Dens, BaseDimension B, Exp E, typename... Rest>
    requires(E.num < 0)
  struct Impl<TypeList<Nums...>, TypeList<Dens...>, TypeList<DimPair<B, E>, Rest...>> {
    using Next = Impl<TypeList<Nums...>,
                      TypeList<Dens..., typename PairToTerm<B, Exp{-E.num, E.den}>::Type>,
                      TypeList<Rest...>>;
    using Numerator = typename Next::Numerator;
    using Denominator = typename Next::Denominator;
  };

  using Result = Impl<TypeList<>, TypeList<>, PairList>;
  using Numerator = typename Result::Numerator;
  using Denominator = typename Result::Denominator;
};

// Build final Dimension
template <typename Nums, typename Dens>
struct BuildDim;

template <typename... Nums>
struct BuildDim<TypeList<Nums...>, TypeList<>> {
  using Type = Dimension<Nums...>;
};

template <typename... Nums, typename... Dens>
struct BuildDim<TypeList<Nums...>, TypeList<Dens...>> {
  using Type = Dimension<Nums..., Per<Dens...>>;
};

template <typename PairList>
struct ToDimension {
  using Split = SplitPairs<PairList>;
  using Type = typename BuildDim<typename Split::Numerator, typename Split::Denominator>::Type;
};

// ============================================================================
// Convert Dimension terms to DimPairs
// ============================================================================

template <typename... Terms>
struct TermsToPairs;

template <>
struct TermsToPairs<> { using Type = TypeList<>; };

template <typename First, typename... Rest>
struct TermsToPairs<First, Rest...> {
  using FirstPairs = typename TermToPairs<First>::Type;
  using RestPairs = typename TermsToPairs<Rest...>::Type;
  using Type = typename MergePairs<FirstPairs, RestPairs>::Type;
};

}  // namespace detail

// ============================================================================
// Normalization - Convert any Dimension to canonical form
// ============================================================================
//
// Canonical form guarantees:
// - Terms sorted by base dimension symbol (lexicographic)
// - Same-base exponents combined
// - Zero-exponent terms eliminated
// - Exponent 1 → bare base dim, Exponent > 1 → Power<B, N>
// - Negative exponents wrapped in Per<...>

template <typename... Terms>
using Normalize = typename detail::ToDimension<typename detail::TermsToPairs<Terms...>::Type>::Type;

// ============================================================================
// Algebraic Operations on DimPair lists (internal representation)
// ============================================================================
//
// Three primitive operations on sorted DimPair lists:
//   Multiply - merge two lists, combining same-base exponents
//   Negate   - flip sign of all exponents
//   Pow      - scale all exponents
//
// Division is composed from these: divide = multiply(a, negate(b))

namespace detail {

template <typename P1, typename P2>
using MultiplyPairs = typename MergePairs<P1, P2>::Type;

template <typename P, Exp E>
using PowPairs = typename ScalePairs<P, E>::Type;

// NegatePairs already defined above

}  // namespace detail

// ============================================================================
// Dimension operations (public interface)
// ============================================================================
//
// These compose: parse to pairs → algebraic operation → render to Dimension

template <typename D1, typename D2>
struct DimMultiplyImpl;

template <typename... T1, typename... T2>
struct DimMultiplyImpl<Dimension<T1...>, Dimension<T2...>> {
  using Pairs1 = typename detail::TermsToPairs<T1...>::Type;
  using Pairs2 = typename detail::TermsToPairs<T2...>::Type;
  using Result = detail::MultiplyPairs<Pairs1, Pairs2>;
  using Type = typename detail::ToDimension<Result>::Type;
};

template <typename D1, typename D2>
using DimMultiply = typename DimMultiplyImpl<D1, D2>::Type;

template <typename D1, typename D2>
struct DimDivideImpl;

template <typename... T1, typename... T2>
struct DimDivideImpl<Dimension<T1...>, Dimension<T2...>> {
  using Pairs1 = typename detail::TermsToPairs<T1...>::Type;
  using Pairs2 = typename detail::TermsToPairs<T2...>::Type;
  // divide = multiply(a, negate(b))
  using Result = detail::MultiplyPairs<Pairs1, typename detail::NegatePairs<Pairs2>::Type>;
  using Type = typename detail::ToDimension<Result>::Type;
};

template <typename D1, typename D2>
using DimDivide = typename DimDivideImpl<D1, D2>::Type;

template <typename D, Exp E>
struct DimPowImpl;

template <typename... Ts, Exp E>
struct DimPowImpl<Dimension<Ts...>, E> {
  using Pairs = typename detail::TermsToPairs<Ts...>::Type;
  using Result = detail::PowPairs<Pairs, E>;
  using Type = typename detail::ToDimension<Result>::Type;
};

template <typename D, Exp E>
using DimPow = typename DimPowImpl<D, E>::Type;

// ============================================================================
// Dimension - Main type
// ============================================================================

template <typename... Terms>
struct Dimension final {
  static constexpr bool isDimensionless() { return sizeof...(Terms) == 0; }

  template <DimensionType Other>
  using multiply = DimMultiply<Dimension, Other>;

  template <DimensionType Other>
  using divide = DimDivide<Dimension, Other>;

  template <Exp E>
  using pow = DimPow<Dimension, E>;

  using inverse = pow<Exp{-1}>;
  using sqrt = pow<Exp{1, 2}>;
  using cbrt = pow<Exp{1, 3}>;
};

// ============================================================================
// Dimensionless
// ============================================================================

using Dimensionless = Dimension<>;

// ============================================================================
// SI Base Dimensions
// ============================================================================

using Length = Dimension<DimLength>;
using Time = Dimension<DimTime>;
using Mass = Dimension<DimMass>;
using Current = Dimension<DimCurrent>;
using Temperature = Dimension<DimTemperature>;
using Amount = Dimension<DimAmount>;
using Luminosity = Dimension<DimLuminosity>;

// ============================================================================
// Common Derived Dimensions
// ============================================================================

using Area = Length::pow<Exp{2}>;
using Volume = Length::pow<Exp{3}>;
using Frequency = Time::inverse;
using Velocity = Length::divide<Time>;
using Acceleration = Velocity::divide<Time>;
using Force = Mass::multiply<Acceleration>;
using Energy = Force::multiply<Length>;
using PowerDim = Energy::divide<Time>;  // "Power" conflicts with Power<B,N> template
using Pressure = Force::divide<Area>;
using Momentum = Mass::multiply<Velocity>;

}  // namespace tempura
