#pragma once

#include "symbolic4/distributions/effects.h"
#include "symbolic4/distributions/indexed_node.h"  // For make_indexed_symbol_t
#include "symbolic4/terminals.h"

// ============================================================================
// prob_terminals.h - Probabilistic terminal dispatch for evaluation
// ============================================================================
//
// Extends BaseTerminals with handling for probabilistic effect types:
//   - Atom<Id, Sample<D>>: look up by Id via corresponding Free symbol
//   - Atom<Id, IndexedSample<D, DimsList>>: look up via IndexedSymbol
//
// No constraint transforms here — the posterior's TransformPack pre-transforms
// unconstrained→constrained before binding, so values are already constrained.
//
// Falls through to BaseTerminals for all other terminal types.
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// ProbTerminals - Probabilistic terminal handler
// ============================================================================

struct ProbTerminals {
  template <typename Interp, typename T, typename Ctx>
  static auto eval(T term, Ctx& ctx) -> double {
    if constexpr (is_indexed_random_var_atom_v<T>) {
      // Atom<Id, IndexedSample<Dist, DimsList>> - look up via IndexedSymbol
      using DimsList = typename T::effect_type::dims_list;
      using IdType = typename T::id_type;
      using LookupSym = detail::make_indexed_symbol_t<IdType, DimsList>;
      return Interp::template lookupIndexedSymbol<LookupSym>(LookupSym{}, ctx);
    } else if constexpr (is_random_var_atom_v<T>) {
      // Sample atom: look up by constructing the corresponding Free symbol.
      // No constraint transform — TransformPack handles that externally.
      using FreeSymbol = Atom<get_id_t<T>, Free>;
      return ctx.scalars[FreeSymbol{}];
    } else {
      return BaseTerminals::eval<Interp>(term, ctx);
    }
  }
};

}  // namespace tempura::symbolic4
