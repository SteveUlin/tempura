// Probe length analysis for linear probing hash map
//
// Measures empirical probe counts and compares to Knuth's theoretical values.
// Outputs ASCII histogram of probe length distribution.
//
// ============================================================================
// THEORETICAL ANALYSIS (Knuth, TAOCP Vol. 3, Section 6.4)
// ============================================================================
//
// Setup:
//   m = number of slots (table capacity)
//   n = number of stored elements
//   α = n/m = load factor (0 < α < 1)
//
// We assume uniform hashing: each key hashes to each slot with probability 1/m.
//
// RESULTS (to be derived below):
//
//   Unsuccessful search:  U(α) = ½(1 + 1/(1-α)²)
//   Successful search:    S(α) = ½(1 + 1/(1-α))
//
// ============================================================================
// PART 1: UNSUCCESSFUL SEARCH - THE CLUSTER ANALYSIS
// ============================================================================
//
// For unsuccessful search, we probe h(k), h(k)+1, h(k)+2, ... until finding
// an empty slot. We need E[number of probes].
//
// KEY INSIGHT: Linear probing creates "clusters" - contiguous runs of occupied
// slots. If slots i, i+1, ..., i+j-1 are occupied but i-1 and i+j are empty,
// that's a cluster of size j.
//
// Let X = number of probes for unsuccessful search.
// Using the tail sum formula for expectation:
//
//   E[X] = Σ_{k=1}^{∞} P(X ≥ k) = Σ_{k=1}^{∞} P(first k-1 probes hit occupied)
//
// Let p_k = P(k consecutive slots starting at random position are occupied).
//
// Then E[X] = 1 + Σ_{k=1}^{∞} p_k  (we always need at least 1 probe)
//
// COMPUTING p_k (the hard part):
// -----------------------------
// For RANDOM probing (independent slots): p_k = α^k, giving E[X] = 1/(1-α).
//
// For LINEAR probing, slots are NOT independent. A cluster of size j "blocks"
// j+1 hash positions (any hash landing in the cluster OR immediately after
// it extends the cluster or adds to its end).
//
// Knuth's analysis uses the CYCLE LEMMA from combinatorics:
//
// Consider n keys hashing into m slots. Arrange slots in a circle. For any
// specific starting position, we want P(k consecutive slots are occupied).
//
// Using the cycle lemma (related to ballot problems), Knuth shows:
//
//   p_k = (1/m) · Σ_{j=k}^{n} C(n,j) · j · (m-j)^{n-j} / m^{n-1}
//
// where C(n,j) is "n choose j".
//
// In the limit of large m,n with α = n/m fixed, this sum evaluates to:
//
//   Σ_{k=1}^{∞} p_k = ½(1/(1-α)² - 1)
//
// Therefore:
//
//   U(α) = E[X] = 1 + ½(1/(1-α)² - 1) = ½ + ½/(1-α)² = ½(1 + 1/(1-α)²)  ∎
//
// INTUITION FOR THE SQUARED TERM:
// -------------------------------
// Why 1/(1-α)² instead of 1/(1-α)? Primary clustering!
//
// A cluster of size k captures insertions hashing to k+1 positions:
//   - The k slots in the cluster
//   - The slot immediately after (new elements go at the end)
//
// So large clusters grow faster. If cluster sizes were independent:
//   - Random probing: E[run length] = α/(1-α), so E[probes] ~ 1/(1-α)
//
// But with clustering, large clusters are MORE likely to be hit (proportional
// to their size), AND they take longer to traverse. This "double whammy"
// of (size) × (size) gives the squared term.
//
// More precisely: P(landing in cluster of size k) ∝ k (bigger = more likely)
//                 E[probes | in cluster of size k] ∝ k (bigger = longer)
//                 Combined effect: proportional to k², hence 1/(1-α)²
//
// ============================================================================
// PART 2: SUCCESSFUL SEARCH - THE HISTORICAL AVERAGE
// ============================================================================
//
// For successful search, we look up a key that's already in the table. The
// number of probes equals what it took to INSERT that key originally.
//
// KEY INSIGHT: A key inserted when load factor was x took U(x) probes on
// average (same as unsuccessful search at that moment, since the key wasn't
// present yet).
//
// If we insert n keys into m slots, key i was inserted when load was (i-1)/m.
// The average probe length over all n keys:
//
//   S(α) = (1/n) · Σ_{i=1}^{n} U((i-1)/m)
//
// In the continuous limit (large n):
//
//   S(α) = (1/α) · ∫₀^α U(x) dx
//
//        = (1/α) · ∫₀^α ½(1 + 1/(1-x)²) dx
//
// Computing the integral:
// ----------------------
//   ∫ ½(1 + 1/(1-x)²) dx
//
// Let u = 1-x, du = -dx:
//
//   = ½ ∫ (1 + 1/u²)(-du)
//   = ½ (-u - (-1/u))
//   = ½ (-（1-x) + 1/(1-x))
//   = ½ (x - 1 + 1/(1-x))
//
// Evaluating from 0 to α:
//
//   [½(x - 1 + 1/(1-x))]₀^α = ½(α - 1 + 1/(1-α)) - ½(0 - 1 + 1)
//                           = ½(α - 1 + 1/(1-α)) - 0
//                           = ½(α - 1 + 1/(1-α))
//
// Dividing by α:
//
//   S(α) = (1/α) · ½(α - 1 + 1/(1-α))
//        = ½(1 - 1/α + 1/(α(1-α)))
//        = ½(1 + (1 - (1-α))/(α(1-α)))     [combining fractions]
//        = ½(1 + α/(α(1-α)))
//        = ½(1 + 1/(1-α))  ∎
//
// ============================================================================
// NUMERICAL EXAMPLES
// ============================================================================
//
//   α     | Successful S(α) | Unsuccessful U(α) | Ratio U/S
//   ------|-----------------|-------------------|----------
//   0.50  |      1.50       |       2.50        |   1.67
//   0.70  |      2.17       |       6.06        |   2.79
//   0.80  |      3.00       |      13.00        |   4.33
//   0.90  |      5.50       |      50.50        |   9.18
//   0.95  |     10.50       |     200.50        |  19.10
//
// Note how unsuccessful search degrades MUCH faster than successful search.
// This is why we keep α ≤ 0.7 - unsuccessful searches (including the probe
// during insertion) become expensive at higher load factors.
//
// ============================================================================
// COMPARISON WITH OTHER PROBING STRATEGIES
// ============================================================================
//
// RANDOM PROBING (theoretical, not practical):
//   U(α) = 1/(1-α)           S(α) = (1/α)·ln(1/(1-α))
//   At α=0.7: U=3.33, S=1.72
//
// DOUBLE HASHING (practical approximation of random):
//   Similar to random probing, avoids primary clustering.
//   At α=0.7: U≈3.33, S≈1.72
//
// LINEAR PROBING (this implementation):
//   U(α) = ½(1 + 1/(1-α)²)   S(α) = ½(1 + 1/(1-α))
//   At α=0.7: U=6.06, S=2.17
//
// Linear probing is ~2x worse for unsuccessful search at α=0.7 due to
// clustering, but has better cache performance in practice since probes
// are sequential in memory.
//
// ============================================================================

#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include "containers/maps/linear_probing.h"

using namespace tempura;

// Instrumented find that returns probe count
template <typename Map, typename Key>
auto countProbes(const Map& map, const Key& key) -> std::size_t {
  if (map.capacity() == 0) return 0;

  std::size_t probes = 0;
  std::size_t idx = std::hash<Key>{}(key) % map.capacity();

  // We need access to internals, so we'll simulate the probe sequence
  // by doing repeated finds with knowledge of the algorithm
  for (std::size_t i = 0; i < map.capacity(); ++i) {
    ++probes;
    std::size_t probe_idx = (idx + i) % map.capacity();

    // Check if key exists at this position
    if (map.contains(key)) {
      // We found it - but we need to know WHERE
      // This is a limitation: we can't easily instrument without modifying the class
      break;
    }
  }
  return probes;
}

auto main() -> int {
  constexpr std::size_t kCapacity = 10000;
  constexpr double kTargetLoadFactor = 0.7;
  constexpr std::size_t kInsertCount =
      static_cast<std::size_t>(kCapacity * kTargetLoadFactor);

  std::printf("Linear Probing Probe Length Analysis\n");
  std::printf("=====================================\n");
  std::printf("Capacity: %zu, Target load factor: %.1f\n", kCapacity,
              kTargetLoadFactor);
  std::printf("Inserting %zu elements\n\n", kInsertCount);

  // Theoretical values (Knuth)
  double alpha = kTargetLoadFactor;
  double expected_success = 0.5 * (1.0 + 1.0 / (1.0 - alpha));
  double expected_fail = 0.5 * (1.0 + 1.0 / ((1.0 - alpha) * (1.0 - alpha)));

  std::printf("Theoretical (Knuth):\n");
  std::printf("  Successful search:   %.2f probes\n", expected_success);
  std::printf("  Unsuccessful search: %.2f probes\n\n", expected_fail);

  // We need to instrument the map directly. Let's create a simple probe counter
  // by reimplementing the probe logic externally.

  // Use a custom map that exposes slot states for analysis
  struct ProbeStats {
    std::vector<std::size_t> insert_probes;
    std::vector<std::size_t> success_probes;
    std::vector<std::size_t> fail_probes;
  } stats;

  // Simple open addressing simulation for clean measurement
  enum class SlotState { kEmpty, kOccupied, kTombstone };
  std::vector<SlotState> slots(kCapacity, SlotState::kEmpty);
  std::vector<int> keys(kCapacity);

  std::mt19937_64 rng{42};
  std::uniform_int_distribution<int> dist{0, 1'000'000'000};

  std::vector<int> inserted_keys;
  inserted_keys.reserve(kInsertCount);

  // Insert phase - count probes for each insertion
  for (std::size_t i = 0; i < kInsertCount; ++i) {
    int key = dist(rng);
    std::size_t hash = std::hash<int>{}(key) % kCapacity;

    std::size_t probes = 0;
    for (std::size_t j = 0; j < kCapacity; ++j) {
      ++probes;
      std::size_t idx = (hash + j) % kCapacity;
      if (slots[idx] == SlotState::kEmpty) {
        slots[idx] = SlotState::kOccupied;
        keys[idx] = key;
        inserted_keys.push_back(key);
        break;
      }
      if (slots[idx] == SlotState::kOccupied && keys[idx] == key) {
        // Duplicate - skip
        break;
      }
    }
    stats.insert_probes.push_back(probes);
  }

  // Successful search - look up keys we inserted
  for (int key : inserted_keys) {
    std::size_t hash = std::hash<int>{}(key) % kCapacity;
    std::size_t probes = 0;
    for (std::size_t j = 0; j < kCapacity; ++j) {
      ++probes;
      std::size_t idx = (hash + j) % kCapacity;
      if (slots[idx] == SlotState::kOccupied && keys[idx] == key) {
        break;
      }
      if (slots[idx] == SlotState::kEmpty) {
        break;  // Not found (shouldn't happen)
      }
    }
    stats.success_probes.push_back(probes);
  }

  // Unsuccessful search - look up random keys not in the map
  for (std::size_t i = 0; i < kInsertCount; ++i) {
    int key = dist(rng) + 2'000'000'000;  // Guaranteed not in map
    std::size_t hash = std::hash<int>{}(key) % kCapacity;
    std::size_t probes = 0;
    for (std::size_t j = 0; j < kCapacity; ++j) {
      ++probes;
      std::size_t idx = (hash + j) % kCapacity;
      if (slots[idx] == SlotState::kEmpty) {
        break;
      }
    }
    stats.fail_probes.push_back(probes);
  }

  // Calculate averages
  auto average = [](const std::vector<std::size_t>& v) {
    double sum = 0;
    for (auto x : v) sum += x;
    return sum / static_cast<double>(v.size());
  };

  double avg_insert = average(stats.insert_probes);
  double avg_success = average(stats.success_probes);
  double avg_fail = average(stats.fail_probes);

  std::printf("Empirical (N=%zu):\n", kInsertCount);
  std::printf("  Insert:              %.2f probes\n", avg_insert);
  std::printf("  Successful search:   %.2f probes (theory: %.2f)\n",
              avg_success, expected_success);
  std::printf("  Unsuccessful search: %.2f probes (theory: %.2f)\n\n", avg_fail,
              expected_fail);

  // Build histogram for successful searches
  constexpr std::size_t kMaxBucket = 20;
  std::vector<std::size_t> hist(kMaxBucket + 1, 0);

  for (auto p : stats.success_probes) {
    if (p <= kMaxBucket) {
      hist[p]++;
    } else {
      hist[kMaxBucket]++;
    }
  }

  // Find max for scaling
  std::size_t max_count = 0;
  for (auto c : hist) max_count = std::max(max_count, c);

  std::printf("Successful Search Probe Distribution:\n");
  std::printf("--------------------------------------\n");

  constexpr int kBarWidth = 50;
  for (std::size_t i = 1; i <= kMaxBucket; ++i) {
    int bar_len =
        static_cast<int>(static_cast<double>(hist[i]) / max_count * kBarWidth);
    double pct = 100.0 * hist[i] / stats.success_probes.size();

    std::printf("%2zu | ", i);
    for (int j = 0; j < bar_len; ++j) std::printf("█");
    for (int j = bar_len; j < kBarWidth; ++j) std::printf(" ");
    std::printf(" %5.1f%% (%zu)\n", pct, hist[i]);
  }

  // Histogram for unsuccessful searches
  std::vector<std::size_t> fail_hist(kMaxBucket + 1, 0);
  for (auto p : stats.fail_probes) {
    if (p <= kMaxBucket) {
      fail_hist[p]++;
    } else {
      fail_hist[kMaxBucket]++;
    }
  }

  max_count = 0;
  for (auto c : fail_hist) max_count = std::max(max_count, c);

  std::printf("\nUnsuccessful Search Probe Distribution:\n");
  std::printf("----------------------------------------\n");

  for (std::size_t i = 1; i <= kMaxBucket; ++i) {
    if (fail_hist[i] == 0 && i > 15) continue;  // Skip empty tail
    int bar_len = static_cast<int>(static_cast<double>(fail_hist[i]) /
                                   max_count * kBarWidth);
    double pct = 100.0 * fail_hist[i] / stats.fail_probes.size();

    std::printf("%2zu | ", i);
    for (int j = 0; j < bar_len; ++j) std::printf("█");
    for (int j = bar_len; j < kBarWidth; ++j) std::printf(" ");
    std::printf(" %5.1f%% (%zu)\n", pct, fail_hist[i]);
  }

  return 0;
}
