#pragma once

#include "units/unit_type.h"

// DerivedUnit - Compound units that preserve structure
//
// Instead of computing km/h as an anonymous unit with magnitude 5/18,
// DerivedUnit preserves the structure: DerivedUnit<Kilometre, Per<Hour>>
//
// This enables:
// - Meaningful symbol generation: "km/h" instead of ""
// - Unit simplification: m/m -> One
// - Better type safety and debugging

namespace tempura {

// ============================================================================
// UnitPower - Unit raised to a power
// ============================================================================

template <UnitType U, int Num, int... Den>
  requires(Num > 0 && !(Num == 1 && sizeof...(Den) == 0))
struct UnitPower {
  using BaseUnit = U;
  static constexpr Exp exponent{Num, Den...};
};

// ============================================================================
// Per - Denominator marker for units (reusing concept from dimensions)
// ============================================================================

// Per is already defined in dimension.h, but we need a unit-specific version
// to avoid confusion. We'll use the same name but with UnitType constraints.

template <typename... Us>
struct UnitPer {};

// ============================================================================
// Type traits for derived unit components
// ============================================================================

template <typename T>
constexpr bool kIsUnitPower = false;

template <UnitType U, int N, int... D>
constexpr bool kIsUnitPower<UnitPower<U, N, D...>> = true;

template <typename T>
constexpr bool kIsUnitPer = false;

template <typename... Us>
constexpr bool kIsUnitPer<UnitPer<Us...>> = true;

// ============================================================================
// DerivedUnit - Compound unit preserving structure
// ============================================================================

template <typename... Terms>
struct DerivedUnit;

// ============================================================================
// Get the QuantitySpec for a derived unit
// ============================================================================

namespace detail {

// Get base unit from a term
template <typename T>
struct GetBaseUnit { using Type = T; };

template <UnitType U, int N, int... D>
struct GetBaseUnit<UnitPower<U, N, D...>> { using Type = U; };

// Get exponent from a unit term
template <typename T>
constexpr Exp getUnitExp() {
  if constexpr (UnitType<T>) return Exp{1};
  else if constexpr (kIsUnitPower<T>) return T::exponent;
  else return Exp{1};
}

// Compute magnitude for a single term
template <typename T>
constexpr double termMagnitude() {
  if constexpr (UnitType<T>) {
    return T::kMagnitude;
  } else if constexpr (kIsUnitPower<T>) {
    // U^(n/d) magnitude = (U::kMagnitude)^(n/d)
    // For simplicity, only handle integer exponents for now
    constexpr Exp e = T::exponent;
    static_assert(e.den == 1, "Fractional unit exponents not yet supported");
    double result = 1.0;
    for (int i = 0; i < e.num; ++i) {
      result = result * T::BaseUnit::kMagnitude;
    }
    return result;
  } else {
    return 1.0;
  }
}

// Compute QuantitySpec for a single term
template <typename T>
struct TermSpec {
  using Type = typename T::QuantitySpec;
};

template <UnitType U, int N, int... D>
struct TermSpec<UnitPower<U, N, D...>> {
  using BaseSpec = typename U::QuantitySpec;
  using Type = QtyPow<BaseSpec, Exp{N, D...}>;
};

// Compute type-level magnitude for a single term
template <typename T>
struct TermMagnitudeType {
  using Type = typename T::Magnitude;
};

template <UnitType U, int N, int... D>
struct TermMagnitudeType<UnitPower<U, N, D...>> {
  // U^(N/D) magnitude type = Mag^(N/D)
  static constexpr int kDen = sizeof...(D) == 0 ? 1 : []{ int arr[] = {D...}; return arr[0]; }();
  using Type = MagPow<typename U::Magnitude, N, kDen>;
};

// Compute combined type-level magnitude for all terms
template <typename... Terms>
struct CombinedMagnitudeType;

template <>
struct CombinedMagnitudeType<> {
  using Type = MagOne;
};

template <typename First>
struct CombinedMagnitudeType<First> {
  using Type = typename TermMagnitudeType<First>::Type;
};

template <typename First, typename Second, typename... Rest>
struct CombinedMagnitudeType<First, Second, Rest...> {
  using FirstMag = typename TermMagnitudeType<First>::Type;
  using RestMag = typename CombinedMagnitudeType<Second, Rest...>::Type;
  using Type = MagMultiply<FirstMag, RestMag>;
};

template <typename... Us>
struct CombinedMagnitudeType<UnitPer<Us...>> {
  using PerMag = typename CombinedMagnitudeType<Us...>::Type;
  using Type = MagInverse<PerMag>;
};

template <typename... Us, typename... Rest>
struct CombinedMagnitudeType<UnitPer<Us...>, Rest...> {
  using PerMag = typename CombinedMagnitudeType<Us...>::Type;
  using RestMag = typename CombinedMagnitudeType<Rest...>::Type;
  using Type = MagMultiply<MagInverse<PerMag>, RestMag>;
};

template <typename First, typename... Us, typename... Rest>
struct CombinedMagnitudeType<First, UnitPer<Us...>, Rest...> {
  using FirstMag = typename TermMagnitudeType<First>::Type;
  using PerMag = typename CombinedMagnitudeType<Us...>::Type;
  using RestMag = typename CombinedMagnitudeType<Rest...>::Type;
  using Type = MagMultiply<MagMultiply<FirstMag, MagInverse<PerMag>>, RestMag>;
};

// Compute combined magnitude for all terms (runtime value)
template <typename... Terms>
struct CombinedMagnitude;

template <>
struct CombinedMagnitude<> {
  static constexpr double value = 1.0;
};

template <typename First, typename... Rest>
struct CombinedMagnitude<First, Rest...> {
  static constexpr double value = termMagnitude<First>() * CombinedMagnitude<Rest...>::value;
};

template <typename... Us, typename... Rest>
struct CombinedMagnitude<UnitPer<Us...>, Rest...> {
  // Per terms contribute inverse magnitude
  static constexpr double perMag = CombinedMagnitude<Us...>::value;
  static constexpr double value = 1.0 / perMag * CombinedMagnitude<Rest...>::value;
};

// Compute combined QuantitySpec for all terms
template <typename... Terms>
struct CombinedSpec;

template <>
struct CombinedSpec<> {
  using Type = QtyDimensionless;
};

template <typename First>
struct CombinedSpec<First> {
  using Type = typename TermSpec<First>::Type;
};

template <typename First, typename Second, typename... Rest>
struct CombinedSpec<First, Second, Rest...> {
  using FirstSpec = typename TermSpec<First>::Type;
  using RestSpec = typename CombinedSpec<Second, Rest...>::Type;
  using Type = QtyMultiply<FirstSpec, RestSpec>;
};

template <typename... Us>
struct CombinedSpec<UnitPer<Us...>> {
  using PerSpec = typename CombinedSpec<Us...>::Type;
  using Type = QtyInverse<PerSpec>;
};

template <typename... Us, typename... Rest>
struct CombinedSpec<UnitPer<Us...>, Rest...> {
  using PerSpec = typename CombinedSpec<Us...>::Type;
  using RestSpec = typename CombinedSpec<Rest...>::Type;
  using Type = QtyMultiply<QtyInverse<PerSpec>, RestSpec>;
};

template <typename First, typename... Us, typename... Rest>
struct CombinedSpec<First, UnitPer<Us...>, Rest...> {
  using FirstSpec = typename TermSpec<First>::Type;
  using PerSpec = typename CombinedSpec<Us...>::Type;
  using RestSpec = typename CombinedSpec<Rest...>::Type;
  using Type = QtyMultiply<QtyMultiply<FirstSpec, QtyInverse<PerSpec>>, RestSpec>;
};

// Build symbol for a single term
template <typename T>
constexpr auto termSymbol() {
  if constexpr (UnitType<T>) {
    return T::kSymbol;
  } else if constexpr (kIsUnitPower<T>) {
    // U^n -> "symbol^n" or "symbol2", "symbol3" for common cases
    constexpr Exp e = T::exponent;
    static_assert(e.den == 1, "Fractional exponents in symbols not supported");
    if constexpr (e.num == 2) {
      return T::BaseUnit::kSymbol + FixedString{"2"};
    } else if constexpr (e.num == 3) {
      return T::BaseUnit::kSymbol + FixedString{"3"};
    } else if constexpr (e.num == 4) {
      return T::BaseUnit::kSymbol + FixedString{"4"};
    } else {
      // For other exponents, use ^n notation
      return T::BaseUnit::kSymbol + FixedString{"^"} + FixedString{"n"};
    }
  } else {
    return FixedString{""};
  }
}

// Build symbol for multiple denominator units
template <typename... Us>
struct PerSymbolBuilder;

template <>
struct PerSymbolBuilder<> {
  static constexpr auto build() { return FixedString{""}; }
};

template <typename First>
struct PerSymbolBuilder<First> {
  static constexpr auto build() { return termSymbol<First>(); }
};

template <typename First, typename Second, typename... Rest>
struct PerSymbolBuilder<First, Second, Rest...> {
  static constexpr auto build() {
    return termSymbol<First>() + FixedString{"·"} +
           PerSymbolBuilder<Second, Rest...>::build();
  }
};

// Extract and build symbol from UnitPer<Us...>
template <typename T>
struct PerContentsSymbol;

template <typename... Us>
struct PerContentsSymbol<UnitPer<Us...>> {
  static constexpr auto build() { return PerSymbolBuilder<Us...>::build(); }
};

// Build complete symbol from terms
template <typename... Terms>
struct SymbolBuilder;

template <>
struct SymbolBuilder<> {
  static constexpr auto build() { return FixedString{""}; }
};

template <typename First>
struct SymbolBuilder<First> {
  static constexpr auto build() {
    if constexpr (kIsUnitPer<First>) {
      // Per alone means inverse: 1/x
      return FixedString{"1/"} + PerContentsSymbol<First>::build();
    } else {
      return termSymbol<First>();
    }
  }
};

// Two or more terms
template <typename First, typename Second, typename... Rest>
struct SymbolBuilder<First, Second, Rest...> {
  static constexpr auto build() {
    if constexpr (kIsUnitPer<First>) {
      // Per at start (shouldn't happen normally)
      return FixedString{"1/"} + PerContentsSymbol<First>::build() +
             SymbolBuilder<Second, Rest...>::build();
    } else if constexpr (kIsUnitPer<Second>) {
      // Numerator followed by Per
      if constexpr (sizeof...(Rest) == 0) {
        return termSymbol<First>() + FixedString{"/"} +
               PerContentsSymbol<Second>::build();
      } else {
        return termSymbol<First>() + FixedString{"/"} +
               PerContentsSymbol<Second>::build() + FixedString{"·"} +
               SymbolBuilder<Rest...>::build();
      }
    } else {
      // Multiple numerator terms
      return termSymbol<First>() + FixedString{"·"} +
             SymbolBuilder<Second, Rest...>::build();
    }
  }
};

// Build symbol string
template <typename... Terms>
constexpr auto buildSymbol() {
  if constexpr (sizeof...(Terms) == 0) {
    return FixedString{"1"};
  } else {
    return SymbolBuilder<Terms...>::build();
  }
}

}  // namespace detail

// ============================================================================
// DerivedUnit implementation
// ============================================================================

template <typename... Terms>
struct DerivedUnit {
  using QuantitySpec = typename detail::CombinedSpec<Terms...>::Type;
  using Magnitude = typename detail::CombinedMagnitudeType<Terms...>::Type;
  static constexpr double kMagnitude = Magnitude::value();
  static constexpr auto kSymbol = detail::buildSymbol<Terms...>();

  // Store the terms for introspection
  using NumeratorTerms = TypeList<Terms...>;
};

// DerivedUnit satisfies UnitType
template <typename... Terms>
constexpr bool kIsDerivedUnit = false;

template <typename... Terms>
constexpr bool kIsDerivedUnit<DerivedUnit<Terms...>> = true;

// ============================================================================
// Unit algebra - multiply/divide units to produce derived units
// ============================================================================

// Unit * Unit
template <UnitType U1, UnitType U2>
using UnitMultiply = DerivedUnit<U1, U2>;

// Unit / Unit
template <UnitType U1, UnitType U2>
using UnitDivide = DerivedUnit<U1, UnitPer<U2>>;

// ============================================================================
// Simplification helpers (for future use)
// ============================================================================

// Check if two units are the same base unit
template <UnitType U1, UnitType U2>
constexpr bool kSameBaseUnit = isSame<U1, U2>;

// ============================================================================
// Common derived unit aliases
// ============================================================================

using MetrePerSecond_Derived = DerivedUnit<Metre, UnitPer<Second>>;
using KilometrePerHour_Derived = DerivedUnit<Kilometre, UnitPer<Hour>>;
using MetrePerSecondSquared_Derived = DerivedUnit<Metre, UnitPer<UnitPower<Second, 2>>>;

}  // namespace tempura
