#pragma once

#include "symbolic4/core.h"
#include "symbolic4/indexed/data.h"
#include "symbolic4/indexed/dim.h"

// ============================================================================
// terminals.h - Domain-independent terminal dispatch for evaluation
// ============================================================================
//
// BaseTerminals handles all core terminal types: Constants, Fractions, Literals,
// Free atoms, IndexedSymbol, and IndexedData. It knows nothing about
// probabilistic effects (Sample, IndexedSample) or constraint transforms.
//
// Domain-specific extensions (e.g., ProbTerminals) handle their own effect
// types and fall through to BaseTerminals for everything else.
//
// ============================================================================

namespace tempura::symbolic4 {

// Forward declarations for helpers defined in indexed_eval.h
namespace indexed_eval_detail {

template <typename T, typename... Binders>
auto lookupByAtomId(T term, const BinderPack<Binders...>& pack) -> double;

}  // namespace indexed_eval_detail

// ============================================================================
// BaseTerminals - Domain-independent terminal handler
// ============================================================================
//
// Handles: Constant, Fraction, Literal, IndexedSymbol, IndexedData, Free atoms.
// Does NOT handle: Sample, IndexedSample, or any probabilistic effects.
//
// The Ctx type must have:
//   - ctx.scalars  (BinderPack of scalar bindings)
//   - ctx.indexed  (BinderPack of indexed bindings)
//   - ctx.dim_indices (DimIndexMap)
//
// The Interp type must have:
//   - Interp::lookupIndexedSymbol<Sym>(sym, ctx) for indexed lookups

struct BaseTerminals {
  template <typename Interp, typename T, typename Ctx>
  static auto eval(T term, Ctx& ctx) -> double {
    if constexpr (is_constant_v<T>) {
      return static_cast<double>(T::value);
    } else if constexpr (is_fraction_v<T>) {
      return T::value;
    } else if constexpr (is_literal_v<T>) {
      return static_cast<double>(term.effect_.value);
    } else if constexpr (is_indexed_symbol_v<T>) {
      return Interp::template lookupIndexedSymbol<T>(term, ctx);
    } else if constexpr (is_indexed_data_v<T>) {
      using SymType = typename T::symbol_type;
      return Interp::template lookupIndexedSymbol<SymType>(SymType{}, ctx);
    } else if constexpr (is_atom_v<T>) {
      // Free atom or any other atom - look up by Id in scalar bindings
      return indexed_eval_detail::lookupByAtomId(term, ctx.scalars);
    } else {
      static_assert(is_atom_v<T>, "Unknown terminal type");
      return 0.0;
    }
  }
};

}  // namespace tempura::symbolic4
