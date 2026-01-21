#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace tempura::dag {

// =============================================================================
// Edge Types for Causal Modeling
// =============================================================================
//
// In causal inference, different edge types represent different relationships:
//
// Cause of Interest:    X ──────→ Y    Direct causal effect we're studying
// Competing Cause:      Z ╌╌╌╌╌╌→ Y    Confounder affecting outcome
// Confounder:           Z ──┬───→ X    Common cause (creates spurious association)
//                           └───→ Y
// Collider:             X ──┬───→ Z    Common effect (conditioning opens path)
//                       Y ──┘
// Mediator:             X → M → Y      Causal path through intermediate variable
// Unobserved:           U ┈┈┈┈┈→ Y     Latent variable (dashed line)
// Bidirectional:        X ←───→ Y      Feedback or unknown direction

enum class EdgeType {
  kCausal,        // Standard causal arrow: ───→
  kCompeting,     // Competing/alternative cause: ╌╌→
  kUnobserved,    // Edge from unobserved variable: ┈┈→
  kBidirectional, // Bidirectional/feedback: ←→
  kSelection,     // Selection/conditioning: ══→
};

// Edge styling for different semantic meanings
enum class EdgeStyle {
  kDefault,   // Normal line weight
  kBold,      // Emphasized (primary effect)
  kDashed,    // Uncertain or hypothesized
  kDotted,    // Weak or indirect
};

// =============================================================================
// Node - A vertex in the DAG
// =============================================================================

struct Node {
  std::string name;
  std::string label;  // Display label (can include unicode)
  bool observed = true;
  int x = 0;  // Grid position for rendering
  int y = 0;

  Node() = default;
  explicit Node(std::string_view n) : name{n}, label{n} {}
  Node(std::string_view n, std::string_view l) : name{n}, label{l} {}
  Node(std::string_view n, std::string_view l, bool obs)
      : name{n}, label{l}, observed{obs} {}
};

// =============================================================================
// Edge - A directed edge in the DAG
// =============================================================================

struct Edge {
  std::string from;
  std::string to;
  EdgeType type = EdgeType::kCausal;
  EdgeStyle style = EdgeStyle::kDefault;
  std::string annotation;  // Optional label on edge (e.g., coefficient name)

  Edge() = default;
  Edge(std::string_view f, std::string_view t)
      : from{f}, to{t} {}
  Edge(std::string_view f, std::string_view t, EdgeType et)
      : from{f}, to{t}, type{et} {}
  Edge(std::string_view f, std::string_view t, EdgeType et, EdgeStyle es)
      : from{f}, to{t}, type{et}, style{es} {}
};

// =============================================================================
// Dag - Directed Acyclic Graph for Causal Models
// =============================================================================

class Dag {
 public:
  Dag() = default;

  // Node operations
  auto addNode(const Node& node) -> Dag& {
    nodes_.push_back(node);
    return *this;
  }

  auto addNode(std::string_view name) -> Dag& {
    return addNode(Node{name});
  }

  auto addNode(std::string_view name, std::string_view label) -> Dag& {
    return addNode(Node{name, label});
  }

  auto addNode(std::string_view name, std::string_view label, bool observed)
      -> Dag& {
    return addNode(Node{name, label, observed});
  }

  // Edge operations
  auto addEdge(const Edge& edge) -> Dag& {
    edges_.push_back(edge);
    return *this;
  }

  auto addEdge(std::string_view from, std::string_view to) -> Dag& {
    return addEdge(Edge{from, to});
  }

  auto addEdge(std::string_view from, std::string_view to, EdgeType type)
      -> Dag& {
    return addEdge(Edge{from, to, type});
  }

  // Convenience: chain of causal arrows A → B → C
  template <typename... Args>
  auto chain(std::string_view first, Args&&... rest) -> Dag& {
    chainImpl(first, std::forward<Args>(rest)...);
    return *this;
  }

  // Convenience: fork pattern A → B, A → C (common cause)
  auto fork(std::string_view cause, std::initializer_list<std::string_view> effects)
      -> Dag& {
    for (auto effect : effects) {
      addEdge(cause, effect);
    }
    return *this;
  }

  // Convenience: collider pattern A → C ← B
  auto collider(std::initializer_list<std::string_view> causes,
                std::string_view effect) -> Dag& {
    for (auto cause : causes) {
      addEdge(cause, effect);
    }
    return *this;
  }

  // Find node by name
  auto findNode(std::string_view name) const -> const Node* {
    auto it = std::ranges::find_if(
        nodes_, [name](const Node& n) { return n.name == name; });
    return it != nodes_.end() ? &*it : nullptr;
  }

  auto findNode(std::string_view name) -> Node* {
    auto it = std::ranges::find_if(
        nodes_, [name](const Node& n) { return n.name == name; });
    return it != nodes_.end() ? &*it : nullptr;
  }

  // Graph queries
  auto parents(std::string_view node) const -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto& e : edges_) {
      if (e.to == node) {
        result.push_back(e.from);
      }
    }
    return result;
  }

  auto children(std::string_view node) const -> std::vector<std::string> {
    std::vector<std::string> result;
    for (const auto& e : edges_) {
      if (e.from == node) {
        result.push_back(e.to);
      }
    }
    return result;
  }

  // Topological sort (returns empty if cycle detected)
  auto topologicalSort() const -> std::vector<std::string> {
    std::vector<std::string> result;
    std::vector<bool> visited(nodes_.size(), false);
    std::vector<bool> in_stack(nodes_.size(), false);
    bool has_cycle = false;

    // Use a local function object instead of deducing this lambda
    // to avoid issues with member variable capture
    struct Visitor {
      const Dag& dag;
      std::vector<std::string>& result;
      std::vector<bool>& visited;
      std::vector<bool>& in_stack;
      bool& has_cycle;

      void operator()(size_t idx) {
        if (has_cycle) return;
        if (in_stack[idx]) {
          has_cycle = true;
          return;
        }
        if (visited[idx]) return;

        in_stack[idx] = true;
        for (const auto& child_name : dag.children(dag.nodes_[idx].name)) {
          auto child_it = std::ranges::find_if(
              dag.nodes_,
              [&child_name](const Node& n) { return n.name == child_name; });
          if (child_it != dag.nodes_.end()) {
            (*this)(
                static_cast<size_t>(std::distance(dag.nodes_.begin(), child_it)));
          }
        }
        in_stack[idx] = false;
        visited[idx] = true;
        result.push_back(dag.nodes_[idx].name);
      }
    };

    Visitor visit{*this, result, visited, in_stack, has_cycle};

    for (size_t i = 0; i < nodes_.size(); ++i) {
      visit(i);
    }

    if (has_cycle) return {};
    std::ranges::reverse(result);
    return result;
  }

  // Accessors
  auto nodes() const -> const std::vector<Node>& { return nodes_; }
  auto edges() const -> const std::vector<Edge>& { return edges_; }
  auto nodes() -> std::vector<Node>& { return nodes_; }
  auto edges() -> std::vector<Edge>& { return edges_; }

 private:
  template <typename... Args>
  void chainImpl(std::string_view first, std::string_view second,
                 Args&&... rest) {
    addEdge(first, second);
    if constexpr (sizeof...(rest) > 0) {
      chainImpl(second, std::forward<Args>(rest)...);
    }
  }

  void chainImpl(std::string_view) {}  // Base case

  std::vector<Node> nodes_;
  std::vector<Edge> edges_;
};

}  // namespace tempura::dag
