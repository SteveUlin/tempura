#pragma once

#include "symbolic4/constraints.h"
#include "symbolic4/distributions/effects.h"
#include "symbolic4/distributions/indexed_node.h"  // For make_indexed_symbol_t
#include "symbolic4/terminals.h"

// ============================================================================
// prob_terminals.h - Probabilistic terminal dispatch for evaluation
// ============================================================================
//
// Extends BaseTerminals with handling for probabilistic effect types:
//   - Atom<Id, Sample<D>>: look up by Id, apply constraint transform
//   - Atom<Id, IndexedSample<D, DimsList>>: look up via IndexedSymbol
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
      // Sample atom: Atom<Id, Sample<Dist>>
      // Look up by Free symbol (Atom<Id, Free>) and apply constraint transform
      double z = indexed_eval_detail::lookupByAtomId(term, ctx.scalars);

      // Apply constraint transform based on distribution support
      using Dist = get_distribution_t<typename T::effect_type>;
      using Support = typename Dist::support_type;
      return constraints::applyNumeric<Support>(z);
    } else {
      return BaseTerminals::eval<Interp>(term, ctx);
    }
  }
};

}  // namespace tempura::symbolic4
