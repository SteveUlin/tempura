#pragma once

#include "symbolic4/compressed.h"

// ============================================================================
// dist_base.h — CRTP base for distribution wrappers
// ============================================================================
//
// Factored out of wrappers.h to eliminate ~40 lines of boilerplate per
// distribution. Each distribution provides:
//   - A support_type alias
//   - logProbImpl(x, params...) and unnormalizedLogProbImpl(x, params...)
//   - Named param accessors (mu(), sigma(), etc.)
//
// The base provides:
//   - CompressedTuple<Params...> storage with [[no_unique_address]]
//   - logProbFor(x) / unnormalizedLogProbFor(x) that unpack and delegate
//   - Default constructor and parameter constructor
//
// The CompressedTuple ensures sizeof(NormalDist<A,B>) == sizeof(Pair<A,B>)
// when A and B are empty symbolic types (which they usually are).

namespace tempura::symbolic4 {

template <typename Derived, typename Support, Symbolic... Params>
struct DistBase {
  using support_type = Support;
  static constexpr SizeT param_count = sizeof...(Params);

  [[no_unique_address]] CompressedTuple<Params...> params_;

  constexpr DistBase() = default;
  constexpr DistBase(Params... ps) : params_{ps...} {}

  template <Symbolic X>
  constexpr auto logProbFor(X x) const {
    return [&]<SizeT... Is>(IndexSequence<Is...>) {
      return static_cast<const Derived*>(this)->logProbImpl(x, get<Is>(params_)...);
    }(MakeIndexSequence<sizeof...(Params)>{});
  }

  template <Symbolic X>
  constexpr auto unnormalizedLogProbFor(X x) const {
    return [&]<SizeT... Is>(IndexSequence<Is...>) {
      return static_cast<const Derived*>(this)->unnormalizedLogProbImpl(
          x, get<Is>(params_)...);
    }(MakeIndexSequence<sizeof...(Params)>{});
  }

 protected:
  // Named parameter access for derived classes
  template <SizeT I>
  constexpr auto param() const {
    return get<I>(params_);
  }
};

}  // namespace tempura::symbolic4
