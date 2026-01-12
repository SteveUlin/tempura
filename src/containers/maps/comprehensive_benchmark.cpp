// Comprehensive Hash Map Benchmark Suite
//
// Design based on research from:
// - hashtable-bench (renzibei): Test across cache hierarchy, multiple distributions
// - Jackson Allan's C/C++ Benchmark: Load factor sweeps, large values, string keys
// - Koloboke Wiki: Tombstone accumulation measurement
// - thenumb.at/Hashtables: Dataset larger than L3 for memory-bound testing
//
// Key workloads:
// 1. Core operations at multiple sizes (L1 → L2 → L3 → RAM)
// 2. Load factor sweep (50% → 75% → 90%)
// 3. Tombstone accumulation (insert N, delete 50%, measure lookup)
// 4. Delete-heavy steady state (simulates LRU cache)
// 5. Large values (cache line traversal cost)
// 6. Iteration performance
// 7. Clustered keys (stress collision handling)
//
// Thread-based timeout: Each test runs in its own thread with a timeout.
// If a test exceeds the timeout, it's abandoned and the map is blacklisted.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <print>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Include all map implementations
#include "containers/maps/anti_robin_hood.h"
#include "containers/maps/cuckoo.h"
#include "containers/maps/double_hashing.h"
#include "containers/maps/f14.h"
#include "containers/maps/f14_prefetch.h"
#include "containers/maps/graveyard.h"
#include "containers/maps/hopscotch.h"
#include "containers/maps/linear_probing.h"
#include "containers/maps/linear_probing_reuse.h"
#include "containers/maps/quadratic_probing.h"
#include "containers/maps/robin_hood.h"
#include "containers/maps/separate_chaining.h"
#include "containers/maps/swiss_table.h"
#include "containers/maps/swiss_table_32.h"
#include "containers/maps/swiss_table_32_graveyard.h"
#include "containers/maps/swiss_table_32_prefetch.h"
#include "containers/maps/swiss_table_32_prefetch_graveyard.h"
#include "containers/maps/swiss_table_64.h"
#include "containers/maps/swiss_table_bloom.h"
#include "containers/maps/swiss_table_graveyard.h"
#include "containers/maps/swiss_table_prefetch.h"
#include "containers/maps/swiss_table_reuse.h"
#include "containers/maps/two_choice.h"

using namespace tempura;

// ============================================================================
// Configuration
// ============================================================================

// ==========================================================================
// AMD Ryzen AI 9 HX 370 (Zen 5) cache hierarchy:
//   L1 Data: 48 KB/core
//   L2: 1 MB/core
//   L3: 24 MB shared
//
// With ~16 bytes per entry (key + value + overhead for open addressing):
//   L1-fit: ~3K entries (48KB / 16)
//   L2-fit: ~64K entries (1MB / 16)
//   L3-fit: ~1.5M entries (24MB / 16)
//   RAM: >1.5M entries
// ==========================================================================

namespace config {
// Dataset sizes targeting YOUR CPU's cache levels
// Ryzen AI 9 HX 370: L1=48KB, L2=1MB, L3=24MB
constexpr std::size_t kL1 = 2'000;         // ~32KB - comfortably in L1 (48KB)
constexpr std::size_t kL2 = 50'000;        // ~800KB - comfortably in L2 (1MB)
constexpr std::size_t kL3 = 1'000'000;     // ~16MB - comfortably in L3 (24MB)
constexpr std::size_t kRAM = 5'000'000;    // ~80MB - exceeds L3, forces RAM access

// For LRU/steady-state tests (tombstone accumulation)
constexpr std::size_t kSteadyState = 500'000;       // Table size for LRU simulation
constexpr std::size_t kSteadyStateOps = 2'000'000;  // 2M ops for real tombstone buildup

// Warm-up iterations to stabilize cache
constexpr std::size_t kWarmupOps = 10'000;

// Timeout: skip map if single test takes longer than this
constexpr double kTimeoutSeconds = 15.0;  // Skip slow maps after 15s
}  // namespace config

// ============================================================================
// Slow Map Tracking
// ============================================================================
// Maps that exceed timeout get blacklisted for remaining tests

#include <set>
std::set<std::string> g_slow_maps;  // Maps that have timed out

// ============================================================================
// Statistics
// ============================================================================

struct Stats {
  double min, p25, p50, p75, p90, p99, max;
  double mean;
  std::size_t count;

  static auto compute(std::vector<std::uint64_t>& samples) -> Stats {
    if (samples.empty()) return {0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::sort(samples.begin(), samples.end());
    std::size_t n = samples.size();
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    return {
        .min = static_cast<double>(samples[0]),
        .p25 = static_cast<double>(samples[n * 25 / 100]),
        .p50 = static_cast<double>(samples[n / 2]),
        .p75 = static_cast<double>(samples[n * 75 / 100]),
        .p90 = static_cast<double>(samples[n * 90 / 100]),
        .p99 = static_cast<double>(samples[n * 99 / 100]),
        .max = static_cast<double>(samples[n - 1]),
        .mean = sum / static_cast<double>(n),
        .count = n,
    };
  }
};

struct BenchResult {
  std::string map_name;
  std::string test_name;
  Stats stats;
  double throughput_mops;  // Million ops per second
};

// ============================================================================
// Timing Utilities
// ============================================================================

template <typename Fn>
auto timeOp(Fn&& fn) -> std::uint64_t {
  auto start = std::chrono::high_resolution_clock::now();
  fn();
  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

// ============================================================================
// Map Type Detection
// ============================================================================

template <typename T>
struct is_std_map : std::false_type {};
template <typename K, typename V, typename... Args>
struct is_std_map<std::unordered_map<K, V, Args...>> : std::true_type {};
template <typename K, typename V, typename... Args>
struct is_std_map<std::map<K, V, Args...>> : std::true_type {};

// ============================================================================
// WORKLOAD 1: Core Operations at Multiple Sizes
// ============================================================================
// Tests insert/find/erase at different cache hierarchy levels.
// Shows how performance changes as data exceeds each cache level.

template <typename Map>
auto benchInsert(std::size_t n, std::mt19937& rng) -> Stats {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) keys[i] = static_cast<int>(rng());

  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (int key : keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        map.insert({key, key});
      } else {
        map.insert(key, key);
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// Returns empty Stats if setup exceeds timeout
template <typename Map>
auto benchFindHit(std::size_t n, std::mt19937& rng) -> Stats {
  auto setup_start = std::chrono::high_resolution_clock::now();

  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    if constexpr (is_std_map<Map>::value) {
      map.insert({keys[i], keys[i]});
    } else {
      map.insert(keys[i], keys[i]);
    }

    // Check timeout every 100K inserts
    if (i % 100'000 == 0 && i > 0) {
      auto now = std::chrono::high_resolution_clock::now();
      double elapsed = std::chrono::duration<double>(now - setup_start).count();
      if (elapsed > config::kTimeoutSeconds) {
        return Stats{.p50 = 999999, .mean = 999999};  // Sentinel for timeout
      }
    }
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  // Warm-up
  volatile int sink = 0;
  for (std::size_t i = 0; i < std::min(config::kWarmupOps, n); ++i) {
    if constexpr (is_std_map<Map>::value) {
      auto it = map.find(keys[i]);
      if (it != map.end()) sink = it->second;
    } else {
      auto* p = map.find(keys[i]);
      if (p) sink = *p;
    }
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  for (int key : keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        auto it = map.find(key);
        if (it != map.end()) sink = it->second;
      } else {
        auto* p = map.find(key);
        if (p) sink = *p;
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

template <typename Map>
auto benchFindMiss(std::size_t n, std::mt19937& rng) -> Stats {
  auto setup_start = std::chrono::high_resolution_clock::now();

  Map map;
  // Insert keys [0, n)
  for (std::size_t i = 0; i < n; ++i) {
    if constexpr (is_std_map<Map>::value) {
      map.insert({static_cast<int>(i), static_cast<int>(i)});
    } else {
      map.insert(static_cast<int>(i), static_cast<int>(i));
    }

    if (i % 100'000 == 0 && i > 0) {
      auto now = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration<double>(now - setup_start).count() > config::kTimeoutSeconds) {
        return Stats{.p50 = 999999, .mean = 999999};
      }
    }
  }

  // Search for keys [n, 2n) - all misses
  std::vector<int> miss_keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    miss_keys[i] = static_cast<int>(n + i);
  }
  std::shuffle(miss_keys.begin(), miss_keys.end(), rng);

  volatile int sink = 0;
  std::vector<std::uint64_t> times;
  times.reserve(n);

  for (int key : miss_keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        auto it = map.find(key);
        if (it != map.end()) sink = it->second;
      } else {
        auto* p = map.find(key);
        if (p) sink = *p;
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 2: Tombstone Accumulation (Critical for comparing F14 vs Swiss)
// ============================================================================
// Insert N keys, delete 50%, then measure lookup on remaining keys.
// Shows how tombstones degrade lookup performance.
// F14's overflow counting should maintain performance; tombstone-based maps degrade.

template <typename Map>
auto benchPostDeleteLookup(std::size_t n, std::mt19937& rng) -> Stats {
  auto setup_start = std::chrono::high_resolution_clock::now();

  Map map;
  std::vector<int> all_keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    all_keys[i] = static_cast<int>(rng());
    if constexpr (is_std_map<Map>::value) {
      map.insert({all_keys[i], all_keys[i]});
    } else {
      map.insert(all_keys[i], all_keys[i]);
    }

    if (i % 100'000 == 0 && i > 0) {
      auto now = std::chrono::high_resolution_clock::now();
      if (std::chrono::duration<double>(now - setup_start).count() > config::kTimeoutSeconds) {
        return Stats{.p50 = 999999, .mean = 999999};
      }
    }
  }

  // Delete 50% of keys (creates tombstones in tombstone-based maps)
  std::shuffle(all_keys.begin(), all_keys.end(), rng);
  std::vector<int> remaining_keys;
  for (std::size_t i = 0; i < n; ++i) {
    if (i < n / 2) {
      if constexpr (is_std_map<Map>::value) {
        map.erase(all_keys[i]);
      } else {
        map.erase(all_keys[i]);
      }
    } else {
      remaining_keys.push_back(all_keys[i]);
    }
  }

  // Now measure lookup on remaining keys (must traverse tombstones)
  std::shuffle(remaining_keys.begin(), remaining_keys.end(), rng);

  volatile int sink = 0;
  std::vector<std::uint64_t> times;
  times.reserve(remaining_keys.size());

  for (int key : remaining_keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        auto it = map.find(key);
        if (it != map.end()) sink = it->second;
      } else {
        auto* p = map.find(key);
        if (p) sink = *p;
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 3: Delete-Heavy Steady State (LRU Cache Simulation)
// ============================================================================
// Simulates an LRU cache: insert new key, delete oldest.
// Tombstones accumulate without limit in naive implementations.
// Tests how maps handle continuous insert/delete at constant size.

template <typename Map>
auto benchLruSimulation(std::size_t table_size, std::size_t num_ops, std::mt19937& rng) -> Stats {
  Map map;

  // Fill to target size
  std::vector<int> keys;
  keys.reserve(table_size);
  for (std::size_t i = 0; i < table_size; ++i) {
    int key = static_cast<int>(rng());
    keys.push_back(key);
    if constexpr (is_std_map<Map>::value) {
      map.insert({key, key});
    } else {
      map.insert(key, key);
    }
  }

  // Now do insert-delete pairs maintaining constant size
  std::vector<std::uint64_t> times;
  times.reserve(num_ops);

  for (std::size_t i = 0; i < num_ops; ++i) {
    // Delete oldest (FIFO order - first in queue)
    int old_key = keys[i % keys.size()];

    // Insert new
    int new_key = static_cast<int>(rng());

    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        map.erase(old_key);
        map.insert({new_key, new_key});
      } else {
        map.erase(old_key);
        map.insert(new_key, new_key);
      }
    });

    keys[i % keys.size()] = new_key;
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 4: Load Factor Sweep
// ============================================================================
// Measures lookup performance at different load factors.
// Shows how performance degrades as table gets fuller.

template <typename Map>
auto benchAtLoadFactor(std::size_t capacity_hint, double target_load, std::mt19937& rng) -> Stats {
  Map map;

  // Try to reserve capacity (not all maps support this)
  // For maps that grow dynamically, we fill to the target load of final size

  std::size_t target_size = static_cast<std::size_t>(capacity_hint * target_load);

  std::vector<int> keys;
  keys.reserve(target_size);

  for (std::size_t i = 0; i < target_size; ++i) {
    int key = static_cast<int>(rng());
    keys.push_back(key);
    if constexpr (is_std_map<Map>::value) {
      map.insert({key, key});
    } else {
      map.insert(key, key);
    }
  }

  std::shuffle(keys.begin(), keys.end(), rng);

  volatile int sink = 0;
  std::vector<std::uint64_t> times;
  times.reserve(keys.size());

  for (int key : keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        auto it = map.find(key);
        if (it != map.end()) sink = it->second;
      } else {
        auto* p = map.find(key);
        if (p) sink = *p;
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 5: Large Values (Cache Line Traversal)
// ============================================================================
// Tests with 64-byte values to show when metadata colocation matters.
// Swiss Table benefits from metadata in separate array (fewer cache lines per probe).
// Traditional open addressing suffers (must load full slot to check).

struct LargeValue {
  std::array<std::uint64_t, 8> data;  // 64 bytes

  LargeValue() : data{} {}
  explicit LargeValue(int v) {
    data.fill(static_cast<std::uint64_t>(v));
  }
};

template <typename Map>
auto benchLargeValueInsert(std::size_t n, std::mt19937& rng) -> Stats {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) keys[i] = static_cast<int>(rng());

  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (int key : keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        map.insert({key, LargeValue{key}});
      } else {
        map.insert(key, LargeValue{key});
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

template <typename Map>
auto benchLargeValueFind(std::size_t n, std::mt19937& rng) -> Stats {
  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    if constexpr (is_std_map<Map>::value) {
      map.insert({keys[i], LargeValue{keys[i]}});
    } else {
      map.insert(keys[i], LargeValue{keys[i]});
    }
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  volatile std::uint64_t sink = 0;
  std::vector<std::uint64_t> times;
  times.reserve(n);

  for (int key : keys) {
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        auto it = map.find(key);
        if (it != map.end()) sink = it->second.data[0];
      } else {
        auto* p = map.find(key);
        if (p) sink = p->data[0];
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 6: Iteration (std:: maps only - not all custom maps have iterators)
// ============================================================================
// Measures time to iterate over all entries.
// Important for use cases that need to traverse the entire table.
// Only implemented for std:: containers as custom maps may not have iterators.

auto benchIterationStdUnordered(std::size_t n, std::mt19937& rng) -> Stats {
  std::unordered_map<int, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    int key = static_cast<int>(rng());
    map.insert({key, key});
  }

  volatile std::int64_t sink = 0;
  std::vector<std::uint64_t> times;

  constexpr int kIterations = 10;
  for (int iter = 0; iter < kIterations; ++iter) {
    auto ns = timeOp([&] {
      std::int64_t sum = 0;
      for (const auto& [k, v] : map) {
        sum += v;
      }
      sink = sum;
    });
    times.push_back(ns / n);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 7: Clustered Keys (Sequential/Patterned)
// ============================================================================
// Uses sequential keys that may cluster due to identity hash.
// Stresses collision handling in probing schemes.

template <typename Map>
auto benchSequentialKeys(std::size_t n) -> Stats {
  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (std::size_t i = 0; i < n; ++i) {
    int key = static_cast<int>(i);  // Sequential!
    auto ns = timeOp([&] {
      if constexpr (is_std_map<Map>::value) {
        map.insert({key, key});
      } else {
        map.insert(key, key);
      }
    });
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// WORKLOAD 8: Mixed Read-Heavy (80% read, 10% insert, 10% delete)
// ============================================================================
// Realistic workload for caches/indexes where reads dominate.

template <typename Map>
auto benchMixedReadHeavy(std::size_t initial_size, std::size_t num_ops, std::mt19937& rng) -> Stats {
  Map map;
  std::vector<int> keys;
  keys.reserve(initial_size * 2);

  // Fill to initial size
  for (std::size_t i = 0; i < initial_size; ++i) {
    int key = static_cast<int>(rng());
    keys.push_back(key);
    if constexpr (is_std_map<Map>::value) {
      map.insert({key, key});
    } else {
      map.insert(key, key);
    }
  }

  std::uniform_real_distribution<double> op_dist(0.0, 1.0);
  std::uniform_int_distribution<std::size_t> idx_dist(0, keys.size() - 1);

  volatile int sink = 0;
  std::vector<std::uint64_t> times;
  times.reserve(num_ops);

  for (std::size_t i = 0; i < num_ops; ++i) {
    double op = op_dist(rng);
    std::uint64_t ns;

    if (op < 0.80) {
      // 80% read
      int key = keys[idx_dist(rng)];
      ns = timeOp([&] {
        if constexpr (is_std_map<Map>::value) {
          auto it = map.find(key);
          if (it != map.end()) sink = it->second;
        } else {
          auto* p = map.find(key);
          if (p) sink = *p;
        }
      });
    } else if (op < 0.90) {
      // 10% insert
      int new_key = static_cast<int>(rng());
      ns = timeOp([&] {
        if constexpr (is_std_map<Map>::value) {
          map.insert({new_key, new_key});
        } else {
          map.insert(new_key, new_key);
        }
      });
      keys.push_back(new_key);
      idx_dist = std::uniform_int_distribution<std::size_t>(0, keys.size() - 1);
    } else {
      // 10% delete
      std::size_t idx = idx_dist(rng);
      int key = keys[idx];
      ns = timeOp([&] {
        if constexpr (is_std_map<Map>::value) {
          map.erase(key);
        } else {
          map.erase(key);
        }
      });
      // Swap-remove
      keys[idx] = keys.back();
      keys.pop_back();
      if (!keys.empty()) {
        idx_dist = std::uniform_int_distribution<std::size_t>(0, keys.size() - 1);
      }
    }
    times.push_back(ns);
  }
  return Stats::compute(times);
}

// ============================================================================
// Output Formatting
// ============================================================================

void printHeader() {
  std::println("╔══════════════════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                         COMPREHENSIVE HASH MAP BENCHMARK                                 ║");
  std::println("╠══════════════════════════════════════════════════════════════════════════════════════════╣");
  std::println("║  Tests designed to reveal strengths/weaknesses of different hash table designs:          ║");
  std::println("║  - Swiss Table (SIMD metadata): Fast probing, good cache locality                        ║");
  std::println("║  - F14 (overflow counting): No tombstone degradation, self-cleaning                      ║");
  std::println("║  - Robin Hood: Variance reduction, but expensive erase                                   ║");
  std::println("║  - Linear probing: Simple, cache-friendly, but clustering issues                         ║");
  std::println("╚══════════════════════════════════════════════════════════════════════════════════════════╝");
  std::println("");
}

void printTestHeader(const std::string& name, const std::string& description) {
  std::println("┌──────────────────────────────────────────────────────────────────────────────────────────┐");
  std::println("│ {:<88} │", name);
  std::println("│ {:<88} │", description);
  std::println("└──────────────────────────────────────────────────────────────────────────────────────────┘");
}

void printResult(const std::string& map_name, const Stats& stats) {
  std::println("  {:<24} p50: {:>6.0f} ns  p99: {:>6.0f} ns  mean: {:>6.0f} ns",
               map_name, stats.p50, stats.p99, stats.mean);
}

void printComparisonTable(const std::vector<BenchResult>& results) {
  if (results.empty()) return;

  // Sort by median
  auto sorted = results;
  std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.stats.p50 < b.stats.p50; });

  double best = sorted[0].stats.p50;

  std::println("");
  std::println("  Rank  Map                      p50      p90      p99    vs Best");
  std::println("  ────  ───────────────────────  ───────  ───────  ───────  ───────");

  for (std::size_t i = 0; i < sorted.size(); ++i) {
    const auto& r = sorted[i];
    double ratio = r.stats.p50 / best;
    std::string ratio_str = (ratio < 1.01) ? "1.00x" : std::format("{:.2f}x", ratio);
    std::println("  {:>4}  {:<24} {:>6.0f}   {:>6.0f}   {:>6.0f}   {:>7}",
                 i + 1, r.map_name, r.stats.p50, r.stats.p90, r.stats.p99, ratio_str);
  }
  std::println("");
}

// ============================================================================
// Benchmark Runner with Thread-based Timeout
// ============================================================================
// Each test runs in a separate thread. If it exceeds the timeout, we abandon
// the thread (it will eventually complete, but we won't wait) and blacklist
// the map for remaining tests. Only one test runs at a time to prevent crosstalk.

template <typename BenchFn>
auto runBenchmark(const std::string& test_name, BenchFn&& fn) -> std::vector<BenchResult> {
  std::vector<BenchResult> results;

  auto run = [&](const std::string& name, auto map_factory) {
    // Skip blacklisted maps
    if (g_slow_maps.contains(name)) {
      std::println("  Skipping {:.<30} (previously timed out)", name + " ");
      return;
    }

    std::print("  Running {:.<30}", name + " ");
    std::cout << std::flush;

    // Launch benchmark in a separate thread using std::async
    auto future = std::async(std::launch::async, [&fn, map_factory]() {
      std::mt19937 rng{42};  // Fresh RNG per test for reproducibility
      return fn(map_factory, rng);
    });

    // Wait with timeout
    auto timeout = std::chrono::duration<double>(config::kTimeoutSeconds);
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
      // Thread is still running - abandon it (will complete eventually)
      std::println(" TIMEOUT (>{:.0f}s) - abandoned", config::kTimeoutSeconds);
      g_slow_maps.insert(name);
      results.push_back({name + " [SLOW]", test_name, Stats{.p50 = 999999}, 0.0});
      // Note: The thread continues running in background but we don't wait
      // This is safe because we don't share mutable state with it
    } else {
      // Thread completed - get result
      auto stats = future.get();

      if (stats.p50 >= 999999) {
        // Internal timeout during setup
        std::println(" TIMEOUT (setup)");
        g_slow_maps.insert(name);
        results.push_back({name + " [SLOW]", test_name, Stats{.p50 = 999999}, 0.0});
      } else {
        std::println(" p50={:>4.0f}  p90={:>4.0f}  p99={:>5.0f} ns",
                     stats.p50, stats.p90, stats.p99);
        results.push_back({name, test_name, stats, 0.0});
      }
    }
  };

  // All maps - slow ones get auto-skipped after timeout
  // Tests run sequentially (one at a time) to prevent crosstalk

  // Linear probing family
  run("Linear Probing", [] { return LinearProbingMap<int, int>{}; });
  run("Linear Reuse", [] { return LinearProbingReuseMap<int, int>{}; });
  run("Quadratic Probing", [] { return QuadraticProbingMap<int, int>{}; });
  run("Double Hashing", [] { return DoubleHashingMap<int, int>{}; });
  run("Robin Hood", [] { return RobinHoodMap<int, int>{}; });
  run("Graveyard", [] { return GraveyardMap<int, int>{}; });

  // Other schemes
  run("Separate Chaining", [] { return SeparateChainingMap<int, int>{}; });
  run("Cuckoo", [] { return CuckooMap<int, int>{}; });
  run("Hopscotch", [] { return HopscotchMap<int, int>{}; });
  run("Two-Choice", [] { return TwoChoiceMap<int, int>{}; });

  // Swiss Table variants
  run("Swiss Table 16", [] { return SwissTable<int, int>{}; });
  run("Swiss Table 32", [] { return SwissTable32<int, int>{}; });
  run("Swiss Prefetch", [] { return SwissTablePrefetch<int, int>{}; });
  run("Swiss Graveyard", [] { return SwissTableGraveyard<int, int>{}; });
  run("Swiss32 Prefetch", [] { return SwissTable32Prefetch<int, int>{}; });
  run("Swiss32 Pf+Gr", [] { return SwissTable32PrefetchGraveyard<int, int>{}; });

  // F14 variants
  run("F14", [] { return F14Map<int, int>{}; });
  run("F14 Prefetch", [] { return F14Prefetch<int, int>{}; });

  // Baselines
  run("std::unordered_map", [] { return std::unordered_map<int, int>{}; });
  run("std::map", [] { return std::map<int, int>{}; });

  return results;
}

// Specialized runner for large value benchmarks (with thread timeout)
template <typename BenchFn>
auto runLargeValueBenchmark(const std::string& test_name, BenchFn&& fn) -> std::vector<BenchResult> {
  std::vector<BenchResult> results;

  auto run = [&](const std::string& name, auto map_factory) {
    if (g_slow_maps.contains(name)) {
      std::println("  Skipping {:.<30} (previously timed out)", name + " ");
      return;
    }

    std::print("  Running {:.<30}", name + " ");
    std::cout << std::flush;

    auto future = std::async(std::launch::async, [&fn, map_factory]() {
      std::mt19937 rng{42};
      return fn(map_factory, rng);
    });

    auto timeout = std::chrono::duration<double>(config::kTimeoutSeconds);
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
      std::println(" TIMEOUT (>{:.0f}s) - abandoned", config::kTimeoutSeconds);
      g_slow_maps.insert(name);
    } else {
      auto stats = future.get();
      std::println(" {:>6.0f} ns (p50)", stats.p50);
      results.push_back({name, test_name, stats, 0.0});
    }
  };

  // Representative selection for large values (full set would be too slow)
  run("Linear Probing", [] { return LinearProbingMap<int, LargeValue>{}; });
  run("Robin Hood", [] { return RobinHoodMap<int, LargeValue>{}; });
  run("Separate Chaining", [] { return SeparateChainingMap<int, LargeValue>{}; });
  run("Swiss Table 16", [] { return SwissTable<int, LargeValue>{}; });
  run("Swiss Table 32", [] { return SwissTable32<int, LargeValue>{}; });
  run("Swiss32 Prefetch", [] { return SwissTable32Prefetch<int, LargeValue>{}; });
  run("F14", [] { return F14Map<int, LargeValue>{}; });
  run("std::unordered_map", [] { return std::unordered_map<int, LargeValue>{}; });

  return results;
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  printHeader();

  std::println("Target: AMD Ryzen AI 9 HX 370 (Zen 5)");
  std::println("  L1: 48KB/core, L2: 1MB/core, L3: 24MB shared");
  std::println("  Test sizes: L1=2K, L2=50K, L3=1M, RAM=5M entries");
  std::println("  All maps tested, slow ones auto-skipped after {}s timeout", config::kTimeoutSeconds);
  std::println("");

  std::vector<std::vector<BenchResult>> all_results;

  // =========================================================================
  // PHASE 1: Cache Hierarchy Tests (Find Hit at each level)
  // =========================================================================

  // L1 Cache (2K entries, ~32KB)
  {
    printTestHeader("TEST 1: Find Hit - L1 Cache (2K entries, ~32KB)",
                    "Best-case: data fits entirely in L1. Shows raw algorithm speed.");

    auto results = runBenchmark("Find-L1", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchFindHit<Map>(config::kL1, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // L2 Cache (50K entries, ~800KB)
  {
    printTestHeader("TEST 2: Find Hit - L2 Cache (50K entries, ~800KB)",
                    "Data fits in L2 but not L1. Some cache misses on access.");

    auto results = runBenchmark("Find-L2", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchFindHit<Map>(config::kL2, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // L3 Cache (1M entries, ~16MB)
  {
    printTestHeader("TEST 3: Find Hit - L3 Cache (1M entries, ~16MB)",
                    "Data fits in L3 but not L2. Significant cache pressure.");

    auto results = runBenchmark("Find-L3", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchFindHit<Map>(config::kL3, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // RAM (5M entries, ~80MB)
  {
    printTestHeader("TEST 4: Find Hit - RAM (5M entries, ~80MB)",
                    "Exceeds L3. Memory-bound - shows true DRAM latency impact.");

    auto results = runBenchmark("Find-RAM", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchFindHit<Map>(config::kRAM, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // =========================================================================
  // PHASE 2: Find Miss (probing to empty slot)
  // =========================================================================
  {
    printTestHeader("TEST 5: Find Miss - L3 size (1M entries)",
                    "Lookup non-existent keys. Must probe to empty slot.");

    auto results = runBenchmark("Miss-L3", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchFindMiss<Map>(config::kL3, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // =========================================================================
  // PHASE 3: Tombstone Tests (CRITICAL for F14 vs Swiss comparison)
  // =========================================================================

  // Post-delete lookup
  {
    printTestHeader("TEST 6: Post-Delete Lookup (500K → delete 50% → lookup)",
                    "CRITICAL: Tombstone impact. F14 overflow counting should help.");

    auto results = runBenchmark("PostDel-500K", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchPostDeleteLookup<Map>(config::kSteadyState, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // LRU simulation (heavy tombstone accumulation)
  {
    printTestHeader("TEST 7: LRU Simulation (500K table, 2M insert-delete pairs)",
                    "CRITICAL: Continuous churn. Tombstones accumulate without bound.");

    auto results = runBenchmark("LRU-500K", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchLruSimulation<Map>(config::kSteadyState, config::kSteadyStateOps, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // =========================================================================
  // PHASE 4: Specialized Workloads
  // =========================================================================

  // Mixed read-heavy (realistic workload)
  {
    printTestHeader("TEST 8: Mixed Workload (200K table, 80% read / 10% ins / 10% del)",
                    "Realistic cache/index workload with some churn.");

    auto results = runBenchmark("Mixed-500K", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchMixedReadHeavy<Map>(config::kSteadyState, config::kSteadyStateOps, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // Sequential keys (clustering stress)
  {
    printTestHeader("TEST 9: Sequential Keys (50K sequential integers)",
                    "Stresses collision handling. Bad hash → clustering.");

    auto results = runBenchmark("Seq-50K", [](auto factory, std::mt19937&) {
      using Map = decltype(factory());
      return benchSequentialKeys<Map>(config::kL2);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // Large values (cache line traversal)
  {
    printTestHeader("TEST 10: Large Values (50K × 64-byte values)",
                    "Metadata separation benefit. Swiss should win here.");

    auto results = runLargeValueBenchmark("LargeVal-50K", [](auto factory, std::mt19937& rng) {
      using Map = decltype(factory());
      return benchLargeValueFind<Map>(config::kL2, rng);
    });
    printComparisonTable(results);
    all_results.push_back(results);
  }

  // =========================================================================
  // Final Summary
  // =========================================================================
  std::println("");
  std::println("╔══════════════════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                                    SUMMARY                                               ║");
  std::println("╠══════════════════════════════════════════════════════════════════════════════════════════╣");
  std::println("║  Test                    Best Performer              vs std::unordered_map               ║");
  std::println("╠══════════════════════════════════════════════════════════════════════════════════════════╣");

  for (const auto& test_results : all_results) {
    if (test_results.empty()) continue;

    auto sorted = test_results;
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.stats.p50 < b.stats.p50; });

    // Find std::unordered_map result
    double umap_p50 = 0;
    for (const auto& r : test_results) {
      if (r.map_name == "std::unordered_map") {
        umap_p50 = r.stats.p50;
        break;
      }
    }

    double speedup = (umap_p50 > 0) ? umap_p50 / sorted[0].stats.p50 : 0;
    std::println("║  {:<22}  {:<26}  {:.2f}x faster                       ║",
                 sorted[0].test_name, sorted[0].map_name, speedup);
  }

  std::println("╚══════════════════════════════════════════════════════════════════════════════════════════╝");
  std::println("");

  std::println("Key Insights:");
  std::println("  • Tests 1-4: Cache hierarchy impact (L1 → L2 → L3 → RAM)");
  std::println("  • Tests 6-7: CRITICAL - tombstone accumulation (F14 vs Swiss)");
  std::println("  • Test 10: Large values favor metadata separation (Swiss Table)");
  std::println("  • Look for: which maps degrade from L1 → RAM, which maintain performance after deletes");
  std::println("");

  return 0;
}
