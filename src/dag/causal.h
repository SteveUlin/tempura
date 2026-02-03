#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "dag/ascii_dag.h"
#include "dag/dag.h"

namespace tempura::dag {

// =============================================================================
// Causal DAG Patterns
// =============================================================================
//
// Common causal structures from Pearl's causal inference framework:
//
// 1. Chain (Mediation):      X → M → Y
//    M is a mediator; controlling for M blocks the causal path
//
// 2. Fork (Confounding):     X ← Z → Y
//    Z is a confounder; X and Y are spuriously correlated
//    Controlling for Z removes the spurious association
//
// 3. Collider (Selection):   X → Z ← Y
//    Z is a collider; X and Y are independent
//    Conditioning on Z creates spurious association (collider bias)
//
// 4. Instrument:             Z → X → Y  with no Z → Y path
//    Z is an instrument for the effect of X on Y
//
// Series of unfortunate events:
//
// 5. Backdoor path:          X ← U → Y  where U is unobserved
//    Hidden confounding creates bias
//
// 6. M-bias:                 U₁ → X ← A → Y ← U₂, U₁ → B ← U₂
//    Controlling for B opens a backdoor path

// =============================================================================
// CausalDag - DAG with causal inference semantics
// =============================================================================

class CausalDag : public Dag {
 public:
  // Mark the exposure (treatment) variable
  auto setExposure(std::string_view name) -> CausalDag& {
    exposure_ = name;
    return *this;
  }

  // Mark the outcome variable
  auto setOutcome(std::string_view name) -> CausalDag& {
    outcome_ = name;
    return *this;
  }

  // Mark variables as conditioned on (observed/adjusted)
  auto condition(std::string_view name) -> CausalDag& {
    conditioned_.push_back(std::string{name});
    return *this;
  }

  auto conditionAll(std::initializer_list<std::string_view> names)
      -> CausalDag& {
    for (auto name : names) {
      conditioned_.push_back(std::string{name});
    }
    return *this;
  }

  // Common patterns
  auto mediator(std::string_view from, std::string_view med,
                std::string_view to) -> CausalDag& {
    addEdge(from, med);
    addEdge(med, to);
    return *this;
  }

  auto confounder(std::string_view conf, std::string_view effect1,
                  std::string_view effect2) -> CausalDag& {
    addEdge(conf, effect1);
    addEdge(conf, effect2);
    return *this;
  }

  auto makeCollider(std::string_view cause1, std::string_view cause2,
                    std::string_view collider) -> CausalDag& {
    addEdge(cause1, collider);
    addEdge(cause2, collider);
    return *this;
  }

  auto backdoor(std::string_view unobserved, std::string_view exposure,
                std::string_view outcome) -> CausalDag& {
    addNode(unobserved, unobserved, false);  // Mark as unobserved
    addEdge(unobserved, exposure, EdgeType::kUnobserved);
    addEdge(unobserved, outcome, EdgeType::kUnobserved);
    return *this;
  }

  // Accessors
  auto exposure() const -> std::string_view { return exposure_; }
  auto outcome() const -> std::string_view { return outcome_; }
  auto conditioned() const -> const std::vector<std::string>& {
    return conditioned_;
  }

 private:
  std::string exposure_;
  std::string outcome_;
  std::vector<std::string> conditioned_;
};

// =============================================================================
// Causal Model Descriptions
// =============================================================================

// Generate a description of the causal structure
inline auto describeCausalStructure(const CausalDag& dag) -> std::string {
  std::string result;

  result += "Causal Model Analysis\n";
  result += "=====================\n\n";

  // Exposure and outcome
  if (!dag.exposure().empty()) {
    result += "Exposure (Treatment): " + std::string{dag.exposure()} + "\n";
  }
  if (!dag.outcome().empty()) {
    result += "Outcome: " + std::string{dag.outcome()} + "\n";
  }
  result += "\n";

  // Identify confounders (common causes of exposure and outcome)
  result += "Potential Confounders:\n";
  bool found_confounder = false;
  for (const auto& node : dag.nodes()) {
    auto kids = dag.children(node.name);
    bool affects_exposure = false;
    bool affects_outcome = false;

    for (const auto& child : kids) {
      if (child == dag.exposure()) affects_exposure = true;
      if (child == dag.outcome()) affects_outcome = true;
    }

    if (affects_exposure && affects_outcome) {
      result += "  • " + node.label;
      if (!node.observed) {
        result += " (unobserved ⚠️)";
      }
      result += "\n";
      found_confounder = true;
    }
  }
  if (!found_confounder) {
    result += "  (none identified)\n";
  }
  result += "\n";

  // Identify mediators (on causal path from exposure to outcome)
  result += "Potential Mediators:\n";
  bool found_mediator = false;
  for (const auto& node : dag.nodes()) {
    if (node.name == dag.exposure() || node.name == dag.outcome()) continue;

    // Check if exposure → node
    auto exposure_children = dag.children(std::string{dag.exposure()});
    bool from_exposure =
        std::ranges::find(exposure_children, node.name) != exposure_children.end();

    // Check if node → outcome
    auto node_children = dag.children(node.name);
    bool to_outcome =
        std::ranges::find(node_children, dag.outcome()) != node_children.end();

    if (from_exposure && to_outcome) {
      result += "  • " + node.label + "\n";
      found_mediator = true;
    }
  }
  if (!found_mediator) {
    result += "  (none identified)\n";
  }
  result += "\n";

  // Identify colliders
  result += "Colliders (do not condition on these!):\n";
  bool found_collider = false;
  for (const auto& node : dag.nodes()) {
    auto pars = dag.parents(node.name);
    if (pars.size() >= 2) {
      result += "  • " + node.label + " ← {";
      for (size_t i = 0; i < pars.size(); ++i) {
        const auto* parent = dag.findNode(pars[i]);
        result += parent ? parent->label : pars[i];
        if (i < pars.size() - 1) result += ", ";
      }
      result += "}\n";
      found_collider = true;
    }
  }
  if (!found_collider) {
    result += "  (none identified)\n";
  }

  return result;
}

// =============================================================================
// Render causal DAG with annotations
// =============================================================================

inline auto renderCausal(const CausalDag& dag) -> std::string {
  std::string result;

  // Main DAG
  result += renderBox(dag);

  // Legend
  result += "\nLegend:\n";
  result += "  ● Observed variable    ○ Unobserved variable\n";
  result += "  ───▶ Causal effect     ╌╌▶ Competing cause\n";
  result += "  ┈┈▶ From unobserved    ◀─▶ Bidirectional\n";

  // Annotations for exposure/outcome
  if (!dag.exposure().empty()) {
    result += "\n  [E] Exposure: " + std::string{dag.exposure()};
  }
  if (!dag.outcome().empty()) {
    result += "\n  [O] Outcome: " + std::string{dag.outcome()};
  }
  if (!dag.conditioned().empty()) {
    result += "\n  [C] Conditioned: ";
    for (size_t i = 0; i < dag.conditioned().size(); ++i) {
      result += dag.conditioned()[i];
      if (i < dag.conditioned().size() - 1) result += ", ";
    }
  }
  result += "\n";

  return result;
}

// =============================================================================
// Example DAG Builders
// =============================================================================

// Classic confounding structure
inline auto makeConfoundingExample() -> CausalDag {
  CausalDag dag;

  dag.addNode("X", "Treatment")
      .addNode("Y", "Outcome")
      .addNode("Z", "Confounder");

  dag.addEdge("X", "Y")  // Causal effect of interest
      .addEdge("Z", "X")  // Confounder affects treatment
      .addEdge("Z", "Y"); // Confounder affects outcome

  dag.setExposure("X").setOutcome("Y");

  return dag;
}

// Mediation structure
inline auto makeMediationExample() -> CausalDag {
  CausalDag dag;

  dag.addNode("X", "Treatment")
      .addNode("M", "Mediator")
      .addNode("Y", "Outcome");

  dag.chain("X", "M", "Y");  // X → M → Y
  dag.addEdge("X", "Y");     // Direct effect X → Y

  dag.setExposure("X").setOutcome("Y");

  return dag;
}

// Collider bias structure
inline auto makeColliderExample() -> CausalDag {
  CausalDag dag;

  dag.addNode("X", "Talent")
      .addNode("Y", "Luck")
      .addNode("Z", "Success");

  dag.makeCollider("X", "Y", "Z");

  return dag;
}

// M-bias (Butterfly bias)
inline auto makeMBiasExample() -> CausalDag {
  CausalDag dag;

  dag.addNode("X", "Treatment")
      .addNode("Y", "Outcome")
      .addNode("U1", "U₁", false)  // Unobserved
      .addNode("U2", "U₂", false)  // Unobserved
      .addNode("M", "M-variable");

  dag.addEdge("U1", "X", EdgeType::kUnobserved)
      .addEdge("U1", "M", EdgeType::kUnobserved)
      .addEdge("U2", "M", EdgeType::kUnobserved)
      .addEdge("U2", "Y", EdgeType::kUnobserved)
      .addEdge("X", "Y");

  dag.setExposure("X").setOutcome("Y");

  return dag;
}

}  // namespace tempura::dag
