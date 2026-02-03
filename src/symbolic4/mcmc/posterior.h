#pragma once

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

#include "symbolic4/core.h"
#include "symbolic4/distributions/joint.h"
#include "symbolic4/distributions/random_var.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/interpreter/eval.h"
#include "symbolic4/interpreter/simplify.h"

// ============================================================================
// posterior.h - Posterior wrapper for MCMC inference
// ============================================================================
//
// Provides a convenient interface for computing log-probabilities and gradients
// for use with MCMC samplers (HMC, NUTS, etc.)
//
// Usage:
//   // Define model
//   auto mu = normal(lit(0.0), lit(5.0));
//   auto sigma = halfNormal(lit(2.0));
//   auto y = normal(mu, sigma);
//
//   // Create posterior conditioned on observed y
//   auto posterior = makePosterior(
//       logProb(mu, sigma, y),  // joint log-prob
//       mu, sigma               // parameters to infer (order matters!)
//   ).observe(y = 3.5);         // observed data
//
//   // Evaluate log-prob and gradient
//   double lp = posterior.logProb(1.0, 2.0);  // mu=1.0, sigma=2.0
//   auto grad = posterior.gradient(1.0, 2.0); // [d/dmu, d/dsigma]
//
// ============================================================================

namespace tempura::symbolic4 {

// ============================================================================
// Posterior - Wrapper for log-prob and gradients
// ============================================================================

template <Symbolic LogProbExpr, typename ObsBindings, typename ParamSymbolsTuple,
          typename GradExprsTuple>
class Posterior {
 public:
  static constexpr std::size_t NumParams = std::tuple_size_v<ParamSymbolsTuple>;
  using GradArray = std::array<double, NumParams>;

  constexpr Posterior(LogProbExpr lp, ObsBindings obs, ParamSymbolsTuple params,
                      GradExprsTuple grads)
      : log_prob_expr_{lp},
        observations_{obs},
        param_symbols_{params},
        grad_exprs_{grads} {}

  // Evaluate log-probability at given parameter values
  template <typename... ParamValues>
    requires(sizeof...(ParamValues) == NumParams)
  auto logProb(ParamValues... params) const -> double {
    return logProbImpl(std::make_index_sequence<NumParams>{}, params...);
  }

  // Evaluate gradient at given parameter values
  template <typename... ParamValues>
    requires(sizeof...(ParamValues) == NumParams)
  auto gradient(ParamValues... params) const -> GradArray {
    return gradientImpl(std::make_index_sequence<NumParams>{}, params...);
  }

 private:
  LogProbExpr log_prob_expr_;
  ObsBindings observations_;
  ParamSymbolsTuple param_symbols_;
  GradExprsTuple grad_exprs_;

  template <std::size_t... Is, typename... ParamValues>
  auto logProbImpl(std::index_sequence<Is...>, ParamValues... params) const -> double {
    std::array<double, NumParams> arr{static_cast<double>(params)...};
    auto bindings = mergeBindings(
        BinderPack{(std::get<Is>(param_symbols_) = arr[Is])...},
        observations_);
    return evaluate(log_prob_expr_, bindings);
  }

  template <std::size_t... Is, typename... ParamValues>
  auto gradientImpl(std::index_sequence<Is...>, ParamValues... params) const -> GradArray {
    std::array<double, NumParams> arr{static_cast<double>(params)...};
    auto bindings = mergeBindings(
        BinderPack{(std::get<Is>(param_symbols_) = arr[Is])...},
        observations_);
    return GradArray{evaluate(std::get<Is>(grad_exprs_), bindings)...};
  }

  // Merge two BinderPacks
  template <typename... PB, typename... Obs>
  static auto mergeBindings(BinderPack<PB...> pb,
                            [[maybe_unused]] BinderPack<Obs...> obs) {
    return BinderPack{static_cast<PB&>(pb)..., static_cast<Obs&>(obs)...};
  }
};

// ============================================================================
// PosteriorBuilder - Fluent builder for Posterior
// ============================================================================

template <Symbolic LogProbExpr, typename ParamSymbolsTuple, typename GradExprsTuple>
class PosteriorBuilder {
 public:
  static constexpr std::size_t NumParams = std::tuple_size_v<ParamSymbolsTuple>;

  constexpr PosteriorBuilder(LogProbExpr lp, ParamSymbolsTuple params, GradExprsTuple grads)
      : log_prob_expr_{lp}, param_symbols_{params}, grad_exprs_{grads} {}

  // Add observations
  template <typename... Binders>
  auto observe(Binders... binders) const {
    auto obs = BinderPack{binders...};
    return Posterior<LogProbExpr, BinderPack<Binders...>, ParamSymbolsTuple,
                     GradExprsTuple>{log_prob_expr_, obs, param_symbols_, grad_exprs_};
  }

  // Create posterior without observations (prior sampling)
  auto build() const {
    return Posterior<LogProbExpr, BinderPack<>, ParamSymbolsTuple, GradExprsTuple>{
        log_prob_expr_, BinderPack<>{}, param_symbols_, grad_exprs_};
  }

 private:
  LogProbExpr log_prob_expr_;
  ParamSymbolsTuple param_symbols_;
  GradExprsTuple grad_exprs_;
};

// ============================================================================
// Factory function
// ============================================================================

namespace detail {

template <Symbolic LogProbExpr, typename ParamsTuple, std::size_t... Is>
auto makeGradExprs(LogProbExpr lp, ParamsTuple params, std::index_sequence<Is...>) {
  return std::make_tuple(simplify(diff(lp, std::get<Is>(params)))...);
}

}  // namespace detail

// Create a posterior builder from log-prob expression and parameter symbols
// Parameters can be RandomVars (their symbols are extracted) or raw Symbols
template <Symbolic LogProbExpr, typename... Params>
constexpr auto makePosterior(LogProbExpr lp, Params... params) {
  // Extract constrained param symbols from params for bindings.
  // RandomVar: use ConstrainedParamSymbol (applies inverse on bind)
  // Symbol: use as-is (already Atom<Id, Free>)
  auto extractBindSym = [](auto p) {
    if constexpr (IsRandomVar<decltype(p)>) {
      using Id = typename decltype(p)::id_type;
      using Support = typename decltype(p)::support_type;
      return ConstrainedParamSymbol<Id, Support>{};
    } else {
      return p;
    }
  };

  // For differentiation, we still need the raw Free symbols
  auto extractDiffSym = [](auto p) {
    if constexpr (IsRandomVar<decltype(p)>) {
      return p.freeSym();
    } else {
      return p;
    }
  };

  auto param_symbols = std::make_tuple(extractBindSym(params)...);
  auto diff_symbols = std::make_tuple(extractDiffSym(params)...);
  auto grad_exprs = detail::makeGradExprs(
      lp, diff_symbols, std::make_index_sequence<sizeof...(Params)>{});

  return PosteriorBuilder<LogProbExpr, decltype(param_symbols), decltype(grad_exprs)>{
      lp, param_symbols, grad_exprs};
}

}  // namespace tempura::symbolic4
