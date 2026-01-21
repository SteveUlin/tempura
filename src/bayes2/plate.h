#pragma once

#include <cstddef>
#include <span>
#include <tuple>
#include <vector>

#include "bayes2/core.h"
#include "bayes2/traversal.h"
#include "symbolic3/core.h"
#include "symbolic3/evaluate.h"

// Plate notation for bayes2 - handling variable numbers of i.i.d. entities.
//
// PROBLEM: bayes2 uses unique types (decltype([]{})) for each RandomVar.
// This works for models with fixed structure, but what about N countries
// where N is only known at runtime?
//
// SOLUTION: Plate notation separates "model structure" from "number of instances":
//   - Model structure is compile-time: define distributions, dependencies
//   - Number of instances (N) is runtime: loop over data
//
// The key insight: we define the model for ONE generic instance, then
// instantiate it N times at runtime by looping over data vectors.
//
// DESIGN:
//   1. IndexedBinding - binds a symbol to a vector, extracts values[i] during eval
//
//   2. PlatedBinderPack - holds mixed scalar/indexed bindings, creates instance views
//
//   3. evaluatePlate - loops over instances, summing log-probs
//
// Example usage:
//   // Shared hyperparameters (NOT plated)
//   auto alpha = halfNormal(5.0);
//   auto beta = halfNormal(5.0);
//
//   // Per-country model (inside plate)
//   auto theta = bayes2::beta(alpha, beta);  // θ ~ Beta(α, β)
//
//   // Evaluation - sum over N countries
//   auto lp = evaluatePlate(theta.nodeLogProb(),
//                           n_countries,
//                           alpha = 2.0, beta = 3.0,     // scalars
//                           indexed(theta, theta_values) // indexed binding
//                          );

namespace tempura::bayes2 {

using namespace tempura::symbolic3;

// =============================================================================
// IndexedBinding - Binds symbol to vector, extracts values[i]
// =============================================================================
//
// Unlike regular bindings (symbol = scalar), IndexedBinding holds a span.
// During plate evaluation, we extract values[current_index].

template <typename Sym>
struct IndexedBinding {
  using symbol_type = Sym;
  std::span<const double> values;

  // Subscript to get value at index i
  double at(std::size_t i) const { return values[i]; }
};

// Factory functions for creating indexed bindings
// indexed(symbol, values) or indexed(graphNode, values)

template <typename Id>
auto indexed(Symbol<Id> /*sym*/, std::span<const double> values) {
  return IndexedBinding<Symbol<Id>>{values};
}

template <typename Id>
auto indexed(Symbol<Id> sym, const std::vector<double>& values) {
  return indexed(sym, std::span{values});
}

// For GraphNodes (RandomVar, DeterministicVar), use their symbol
template <GraphNode Node>
auto indexed(const Node& /*node*/, std::span<const double> values) {
  return IndexedBinding<typename Node::symbol_type>{values};
}

template <GraphNode Node>
auto indexed(const Node& node, const std::vector<double>& values) {
  return indexed(node, std::span{values});
}

// Check if a type is IndexedBinding
template <typename T>
constexpr bool is_indexed_binding = false;

template <typename Sym>
constexpr bool is_indexed_binding<IndexedBinding<Sym>> = true;

// =============================================================================
// makeInstanceBindings - Create BinderPack for a specific instance
// =============================================================================
//
// Given a mix of scalar binders and IndexedBindings, produce a BinderPack
// with all scalars for instance i (extracting indexed values at index i).

// Convert one binder to scalar for instance i
template <typename Binder>
auto toScalarBinder(const Binder& binder, std::size_t i) {
  if constexpr (is_indexed_binding<Binder>) {
    using Sym = typename Binder::symbol_type;
    return TypeValueBinder<Sym, double>{Sym{}, binder.at(i)};
  } else {
    return binder;  // Already scalar
  }
}

// Helper to build BinderPack from multiple binders for instance i
template <typename... Binders>
auto makeInstanceBindings(std::size_t i, const Binders&... binders) {
  return BinderPack{toScalarBinder(binders, i)...};
}

// =============================================================================
// evaluatePlate - Evaluate expression summing over N instances
// =============================================================================
//
// For plated models, the log-prob is: Σᵢ log p(instance_i)
// We compute the symbolic expression for ONE instance, then sum numerically.

template <Symbolic Expr, typename... Binders>
auto evaluatePlate(Expr expr, std::size_t n_instances, Binders... binders)
    -> double {
  double total = 0.0;
  for (std::size_t i = 0; i < n_instances; ++i) {
    auto instance_bindings = makeInstanceBindings(i, binders...);
    total += evaluate(expr, instance_bindings);
  }
  return total;
}

// =============================================================================
// evaluatePlateGradient - Gradient of plated expression
// =============================================================================
//
// For HMC/ADVI: ∇ Σᵢ log p(instance_i) = Σᵢ ∇ log p(instance_i)
//
// We compute ∇ log p symbolically for one instance, then sum numerically.
// This keeps symbolic autodiff while handling variable N.

template <Symbolic Expr, GraphNode Param, typename... Binders>
auto evaluatePlateGradient(Expr expr, const Param& param,
                           std::size_t n_instances, Binders... binders)
    -> double {
  auto grad_expr = diff(expr, param);
  double total_grad = 0.0;
  for (std::size_t i = 0; i < n_instances; ++i) {
    auto instance_bindings = makeInstanceBindings(i, binders...);
    total_grad += evaluate(grad_expr, instance_bindings);
  }
  return total_grad;
}

// =============================================================================
// Plate Struct (Optional) - For explicit plate grouping
// =============================================================================
//
// You can use Plate to explicitly mark which nodes are replicated.
// This is optional - you can also just use evaluatePlate directly.

template <typename Id, typename... InnerNodes>
struct Plate {
  using id_type = Id;

  std::tuple<InnerNodes...> inner_nodes_;

  // Log-prob expression for ONE instance (symbolic)
  constexpr auto instanceLogProb() const {
    return std::apply(
        [](const auto&... nodes) { return jointLogProb(nodes...); },
        inner_nodes_);
  }

  // Plate itself contributes 0 - the sum happens in evaluatePlate
  constexpr auto nodeLogProb() const { return Literal{0.0}; }

  constexpr const auto& innerNodes() const { return inner_nodes_; }
};

// Factory
template <typename Id = decltype([] {}), typename... Nodes>
constexpr auto plate(Nodes... nodes) {
  return Plate<Id, Nodes...>{{nodes...}};
}

// =============================================================================
// USAGE EXAMPLE
// =============================================================================
//
// Hierarchical model for BMJ weekend submissions:
//
//   α ~ HalfNormal(5)           // global hyperparameter
//   β ~ HalfNormal(5)           // global hyperparameter
//   for i in 1..N:
//     θ_i ~ Beta(α, β)          // per-country rate
//
// In bayes2 with plates:
//
//   auto alpha = halfNormal(5.0);
//   auto beta_param = halfNormal(5.0);
//
//   // Define the model for ONE generic instance
//   auto theta = bayes2::beta(alpha, beta_param);
//
//   // Get log-prob expression for one instance
//   auto instance_lp = theta.nodeLogProb();
//
//   // Data: N countries with different theta values
//   std::vector<double> theta_values = {0.3, 0.5, 0.7};
//
//   // Evaluate: Σᵢ log p(θᵢ | α, β)
//   double total_lp = evaluatePlate(
//       instance_lp,
//       n_countries,
//       alpha = 2.0,                      // scalar binding
//       beta_param = 3.0,                 // scalar binding
//       indexed(theta, theta_values)      // indexed binding
//   );
//
// KEY INSIGHT: Model structure (DAG, distributions) is fully compile-time.
// Only the instance count N is runtime. Symbolic gradients work on the
// "one instance" expression; we sum numerically over instances.

}  // namespace tempura::bayes2
