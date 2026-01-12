#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <print>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Original maps
#include "containers/maps/anti_robin_hood.h"
// NOTE: coalesced.h excluded - O(n) insert makes it too slow for benchmarks
#include "containers/maps/cuckoo.h"
#include "containers/maps/double_hashing.h"
// NOTE: extendible.h excluded - O(n) operations make it too slow for benchmarks
#include "containers/maps/hopscotch.h"
#include "containers/maps/linear_probing.h"
#include "containers/maps/quadratic_probing.h"
#include "containers/maps/robin_hood.h"
#include "containers/maps/separate_chaining.h"
#include "containers/maps/swiss_table.h"
#include "containers/maps/swiss_table_64.h"
#include "containers/maps/two_choice.h"

// New maps
#include "containers/maps/f14.h"
#include "containers/maps/f14_prefetch.h"
#include "containers/maps/graveyard.h"
#include "containers/maps/linear_probing_reuse.h"
#include "containers/maps/swiss_table_32.h"
#include "containers/maps/swiss_table_32_graveyard.h"
#include "containers/maps/swiss_table_32_prefetch.h"
#include "containers/maps/swiss_table_32_prefetch_graveyard.h"
#include "containers/maps/swiss_table_graveyard.h"
#include "containers/maps/swiss_table_prefetch.h"
#include "containers/maps/swiss_table_bloom.h"
#include "containers/maps/swiss_table_reuse.h"

using namespace tempura;

// ============================================================================
// Configuration
// ============================================================================

constexpr std::size_t kInitialSize = 500'000;   // Initial bulk insert
constexpr std::size_t kMixedOps = 200'000;      // Mixed operations after
constexpr double kLookupRatio = 0.80;           // 80% lookups
constexpr double kInsertRatio = 0.10;           // 10% inserts
constexpr double kEraseRatio = 0.10;            // 10% erases


// ============================================================================
// Benchmark Result
// ============================================================================

struct BenchResult {
  std::string name;
  double insert_ns;      // Median ns per insert
  double mixed_ns;       // Median ns per mixed op
  double total_ms;       // Total time in ms
  bool success{true};
};

// ============================================================================
// Helper to detect std:: maps
// ============================================================================

template <typename T>
struct is_std_map : std::false_type {};
template <typename K, typename V, typename... Args>
struct is_std_map<std::unordered_map<K, V, Args...>> : std::true_type {};
template <typename K, typename V, typename... Args>
struct is_std_map<std::map<K, V, Args...>> : std::true_type {};

// ============================================================================
// Benchmark Runner
// ============================================================================

template <typename Map>
auto runBenchmark(const std::string& name) -> BenchResult {
  BenchResult result;
  result.name = name;

  std::mt19937 rng{42};
  std::uniform_int_distribution<int> key_dist(0, static_cast<int>(kInitialSize * 10));

  auto start_total = std::chrono::high_resolution_clock::now();

  // Phase 1: Bulk insert
  std::vector<std::uint64_t> insert_times;
  insert_times.reserve(kInitialSize);

  Map map;
  std::vector<int> inserted_keys;
  inserted_keys.reserve(kInitialSize);

  for (std::size_t i = 0; i < kInitialSize; ++i) {
    int key = key_dist(rng);

    auto t1 = std::chrono::high_resolution_clock::now();
    if constexpr (is_std_map<Map>::value) {
      map.insert({key, static_cast<int>(i)});
    } else {
      map.insert(key, static_cast<int>(i));
    }
    auto t2 = std::chrono::high_resolution_clock::now();

    insert_times.push_back(
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()));
    inserted_keys.push_back(key);
  }

  // Phase 2: Mixed workload with uniform distribution (samples full hash space)
  std::vector<std::uint64_t> mixed_times;
  mixed_times.reserve(kMixedOps);

  std::uniform_real_distribution<double> op_dist(0.0, 1.0);

  volatile int sink = 0;  // Prevent optimization

  for (std::size_t i = 0; i < kMixedOps; ++i) {
    double op = op_dist(rng);
    std::uniform_int_distribution<std::size_t> idx_dist(0, inserted_keys.size() - 1);
    int key = inserted_keys[idx_dist(rng)];

    auto t1 = std::chrono::high_resolution_clock::now();

    if (op < kLookupRatio) {
      // Lookup (80%)
      if constexpr (is_std_map<Map>::value) {
        auto it = map.find(key);
        if (it != map.end()) sink = it->second;
      } else {
        auto* ptr = map.find(key);
        if (ptr) sink = *ptr;
      }
    } else if (op < kLookupRatio + kInsertRatio) {
      // Insert (10%)
      int new_key = key_dist(rng);
      if constexpr (is_std_map<Map>::value) {
        map.insert({new_key, static_cast<int>(i)});
      } else {
        map.insert(new_key, static_cast<int>(i));
      }
      inserted_keys.push_back(new_key);
    } else {
      // Erase (10%)
      if constexpr (is_std_map<Map>::value) {
        map.erase(key);
      } else {
        map.erase(key);
      }
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    mixed_times.push_back(
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()));
  }

  auto end_total = std::chrono::high_resolution_clock::now();

  // Compute medians
  std::sort(insert_times.begin(), insert_times.end());
  std::sort(mixed_times.begin(), mixed_times.end());

  result.insert_ns = static_cast<double>(insert_times[insert_times.size() / 2]);
  result.mixed_ns = static_cast<double>(mixed_times[mixed_times.size() / 2]);
  result.total_ms = std::chrono::duration<double, std::milli>(end_total - start_total).count();

  return result;
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("╔══════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                        QUICK HASH MAP BENCHMARK                              ║");
  std::println("╠══════════════════════════════════════════════════════════════════════════════╣");
  std::println("║  Workload: {} bulk inserts, then {} mixed ops (uniform)          ║", kInitialSize, kMixedOps);
  std::println("║  Mix: {}% lookup, {}% insert, {}% erase                                      ║",
               static_cast<int>(kLookupRatio * 100),
               static_cast<int>(kInsertRatio * 100),
               static_cast<int>(kEraseRatio * 100));
  std::println("╚══════════════════════════════════════════════════════════════════════════════╝");
  std::println("");

  std::vector<BenchResult> results;

  auto bench = [&](const std::string& name, auto fn) {
    std::print("  Running {:.<30}", name + " ");
    std::cout << std::flush;
    auto result = fn();
    results.push_back(result);
    std::println(" {:>6.0f} ms", result.total_ms);
  };

  // Original maps
  bench("Linear Probing", [] { return runBenchmark<LinearProbingMap<int, int>>("Lin"); });
  bench("Quadratic Probing", [] { return runBenchmark<QuadraticProbingMap<int, int>>("Quad"); });
  bench("Double Hashing", [] { return runBenchmark<DoubleHashingMap<int, int>>("Dbl"); });
  bench("Robin Hood", [] { return runBenchmark<RobinHoodMap<int, int>>("Robin"); });
  bench("Anti-Robin Hood", [] { return runBenchmark<AntiRobinHoodMap<int, int>>("Anti"); });
  bench("Separate Chaining", [] { return runBenchmark<SeparateChainingMap<int, int>>("Chain"); });
  bench("Cuckoo", [] { return runBenchmark<CuckooMap<int, int>>("Cuck"); });
  bench("Hopscotch", [] { return runBenchmark<HopscotchMap<int, int>>("Hop"); });
  bench("Two-Choice", [] { return runBenchmark<TwoChoiceMap<int, int>>("2Ch"); });
  // Coalesced excluded - O(n) insert makes it too slow
  // Extendible excluded - O(n) operations make it too slow
  bench("Swiss Table 16", [] { return runBenchmark<SwissTable<int, int>>("Sw16"); });
  bench("Swiss Table 32", [] { return runBenchmark<SwissTable32<int, int>>("Sw32"); });
  bench("Swiss Table 64", [] { return runBenchmark<SwissTable64<int, int>>("Sw64"); });
  bench("F14", [] { return runBenchmark<F14Map<int, int>>("F14"); });
  bench("F14 Prefetch", [] { return runBenchmark<F14Prefetch<int, int>>("F14P"); });

  // New tombstone/optimization variants
  bench("Linear Reuse", [] { return runBenchmark<LinearProbingReuseMap<int, int>>("LinR"); });
  bench("Swiss Reuse", [] { return runBenchmark<SwissTableReuse<int, int>>("SwR"); });
  bench("Swiss Prefetch", [] { return runBenchmark<SwissTablePrefetch<int, int>>("SwPf"); });
  bench("Graveyard (Linear)", [] { return runBenchmark<GraveyardMap<int, int>>("Grave"); });
  bench("Swiss Graveyard", [] { return runBenchmark<SwissTableGraveyard<int, int>>("SwGr"); });

  // Swiss Table 32 variants
  bench("Swiss32 Prefetch", [] { return runBenchmark<SwissTable32Prefetch<int, int>>("S32P"); });
  bench("Swiss32 Graveyard", [] { return runBenchmark<SwissTable32Graveyard<int, int>>("S32G"); });
  bench("Swiss32 Pf+Gr", [] { return runBenchmark<SwissTable32PrefetchGraveyard<int, int>>("S32PG"); });

  // Bloom filter variant
  bench("Swiss Bloom", [] { return runBenchmark<SwissTableBloom<int, int>>("SwBl"); });

  // Baselines
  bench("std::unordered_map", [] { return runBenchmark<std::unordered_map<int, int>>("UMap"); });
  bench("std::map", [] { return runBenchmark<std::map<int, int>>("Tree"); });

  // Sort by total time
  std::sort(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return a.total_ms < b.total_ms; });

  std::println("");
  std::println("╔══════════════════════════════════════════════════════════════════════════════╗");
  std::println("║                              RESULTS (sorted)                                ║");
  std::println("╠══════════════════════════════════════════════════════════════════════════════╣");
  std::println("║  {:.<24} {:>10} {:>10} {:>10} {:>10}  ║", "Map", "Insert", "Mixed", "Total", "vs Best");
  std::println("║  {:.<24} {:>10} {:>10} {:>10} {:>10}  ║", "", "(ns)", "(ns)", "(ms)", "");
  std::println("╠══════════════════════════════════════════════════════════════════════════════╣");

  double best_total = results[0].total_ms;
  for (const auto& r : results) {
    double ratio = r.total_ms / best_total;
    std::string ratio_str = (ratio < 1.01) ? "1.00x" : std::format("{:.2f}x", ratio);
    std::println("║  {:.<24} {:>10.0f} {:>10.0f} {:>10.1f} {:>10}  ║",
                 r.name, r.insert_ns, r.mixed_ns, r.total_ms, ratio_str);
  }

  std::println("╚══════════════════════════════════════════════════════════════════════════════╝");

  // Category winners
  std::println("");
  std::println("Category Analysis:");

  auto find_best = [&](auto pred) -> const BenchResult* {
    const BenchResult* best = nullptr;
    for (const auto& r : results) {
      if (pred(r.name)) {
        if (!best || r.total_ms < best->total_ms) best = &r;
      }
    }
    return best;
  };

  // Swiss variants
  auto best_swiss = find_best([](const std::string& n) {
    return n.find("Swiss") != std::string::npos;
  });
  if (best_swiss) {
    std::println("  Best Swiss variant: {} ({:.1f} ms)", best_swiss->name, best_swiss->total_ms);
  }

  // Linear probing variants
  auto best_linear = find_best([](const std::string& n) {
    return n.find("Linear") != std::string::npos || n.find("Graveyard") != std::string::npos;
  });
  if (best_linear) {
    std::println("  Best Linear/Graveyard: {} ({:.1f} ms)", best_linear->name, best_linear->total_ms);
  }

  // vs std::unordered_map
  auto umap = find_best([](const std::string& n) { return n == "std::unordered_map"; });
  if (umap && !results.empty()) {
    double speedup = umap->total_ms / results[0].total_ms;
    std::println("  Best map is {:.2f}x faster than std::unordered_map", speedup);
  }

  return 0;
}
