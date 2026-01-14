#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <random>
#include <utility>

namespace tempura {

// ============================================================================
// Reservoir Sampling (Algorithm R)
// ============================================================================
//
// Reservoir sampling solves an elegant problem: select k random items uniformly
// from a stream of unknown length n, using only O(k) memory and a single pass.
//
// THE PROBLEM
// -----------
// You're processing a stream of items (log entries, user clicks, sensor data).
// You want a random sample of size k, but you don't know how many items will
// arrive, and you can't store them all. How do you ensure each item has equal
// probability of being in your final sample?
//
// THE ALGORITHM (Vitter's "Algorithm R", 1985)
// --------------------------------------------
// 1. Fill the reservoir with the first k items
// 2. For each subsequent item i (where i > k):
//    - Generate random j in [0, i)
//    - If j < k, replace reservoir[j] with item i
//
// WHY IT WORKS
// ------------
// After processing n items, each item has probability k/n of being in the sample.
//
// Proof by induction:
// - Base case: After k items, each is in the reservoir with probability 1 = k/k ✓
// - Inductive step: Assume after i-1 items, each has probability k/(i-1).
//   When item i arrives:
//   - Item i enters reservoir with probability k/i ✓
//   - For items already in reservoir: P(survives) = P(was in) × P(not replaced)
//     = k/(i-1) × (1 - 1/i) = k/(i-1) × (i-1)/i = k/i ✓
//
// EXAMPLE
// -------
// Reservoir size k=3, processing items A,B,C,D,E:
//
//   Item  i   Reservoir    Action
//   A     1   [A, _, _]    Fill slot 0
//   B     2   [A, B, _]    Fill slot 1
//   C     3   [A, B, C]    Fill slot 2 (reservoir full)
//   D     4   [A, B, C]    j=rand(0,4)=2, 2<3 → replace C → [A, B, D]
//   E     5   [A, B, D]    j=rand(0,5)=4, 4≥3 → keep → [A, B, D]
//
// Each of A,B,C,D,E had probability 3/5 = 60% of being in the final sample.
//
// TEMPLATE PARAMETERS
// -------------------
//   T - Element type (must be movable)
//   K - Reservoir size (compile-time constant for std::array efficiency)
//
// COMPLEXITY
// ----------
//   add():    O(1) time
//   sample(): O(1) time
//   Space:    O(K) - stores exactly K elements
//
// USE CASES
// ---------
// • Sampling from files too large to fit in memory
// • A/B test logging - sample 1% of events without knowing total
// • Debugging distributed systems - random sample of requests
// • Online learning - maintain representative training subset
//
template <typename T, std::size_t K>
class ReservoirSampling {
  static_assert(K > 0, "Reservoir size must be positive");

 public:
  ReservoirSampling() = default;

  // Construct with a specific seed for reproducibility
  explicit ReservoirSampling(std::uint64_t seed) : rng_{seed} {}

  // Process the next item from the stream.
  // Returns true if the item was added to the reservoir.
  auto add(const T& item) -> bool {
    ++count_;

    if (count_ <= K) {
      // Reservoir not yet full - just add the item
      reservoir_[count_ - 1] = item;
      return true;
    }

    // Reservoir full - apply Algorithm R
    // Item i replaces a random element with probability k/i
    std::uniform_int_distribution<std::size_t> dist{0, count_ - 1};
    std::size_t j = dist(rng_);

    if (j < K) {
      reservoir_[j] = item;
      return true;
    }
    return false;
  }

  // Move variant for efficiency with large objects
  auto add(T&& item) -> bool {
    ++count_;

    if (count_ <= K) {
      reservoir_[count_ - 1] = std::move(item);
      return true;
    }

    std::uniform_int_distribution<std::size_t> dist{0, count_ - 1};
    std::size_t j = dist(rng_);

    if (j < K) {
      reservoir_[j] = std::move(item);
      return true;
    }
    return false;
  }

  // Get the current reservoir sample.
  // Note: If count() < K, only the first count() elements are valid.
  auto sample() const -> const std::array<T, K>& { return reservoir_; }

  // Number of valid elements in the reservoir (min of count and K)
  auto size() const -> std::size_t { return std::min(count_, K); }

  // Total items seen so far
  auto count() const -> std::size_t { return count_; }

  // Whether the reservoir is full
  auto full() const -> bool { return count_ >= K; }

  // Reset to initial state
  void clear() {
    count_ = 0;
    reservoir_ = {};
  }

  // Access individual elements (0 <= idx < size())
  auto operator[](std::size_t idx) const -> const T& {
    assert(idx < size());
    return reservoir_[idx];
  }

  // Iterator support for range-based for loops
  auto begin() const { return reservoir_.begin(); }
  auto end() const { return reservoir_.begin() + static_cast<std::ptrdiff_t>(size()); }

 private:
  std::array<T, K> reservoir_{};
  std::size_t count_ = 0;
  std::mt19937_64 rng_{std::random_device{}()};
};

}  // namespace tempura
