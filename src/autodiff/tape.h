#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tempura::autodiff {

// Reverse-mode AD core (the VJP primitive): a flat Wengert tape. Each elementary op pushes
// ONE Entry recording its local partials and its parents' indices. Because operations are
// recorded in evaluation order, that order is ALREADY a topological sort — so the reverse
// sweep is just a backward scan accumulating adjoints: linear in the number of ops,
// cache-friendly, and correct on shared subexpressions (where a virtual + shared_ptr DAG
// re-walks shared nodes and goes exponential). Parents are INTEGER INDICES so they survive
// the vector's reallocation (raw pointers would dangle). Runtime/allocating by nature —
// reverse mode is not a constexpr path (forward dual/jet is).
//
// Generic over T is the keystone: Tape<Dual<U>> records a forward-dual computation and
// sweeps it backward → forward-over-reverse HVP/Hessian with no second AD system.
template <typename T>
class Tape {
 public:
  struct Entry {
    std::array<T, 2> partials{};          // ∂(this op)/∂(parent k)
    std::array<std::uint32_t, 2> deps{};  // parent indices; a leaf/unary's spare slot self-refs
  };

  // An independent variable / constant: no parents, nothing to propagate through.
  auto leaf() -> std::uint32_t {
    const auto idx = static_cast<std::uint32_t>(entries_.size());
    entries_.push_back(Entry{{T{}, T{}}, {idx, idx}});
    return idx;
  }
  // A unary op with local partial p w.r.t. parent a (spare slot self-references → ignored).
  auto unary(std::uint32_t a, const T& p) -> std::uint32_t {
    const auto idx = static_cast<std::uint32_t>(entries_.size());
    entries_.push_back(Entry{{p, T{}}, {a, idx}});
    return idx;
  }
  // A binary op with local partials pa, pb w.r.t. parents a, b.
  auto binary(std::uint32_t a, std::uint32_t b, const T& pa, const T& pb) -> std::uint32_t {
    const auto idx = static_cast<std::uint32_t>(entries_.size());
    entries_.push_back(Entry{{pa, pb}, {a, b}});
    return idx;
  }

  auto size() const -> std::size_t { return entries_.size(); }

  // Reverse sweep from `output` with cotangent `seed`. Returns the full adjoint vector,
  // where adj[i] = ∂(seed·output)/∂(node i); read adj[leaf.idx] for an input's gradient.
  // Adjoints live in a SEPARATE buffer and the tape is const, so sweeps with different
  // seeds (multi-output VJP, or multi-column reverse Jacobians) are independent.
  auto backward(std::uint32_t output, const T& seed = T{1}) const -> std::vector<T> {
    assert(output < entries_.size() && "output index out of range");
    std::vector<T> adj(entries_.size(), T{});
    adj[output] = seed;
    for (std::size_t i = entries_.size(); i-- > 0;) {
      const Entry& e = entries_[i];
      const T a = adj[i];
      adj[e.deps[0]] += a * e.partials[0];
      // Skip the self-referencing spare slot of a leaf/unary; a real second parent (incl.
      // a repeated parent like x*x) has index < i, so its contribution accumulates.
      if (e.deps[1] != static_cast<std::uint32_t>(i)) adj[e.deps[1]] += a * e.partials[1];
    }
    return adj;
  }

 private:
  std::vector<Entry> entries_;
};

}  // namespace tempura::autodiff
