#pragma once

#include <type_traits>

// ============================================================================
// structure.h - Compile-time matrix structure tags
// ============================================================================
//
// Structure tags encode what we know about a matrix at compile time. Operations
// preserve or transform structure via type-level dispatch — symmetric matrices
// stay symmetric under transpose, triangular matrices flip orientation, etc.
//
// The compiler tracks structure through chains of operations, enabling:
//   - Eliminating redundant transposes (symmetric → identity)
//   - Selecting optimal algorithms (Cholesky for PositiveDefinite)
//   - Catching structural violations at compile time
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Structure Tags
// ============================================================================

struct General {};            // No structural constraints
struct Symmetric {};          // A = Aᵀ
struct LowerTriangular {};    // A_{ij} = 0 for j > i
struct UpperTriangular {};    // A_{ij} = 0 for j < i
struct Diagonal {};           // A_{ij} = 0 for i ≠ j
struct PositiveDefinite {};   // Symmetric + all eigenvalues > 0

// ============================================================================
// Structure Traits
// ============================================================================

// Does this structure imply symmetry?
// Diagonal and PositiveDefinite matrices are always symmetric.
template <typename S>
constexpr bool is_symmetric_v = false;

template <>
constexpr bool is_symmetric_v<Symmetric> = true;

template <>
constexpr bool is_symmetric_v<PositiveDefinite> = true;

template <>
constexpr bool is_symmetric_v<Diagonal> = true;

// Is this a triangular structure?
template <typename S>
constexpr bool is_triangular_v = false;

template <>
constexpr bool is_triangular_v<LowerTriangular> = true;

template <>
constexpr bool is_triangular_v<UpperTriangular> = true;

// ============================================================================
// Structure Transformation Rules
// ============================================================================
//
// When we transpose a matrix, its structure changes predictably:
//   Symmetric       → Symmetric       (Aᵀ = A)
//   PositiveDefinite → PositiveDefinite (ditto)
//   Diagonal        → Diagonal        (ditto)
//   LowerTriangular → UpperTriangular (flip)
//   UpperTriangular → LowerTriangular (flip)
//   General         → General         (no info gained)

template <typename S>
struct TransposeStructure {
  using type = General;
};

template <>
struct TransposeStructure<Symmetric> {
  using type = Symmetric;
};

template <>
struct TransposeStructure<PositiveDefinite> {
  using type = PositiveDefinite;
};

template <>
struct TransposeStructure<Diagonal> {
  using type = Diagonal;
};

template <>
struct TransposeStructure<LowerTriangular> {
  using type = UpperTriangular;
};

template <>
struct TransposeStructure<UpperTriangular> {
  using type = LowerTriangular;
};

template <typename S>
using transpose_structure_t = typename TransposeStructure<S>::type;

}  // namespace tempura::symbolic4
