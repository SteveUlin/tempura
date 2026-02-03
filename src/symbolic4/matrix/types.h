#pragma once

#include <type_traits>

#include "symbolic4/core.h"

// ============================================================================
// types.h - Matrix core types for dimension-tagged vectors and Cholesky factors
// ============================================================================
//
// Provides matrix-valued symbols whose dimensions are determined by dimension
// tags rather than compile-time constants. This enables matrix sizes to be
// specified at model construction time (like plate dimensions).
//
// Core types:
//   DimVectorSymbol<Id, DimTag>   - Vector of length |DimTag|
//   CholeskyCovSymbol<Id, DimTag> - Lower triangular Cholesky factor of covariance
//   CholeskyCorrSymbol<Id, DimTag> - Cholesky factor with unit diagonal (correlation)
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// DimVectorSymbol - A vector whose dimension is determined by a tag
// ============================================================================
//
// Unlike VectorSymbol<Id, D> which has compile-time dimension D, this symbol's
// dimension comes from a DimTag whose size is known at model construction time.
//
// Example:
//   struct Countries;
//   auto mu = dimVector<Countries>();  // μ ∈ ℝ^N where N = |Countries|

template <typename Id, typename DimTag>
struct DimVectorSymbol : SymbolicTag {
  using id_type = Id;
  using dim_tag = DimTag;
};

// ============================================================================
// CholeskyCovSymbol - Cholesky factor of a covariance matrix
// ============================================================================
//
// Represents a K×K lower triangular matrix L where Σ = LLᵀ is a covariance
// matrix. The diagonal elements must be positive.
//
// Parameters: K*(K+1)/2 (full lower triangle)

template <typename Id, typename DimTag>
struct CholeskyCovSymbol : SymbolicTag {
  using id_type = Id;
  using dim_tag = DimTag;
  static constexpr bool is_correlation = false;
};

// ============================================================================
// CholeskyCorrSymbol - Cholesky factor of a correlation matrix
// ============================================================================
//
// Represents a K×K lower triangular matrix L where Ω = LLᵀ is a correlation
// matrix (unit diagonal). The Cholesky factor has unit row norms.
//
// Parameters: K*(K-1)/2 (off-diagonal elements only; diagonal is determined)

template <typename Id, typename DimTag>
struct CholeskyCorrSymbol : SymbolicTag {
  using id_type = Id;
  using dim_tag = DimTag;
  static constexpr bool is_correlation = true;
};

// ============================================================================
// Type Traits
// ============================================================================

// Is this a DimVectorSymbol?
template <typename T>
struct IsDimVectorSymbol : std::false_type {};

template <typename Id, typename DimTag>
struct IsDimVectorSymbol<DimVectorSymbol<Id, DimTag>> : std::true_type {};

template <typename T>
constexpr bool is_dim_vector_symbol_v = IsDimVectorSymbol<T>::value;

// Is this any Cholesky symbol (cov or corr)?
template <typename T>
struct IsCholeskySymbol : std::false_type {};

template <typename Id, typename DimTag>
struct IsCholeskySymbol<CholeskyCovSymbol<Id, DimTag>> : std::true_type {};

template <typename Id, typename DimTag>
struct IsCholeskySymbol<CholeskyCorrSymbol<Id, DimTag>> : std::true_type {};

template <typename T>
constexpr bool is_cholesky_symbol_v = IsCholeskySymbol<T>::value;

// Is this specifically a CholeskyCorrSymbol?
template <typename T>
struct IsCholeskyCorr : std::false_type {};

template <typename Id, typename DimTag>
struct IsCholeskyCorr<CholeskyCorrSymbol<Id, DimTag>> : std::true_type {};

template <typename T>
constexpr bool is_cholesky_corr_v = IsCholeskyCorr<T>::value;

// ============================================================================
// Accessors
// ============================================================================

// Get the DimTag from a dimension-tagged symbol
template <typename T>
struct GetDimTag;

template <typename Id, typename DimTag>
struct GetDimTag<DimVectorSymbol<Id, DimTag>> {
  using type = DimTag;
};

template <typename Id, typename DimTag>
struct GetDimTag<CholeskyCovSymbol<Id, DimTag>> {
  using type = DimTag;
};

template <typename Id, typename DimTag>
struct GetDimTag<CholeskyCorrSymbol<Id, DimTag>> {
  using type = DimTag;
};

template <typename T>
using get_dim_tag_t = typename GetDimTag<T>::type;

// ============================================================================
// Factory Functions
// ============================================================================

// Create a DimVectorSymbol with unique Id
template <typename DimTag, typename Id = decltype([] {})>
constexpr auto dimVector() {
  return DimVectorSymbol<Id, DimTag>{};
}

// Create a CholeskyCovSymbol with unique Id
template <typename DimTag, typename Id = decltype([] {})>
constexpr auto cholCov() {
  return CholeskyCovSymbol<Id, DimTag>{};
}

// Create a CholeskyCorrSymbol with unique Id
template <typename DimTag, typename Id = decltype([] {})>
constexpr auto cholCorr() {
  return CholeskyCorrSymbol<Id, DimTag>{};
}

}  // namespace tempura::symbolic4
