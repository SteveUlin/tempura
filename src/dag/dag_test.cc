#include "dag/causal.h"
#include "dag/dag.h"
#include "dag/unicode_render.h"
#include "unit.h"

#include <iostream>
#include <string>

using namespace tempura;
using namespace tempura::dag;

// =============================================================================
// Contraceptive Use → Number of Children Causal Model
// =============================================================================
//
// From Statistical Rethinking (McElreath):
// Modeling the impact of contraceptive use on number of children in a region.
//
// Variables per woman i:
//   - Age_i: Woman's age
//   - Contraceptive_i: Uses contraception (binary)
//   - Kids_i: Number of children
//   - District_i: District identifier
//   - Urban_i: Lives in urban area (binary)
//
// Causal Structure:
//
// 1. CAUSE OF INTEREST:
//    Contraceptive → Kids (negative effect, we want to estimate this)
//
// 2. COMPETING CAUSES:
//    Age → Kids (older women have had more time to have children)
//    District characteristics affect both variables
//
// 3. CONFOUNDERS (create spurious associations):
//    Age → Contraceptive (older women may use contraception differently)
//    Urban → Contraceptive (urban areas have better access)
//    Urban → Kids (urban fertility is lower)
//
// 4. UNFORTUNATE RELATIONSHIPS:
//    Kids → Contraceptive (reverse causality! Having kids affects choice)
//    This creates a feedback loop we need to address
//
// 5. SERIES OF UNFORTUNATE RELATIONSHIPS:
//    Unobserved U affects both Contraceptive and Kids
//    (e.g., underlying fertility intentions, economic status)

auto makeContraceptiveModel() -> CausalDag {
  CausalDag dag;

  // Add nodes
  dag.addNode("C", "Contraceptive")
      .addNode("K", "Kids")
      .addNode("A", "Age")
      .addNode("U", "Urban")
      .addNode("D", "District")
      .addNode("F", "Fertility Intent", false);  // Unobserved

  // CAUSE OF INTEREST
  dag.addEdge("C", "K", EdgeType::kCausal);

  // COMPETING CAUSES
  dag.addEdge("A", "K", EdgeType::kCompeting);

  // CONFOUNDERS
  dag.addEdge("A", "C");  // Age affects contraceptive use
  dag.addEdge("U", "C");  // Urban affects contraceptive access
  dag.addEdge("U", "K");  // Urban affects fertility

  // District-level effects
  dag.addEdge("D", "U");  // District determines urban/rural
  dag.addEdge("D", "C");  // District policies affect contraception

  // UNFORTUNATE: Reverse causality
  dag.addEdge("K", "C", EdgeType::kBidirectional);  // Kids affect future contraceptive choice

  // SERIES OF UNFORTUNATE: Unobserved confounder
  dag.addEdge("F", "C", EdgeType::kUnobserved);  // Fertility intent → contraceptive
  dag.addEdge("F", "K", EdgeType::kUnobserved);  // Fertility intent → kids

  dag.setExposure("C").setOutcome("K");

  return dag;
}

// Simpler version without feedback loops (for identification)
auto makeSimplifiedContraceptiveModel() -> CausalDag {
  CausalDag dag;

  dag.addNode("C", "Contraceptive")
      .addNode("K", "Kids")
      .addNode("A", "Age")
      .addNode("U", "Urban")
      .addNode("D", "District");

  // Causal path of interest
  dag.addEdge("C", "K");

  // Direct effect of age
  dag.addEdge("A", "K");

  // Confounders
  dag.addEdge("A", "C");  // Age confounds C-K relationship
  dag.addEdge("U", "C");
  dag.addEdge("U", "K");

  // District structure
  dag.addEdge("D", "U");
  dag.addEdge("D", "C");

  dag.setExposure("C").setOutcome("K");

  return dag;
}

int main() {
  "basic dag construction"_test = [] {
    Dag dag;
    dag.addNode("A", "Alpha")
        .addNode("B", "Beta")
        .addNode("C", "Gamma");

    dag.addEdge("A", "B")
        .addEdge("B", "C")
        .addEdge("A", "C");

    expectEq(dag.nodes().size(), 3uz);
    expectEq(dag.edges().size(), 3uz);

    auto sorted = dag.topologicalSort();
    expectEq(sorted.size(), 3uz);
    expectEq(sorted[0], "A");
  };

  "chain helper"_test = [] {
    Dag dag;
    dag.addNode("X").addNode("Y").addNode("Z");
    dag.chain("X", "Y", "Z");

    expectEq(dag.edges().size(), 2uz);
    expectEq(dag.children("X").size(), 1uz);
    expectEq(dag.children("X")[0], "Y");
    expectEq(dag.children("Y")[0], "Z");
  };

  "fork helper"_test = [] {
    Dag dag;
    dag.addNode("Z").addNode("X").addNode("Y");
    dag.fork("Z", {"X", "Y"});

    expectEq(dag.edges().size(), 2uz);
    expectEq(dag.children("Z").size(), 2uz);
    expectEq(dag.parents("X").size(), 1uz);
    expectEq(dag.parents("Y").size(), 1uz);
  };

  "collider helper"_test = [] {
    Dag dag;
    dag.addNode("X").addNode("Y").addNode("Z");
    dag.collider({"X", "Y"}, "Z");

    expectEq(dag.edges().size(), 2uz);
    expectEq(dag.parents("Z").size(), 2uz);
  };

  "simple rendering"_test = [] {
    Dag dag;
    dag.addNode("X", "Exposure")
        .addNode("Y", "Outcome")
        .addNode("Z", "Confounder");

    dag.addEdge("X", "Y")
        .addEdge("Z", "X")
        .addEdge("Z", "Y");

    auto rendered = renderSimple(dag);
    expectTrue(rendered.find("Exposure") != std::string::npos);
    expectTrue(rendered.find("Outcome") != std::string::npos);
    expectTrue(rendered.find("Confounder") != std::string::npos);

    std::cout << "\n=== Simple Confounding DAG ===\n";
    std::cout << rendered << "\n";
  };

  "box rendering"_test = [] {
    Dag dag;
    dag.addNode("X", "Treatment")
        .addNode("Y", "Outcome");
    dag.addEdge("X", "Y");

    auto rendered = renderBox(dag);
    expectTrue(rendered.find("Treatment") != std::string::npos);

    std::cout << "\n=== Box Rendering ===\n";
    std::cout << rendered << "\n";
  };

  "contraceptive model - full"_test = [] {
    auto dag = makeContraceptiveModel();

    std::cout << "\n=== Contraceptive Use → Kids (Full Model) ===\n";
    std::cout << renderSimple(dag) << "\n";
    std::cout << describeCausalStructure(dag) << "\n";
  };

  "contraceptive model - simplified"_test = [] {
    auto dag = makeSimplifiedContraceptiveModel();

    std::cout << "\n=== Contraceptive Use → Kids (Simplified) ===\n";
    std::cout << renderCausal(dag) << "\n";
    std::cout << describeCausalStructure(dag) << "\n";
  };

  "compact rendering"_test = [] {
    auto dag = makeSimplifiedContraceptiveModel();

    std::cout << "\n=== Compact Rendering ===\n";
    std::cout << renderCompact(dag) << "\n";
  };

  "example: confounding"_test = [] {
    auto dag = makeConfoundingExample();

    std::cout << "\n=== Classic Confounding ===\n";
    std::cout << renderCausal(dag) << "\n";
  };

  "example: mediation"_test = [] {
    auto dag = makeMediationExample();

    std::cout << "\n=== Mediation Structure ===\n";
    std::cout << renderCausal(dag) << "\n";
    std::cout << describeCausalStructure(dag) << "\n";
  };

  "example: collider"_test = [] {
    auto dag = makeColliderExample();

    std::cout << "\n=== Collider (Berkson's Paradox) ===\n";
    std::cout << "Talent and Luck are independent.\n";
    std::cout << "Conditioning on Success creates spurious association.\n\n";
    std::cout << renderSimple(dag) << "\n";
  };

  "example: M-bias"_test = [] {
    auto dag = makeMBiasExample();

    std::cout << "\n=== M-Bias (Butterfly Bias) ===\n";
    std::cout << "Do NOT condition on M - it opens a backdoor path!\n\n";
    std::cout << renderSimple(dag) << "\n";
  };

  "unobserved variables"_test = [] {
    Dag dag;
    dag.addNode("X", "Treatment")
        .addNode("Y", "Outcome")
        .addNode("U", "Unmeasured", false);  // Unobserved

    dag.addEdge("X", "Y")
        .addEdge("U", "X", EdgeType::kUnobserved)
        .addEdge("U", "Y", EdgeType::kUnobserved);

    std::cout << "\n=== Hidden Confounding ===\n";
    std::cout << renderSimple(dag);
    std::cout << "\nNote: ○ indicates unobserved variable, ┈┈→ indicates edge from unobserved\n";
  };

  "edge types display"_test = [] {
    Dag dag;
    dag.addNode("A").addNode("B").addNode("C").addNode("D").addNode("E");

    dag.addEdge("A", "B", EdgeType::kCausal)
        .addEdge("A", "C", EdgeType::kCompeting)
        .addEdge("A", "D", EdgeType::kUnobserved)
        .addEdge("A", "E", EdgeType::kBidirectional);

    std::cout << "\n=== Edge Types ===\n";
    std::cout << renderSimple(dag);
  };

  return TestRegistry::result();
}
