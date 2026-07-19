#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace tempura {

// Append-only weighted DAG in CSR form. Every edge points to an earlier node:
// insertion order is a topological order and the weights form a strictly
// lower-triangular L.
template <typename T>
class WeightedDag {
 public:
  struct Edge {
    std::size_t dep;
    T weight;
  };

  // addNode({{a, pa}, {b, pb}})
  auto addNode(std::span<const Edge> edges) -> std::size_t {
    for (const Edge& e : edges) {
      assert(e.dep < nodeCount() && "a parent must be an existing node");
      edges_.push_back(e);
    }
    offsets_.push_back(edges_.size());
    return offsets_.size() - 2;
  }

  auto nodeCount() const -> std::size_t { return offsets_.size() - 1; }

  // A node's edges — row n of L.
  //
  // Borrowed; invalidated by addNode.
  auto edges(std::size_t node) -> std::span<Edge> {
    assert(node < nodeCount() && "node out of range");
    return std::span{edges_}.subspan(offsets_[node],
                                     offsets_[node + 1] - offsets_[node]);
  }

  auto edges(std::size_t node) const -> std::span<const Edge> {
    assert(node < nodeCount() && "node out of range");
    return std::span{edges_}.subspan(offsets_[node],
                                     offsets_[node + 1] - offsets_[node]);
  }

  // Empties, keeping capacity.
  auto clear() -> void {
    edges_.clear();
    offsets_.resize(1);
  }

 private:
  std::vector<Edge> edges_;
  std::vector<std::size_t> offsets_{0};
};

}  // namespace tempura
