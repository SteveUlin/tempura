#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <ranges>
#include <vector>

namespace tempura {

// ============================================================================
// XOR Filter
// ============================================================================
//
// An XOR filter is a static, space-efficient probabilistic data structure for
// set membership testing. Like Bloom filters, it has no false negatives but
// may have false positives. Unlike Bloom filters, XOR filters:
//   - Are more space-efficient (~9 bits/element vs ~10 for Bloom at 1% FPR)
//   - Are static: the set must be known at construction time (no insertions)
//   - Use a fundamentally different algorithm based on solving linear equations
//
// STRUCTURE
// ---------
// An XOR filter stores an array B of fingerprints. For each item x in the set:
//   B[h0(x)] XOR B[h1(x)] XOR B[h2(x)] = fingerprint(x)
//
// To query item y:
//   If B[h0(y)] XOR B[h1(y)] XOR B[h2(y)] == fingerprint(y), return "maybe in set"
//   Otherwise, return "definitely not in set"
//
// The magic is in construction: we need to find values for B such that all
// these XOR equations are satisfied simultaneously. This is a system of linear
// equations over GF(2) (the field with just 0 and 1, where addition is XOR).
//
// CONSTRUCTION (The Peeling Algorithm)
// ------------------------------------
// The construction uses a hypergraph interpretation:
//   - Each slot in B is a vertex
//   - Each item is a hyperedge connecting its 3 hash positions (h0, h1, h2)
//
// The peeling algorithm works as follows:
//
// 1. Build the hypergraph from all items
//
// 2. PEEL PHASE: Repeatedly find and remove hyperedges with a "pure" vertex
//    (a vertex that belongs to exactly one hyperedge). When we remove such an
//    edge, we record it in a stack.
//
//    Example: If vertex 5 only appears in item "apple"'s hyperedge, we can
//    remove "apple" and record it.
//
//    If all edges can be peeled away, the graph was "peelable" and we continue.
//    If we get stuck with no pure vertices, construction fails - try a new seed.
//
// 3. ASSIGN PHASE: Process the stack in reverse (last-peeled first).
//    For each item, we know its pure vertex (the one that was degree-1 when
//    we peeled it). We can freely assign that slot's fingerprint to satisfy
//    the XOR equation, since the other two slots are already assigned.
//
//    B[pure_vertex] = fingerprint(item) XOR B[other1] XOR B[other2]
//
// WHY IT WORKS
// ------------
// The peeling algorithm succeeds when the random hypergraph is "peelable."
// For 3-uniform hypergraphs (edges of size 3), this happens with high
// probability when the number of vertices is ~1.23x the number of edges.
// This is why XOR filters use array size ≈ 1.23 * n elements.
//
// The 1.23 factor comes from the theory of random hypergraph peelability
// (related to the 2-core threshold of random 3-uniform hypergraphs).
//
// FALSE POSITIVE RATE
// -------------------
// The FP rate is approximately 1/2^f where f is the fingerprint bit width.
// Default of 8 bits gives ~0.4% FP rate, 16 bits gives ~0.0015%.
//
// SPACE EFFICIENCY
// ----------------
// Total space = 1.23n * f bits, where f = fingerprint bits
// For f=8 bits: ~9.8 bits per element (vs ~10 bits for Bloom at 1% FPR)
// For f=16 bits: ~19.7 bits per element
//
// XOR filters are particularly efficient because the 1.23 overhead is
// multiplicative with fingerprint size, while Bloom filters need more bits
// for more hash functions.
//
// TEMPLATE PARAMETERS
// -------------------
//   FingerprintBits - Number of bits per fingerprint (default 8)
//                     Determines false positive rate: ~1/2^FingerprintBits
//
// COMPLEXITY
// ----------
//   Construction:  O(n) expected (may need multiple attempts with different seeds)
//   contains:      O(1) (3 array lookups and XORs)
//   Space:         ~1.23n * FingerprintBits bits
//
template <std::size_t FingerprintBits = 8>
class XorFilter {
  static_assert(FingerprintBits == 8 || FingerprintBits == 16 ||
                    FingerprintBits == 32,
                "FingerprintBits must be 8, 16, or 32");

  // Choose the smallest type that fits the fingerprint
  using Fingerprint =
      std::conditional_t<FingerprintBits <= 8, std::uint8_t,
                         std::conditional_t<FingerprintBits <= 16,
                                            std::uint16_t, std::uint32_t>>;

public:
  // Construct an empty filter (contains nothing)
  XorFilter() = default;

  // Construct from a range of uint64_t items.
  // May fail if the hypergraph is not peelable - retries with different seeds.
  // If construction fails after max_attempts, the filter will be empty.
  // Duplicate items are automatically deduplicated.
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::uint64_t>
  explicit XorFilter(R&& items, std::size_t max_attempts = 100) {
    // Collect items into a vector (we need multiple passes)
    std::vector<std::uint64_t> keys;
    for (auto&& item : items) {
      keys.push_back(static_cast<std::uint64_t>(item));
    }

    if (keys.empty()) {
      return;
    }

    // Deduplicate - XOR filter requires unique items
    // (duplicate items create unsatisfiable equations)
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    num_items_ = keys.size();

    // Array size must be at least 1.23 * n for high peeling success probability
    // We use 1.23 and add some slack, rounding up to ensure enough space
    array_size_ = static_cast<std::size_t>(1.23 * num_items_) + 32;

    // Each of the 3 segments has array_size_/3 slots
    segment_size_ = array_size_ / 3;
    array_size_ = segment_size_ * 3;  // Ensure divisible by 3

    fingerprints_.resize(array_size_, 0);

    std::mt19937_64 rng{std::random_device{}()};

    for (std::size_t attempt = 0; attempt < max_attempts; ++attempt) {
      seed_ = rng();
      if (tryConstruct(keys)) {
        constructed_ = true;
        return;
      }
      // Reset fingerprints for next attempt
      std::fill(fingerprints_.begin(), fingerprints_.end(), 0);
    }

    // Construction failed - leave filter empty
    fingerprints_.clear();
    num_items_ = 0;
    array_size_ = 0;
    segment_size_ = 0;
  }

  // Check if an item might be in the set.
  // Returns false: definitely not in set (no false negatives)
  // Returns true: maybe in set (false positives possible with rate ~1/2^FingerprintBits)
  auto contains(std::uint64_t item) const -> bool {
    if (!constructed_ || fingerprints_.empty()) {
      return false;
    }

    auto [h0, h1, h2] = hashPositions(item);
    auto fp = fingerprint(item);

    return (fingerprints_[h0] ^ fingerprints_[h1] ^ fingerprints_[h2]) == fp;
  }

  // Number of items in the filter
  auto size() const -> std::size_t { return num_items_; }

  // Check if the filter was successfully constructed
  auto isConstructed() const -> bool { return constructed_; }

  // Memory usage in bytes
  auto sizeInBytes() const -> std::size_t {
    return fingerprints_.size() * sizeof(Fingerprint) +
           sizeof(*this);
  }

  // Bits per element (useful for comparing with Bloom filters)
  auto bitsPerElement() const -> double {
    if (num_items_ == 0) return 0.0;
    return static_cast<double>(fingerprints_.size() * FingerprintBits) /
           static_cast<double>(num_items_);
  }

  // Theoretical false positive rate
  static constexpr auto falsePositiveRate() -> double {
    return 1.0 / static_cast<double>(1ULL << FingerprintBits);
  }

private:
  // SplitMix64 - fast, high-quality 64-bit hash mixing
  static constexpr auto splitmix64(std::uint64_t x) -> std::uint64_t {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
  }

  // Compute the fingerprint for an item
  auto fingerprint(std::uint64_t item) const -> Fingerprint {
    std::uint64_t h = splitmix64(item ^ seed_);
    return static_cast<Fingerprint>(h & ((1ULL << FingerprintBits) - 1));
  }

  // Compute the 3 hash positions for an item.
  // Uses different parts of the hash to map to 3 different segments.
  auto hashPositions(std::uint64_t item) const
      -> std::tuple<std::size_t, std::size_t, std::size_t> {
    std::uint64_t h = splitmix64(item ^ seed_ ^ 0x123456789ABCDEFULL);

    // Map to 3 different segments to ensure h0, h1, h2 are in disjoint ranges
    std::size_t h0 = static_cast<std::size_t>(h % segment_size_);
    std::size_t h1 = static_cast<std::size_t>((h >> 21) % segment_size_) +
                     segment_size_;
    std::size_t h2 = static_cast<std::size_t>((h >> 42) % segment_size_) +
                     2 * segment_size_;

    return {h0, h1, h2};
  }

  // Attempt to construct the filter using the peeling algorithm
  auto tryConstruct(const std::vector<std::uint64_t>& keys) -> bool {
    const std::size_t n = keys.size();

    // Build adjacency information: for each slot, which items hash to it
    std::vector<std::vector<std::size_t>> slot_to_items(array_size_);
    std::vector<std::tuple<std::size_t, std::size_t, std::size_t>> item_hashes(n);

    for (std::size_t i = 0; i < n; ++i) {
      auto [h0, h1, h2] = hashPositions(keys[i]);
      item_hashes[i] = {h0, h1, h2};
      slot_to_items[h0].push_back(i);
      slot_to_items[h1].push_back(i);
      slot_to_items[h2].push_back(i);
    }

    // Track which items have been peeled
    std::vector<bool> peeled(n, false);

    // Stack of (item_index, pure_slot) for the assign phase
    std::vector<std::pair<std::size_t, std::size_t>> peel_order;
    peel_order.reserve(n);

    // Queue of slots that might be pure (degree 1)
    std::vector<std::size_t> queue;
    queue.reserve(array_size_);

    // Initialize queue with all degree-1 slots
    for (std::size_t slot = 0; slot < array_size_; ++slot) {
      if (slot_to_items[slot].size() == 1) {
        queue.push_back(slot);
      }
    }

    // Peel phase
    while (!queue.empty()) {
      std::size_t slot = queue.back();
      queue.pop_back();

      // Find the single unpeeled item at this slot
      std::size_t item_idx = SIZE_MAX;
      for (std::size_t idx : slot_to_items[slot]) {
        if (!peeled[idx]) {
          if (item_idx != SIZE_MAX) {
            // More than one unpeeled item - not pure anymore
            item_idx = SIZE_MAX;
            break;
          }
          item_idx = idx;
        }
      }

      if (item_idx == SIZE_MAX) {
        continue;  // Slot is no longer pure
      }

      // Peel this item
      peeled[item_idx] = true;
      peel_order.emplace_back(item_idx, slot);

      // Update degrees of other slots in this item's hyperedge
      auto [h0, h1, h2] = item_hashes[item_idx];
      for (std::size_t s : {h0, h1, h2}) {
        if (s == slot) continue;

        // Count remaining unpeeled items at this slot
        std::size_t count = 0;
        for (std::size_t idx : slot_to_items[s]) {
          if (!peeled[idx]) ++count;
        }

        if (count == 1) {
          queue.push_back(s);
        }
      }
    }

    // Check if all items were peeled
    if (peel_order.size() != n) {
      return false;  // Peeling failed - need different seed
    }

    // Assign phase: process in reverse order
    for (auto it = peel_order.rbegin(); it != peel_order.rend(); ++it) {
      auto [item_idx, pure_slot] = *it;
      auto [h0, h1, h2] = item_hashes[item_idx];
      auto fp = fingerprint(keys[item_idx]);

      // The pure_slot is where we can freely assign
      // B[pure_slot] = fp XOR B[other1] XOR B[other2]
      Fingerprint xor_sum = fp;
      for (std::size_t s : {h0, h1, h2}) {
        if (s != pure_slot) {
          xor_sum ^= fingerprints_[s];
        }
      }
      fingerprints_[pure_slot] = xor_sum;
    }

    return true;
  }

  std::vector<Fingerprint> fingerprints_;
  std::size_t num_items_ = 0;
  std::size_t array_size_ = 0;
  std::size_t segment_size_ = 0;
  std::uint64_t seed_ = 0;
  bool constructed_ = false;
};

}  // namespace tempura
