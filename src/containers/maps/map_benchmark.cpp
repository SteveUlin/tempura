#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <print>
#include <random>
#include <string>
#include <vector>

#include "containers/maps/anti_robin_hood.h"
#include "containers/maps/cuckoo.h"
#include "containers/maps/double_hashing.h"
#include "containers/maps/hopscotch.h"
#include "containers/maps/linear_probing.h"
#include "containers/maps/quadratic_probing.h"
#include "containers/maps/robin_hood.h"
#include "containers/maps/separate_chaining.h"
#include "containers/maps/two_choice.h"
#include "plot.h"

using namespace tempura;

// ============================================================================
// Map Registry
// ============================================================================

struct MapInfo {
  std::string name;
  std::string short_name;
  RGB color;
};

inline constexpr std::size_t kNumMaps = 9;

inline const std::array<MapInfo, kNumMaps> kMaps = {{
    {"Linear", "Lin", colors::kCyan},
    {"Quadratic", "Quad", colors::kGreen},
    {"Double", "Dbl", colors::kMagenta},
    {"Robin Hood", "Robin", colors::kYellow},
    {"Anti-Robin", "Anti", colors::kRed},
    {"Chaining", "Chain", colors::kBlue},
    {"Cuckoo", "Cuck", {255, 165, 0}},     // Orange
    {"Hopscotch", "Hop", {180, 100, 255}}, // Purple
    {"Two-Choice", "2Ch", {100, 200, 150}}, // Teal
}};

// ============================================================================
// Box Drawing
// ============================================================================

constexpr int kBoxWidth = 78;

void printBoxTop() {
  std::print("╔");
  for (int i = 0; i < kBoxWidth; ++i) std::print("═");
  std::println("╗");
}

void printBoxBottom() {
  std::print("╚");
  for (int i = 0; i < kBoxWidth; ++i) std::print("═");
  std::println("╝");
}

void printBoxSep() {
  std::print("╠");
  for (int i = 0; i < kBoxWidth; ++i) std::print("═");
  std::println("╣");
}

void printBoxLine(const std::string& text) {
  std::print("║");
  std::size_t printed = 0;
  for (char c : text) {
    if (printed >= static_cast<std::size_t>(kBoxWidth)) break;
    std::print("{}", c);
    ++printed;
  }
  while (printed < static_cast<std::size_t>(kBoxWidth)) {
    std::print(" ");
    ++printed;
  }
  std::println("║");
}

void printBoxCenter(const std::string& text) {
  int padding = (kBoxWidth - static_cast<int>(text.size())) / 2;
  if (padding < 0) padding = 0;
  std::string padded(padding, ' ');
  padded += text;
  printBoxLine(padded);
}

// ============================================================================
// Timing Utilities
// ============================================================================

template <typename Fn>
auto timeOps(Fn&& fn, std::size_t ops) -> double {
  auto start = std::chrono::high_resolution_clock::now();
  fn();
  auto end = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
  return static_cast<double>(ns.count()) / static_cast<double>(ops);
}

// ============================================================================
// Benchmark Functions
// ============================================================================

template <typename Map>
auto benchmarkInsert(std::size_t n, std::mt19937& rng) -> double {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  return timeOps(
      [&] {
        Map map;
        for (int key : keys) {
          map.insert(key, key);
        }
      },
      n);
}

template <typename Map>
auto benchmarkFind(std::size_t n, double hit_rate, std::mt19937& rng) -> double {
  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert(keys[i], keys[i]);
  }

  std::vector<int> lookups(n);
  std::uniform_real_distribution<double> dist{0.0, 1.0};
  for (std::size_t i = 0; i < n; ++i) {
    if (dist(rng) < hit_rate) {
      lookups[i] = keys[rng() % n];
    } else {
      lookups[i] = static_cast<int>(rng()) | 0x80000000;
    }
  }

  volatile int sink = 0;
  return timeOps(
      [&] {
        for (int key : lookups) {
          auto* ptr = map.find(key);
          if (ptr) sink = *ptr;
        }
      },
      n);
}

template <typename Map>
auto benchmarkMixed(std::size_t n, std::mt19937& rng) -> double {
  Map map;
  std::vector<int> existing_keys;
  existing_keys.reserve(n);

  std::uniform_real_distribution<double> op_dist{0.0, 1.0};
  volatile int sink = 0;

  return timeOps(
      [&] {
        for (std::size_t i = 0; i < n; ++i) {
          double r = op_dist(rng);
          if (r < 0.5 || existing_keys.empty()) {
            int key = static_cast<int>(rng());
            map.insert(key, key);
            existing_keys.push_back(key);
          } else if (r < 0.8) {
            int key = existing_keys[rng() % existing_keys.size()];
            auto* ptr = map.find(key);
            if (ptr) sink = *ptr;
          } else {
            std::size_t idx = rng() % existing_keys.size();
            map.erase(existing_keys[idx]);
            existing_keys[idx] = existing_keys.back();
            existing_keys.pop_back();
          }
        }
      },
      n);
}

// Pathological: 16 buckets - extreme clustering, defeats linear probing variants
struct SevereClusteringHash {
  auto operator()(int key) const -> std::size_t {
    return static_cast<std::size_t>(key & 0xF);
  }
};

struct ClusteringHash2 {
  auto operator()(int key) const -> std::size_t {
    return static_cast<std::size_t>((key >> 4) & 0x7) + 1;
  }
};

template <typename Map>
auto benchmarkClustering(std::size_t n, std::mt19937& rng) -> double {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  return timeOps(
      [&] {
        Map map;
        for (int key : keys) {
          map.insert(key, key);
        }
        for (int key : keys) {
          volatile auto* ptr = map.find(key);
          (void)ptr;
        }
      },
      n * 2);
}

// ============================================================================
// Benchmark Runner - Per-Operation Timing
// ============================================================================

struct OpStats {
  double min, q1, p50, p90, p99, max;
  double mean;
};

auto computeOpStats(std::vector<std::uint64_t>& samples) -> OpStats {
  std::sort(samples.begin(), samples.end());
  std::size_t n = samples.size();

  double sum = 0;
  for (auto s : samples) sum += static_cast<double>(s);

  return {
      .min = static_cast<double>(samples[0]),
      .q1 = static_cast<double>(samples[n * 25 / 100]),
      .p50 = static_cast<double>(samples[n / 2]),
      .p90 = static_cast<double>(samples[n * 90 / 100]),
      .p99 = static_cast<double>(samples[n * 99 / 100]),
      .max = static_cast<double>(samples[n - 1]),
      .mean = sum / static_cast<double>(n),
  };
}

struct BenchResult {
  std::string name;
  std::array<OpStats, kNumMaps> stats;
};

// Measure per-operation times in nanoseconds
template <typename Map>
auto collectInsertTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert(key, key);
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

template <typename Map>
auto collectFindTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert(keys[i], keys[i]);
  }

  // Shuffle for random access pattern
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto* ptr = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (ptr) sink = *ptr;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

template <typename Fn>
auto runBenchmark(const std::string& name, Fn&& fn) -> BenchResult {
  BenchResult result;
  result.name = name;

  for (std::size_t map_idx = 0; map_idx < kNumMaps; ++map_idx) {
    std::mt19937 rng{42};
    std::vector<std::uint64_t> times;

    switch (map_idx) {
      case 0: times = fn.template operator()<LinearProbingMap<int, int>>(rng); break;
      case 1: times = fn.template operator()<QuadraticProbingMap<int, int>>(rng); break;
      case 2: times = fn.template operator()<DoubleHashingMap<int, int>>(rng); break;
      case 3: times = fn.template operator()<RobinHoodMap<int, int>>(rng); break;
      case 4: times = fn.template operator()<AntiRobinHoodMap<int, int>>(rng); break;
      case 5: times = fn.template operator()<SeparateChainingMap<int, int>>(rng); break;
      case 6: times = fn.template operator()<CuckooMap<int, int>>(rng); break;
      case 7: times = fn.template operator()<HopscotchMap<int, int>>(rng); break;
      case 8: times = fn.template operator()<TwoChoiceMap<int, int>>(rng); break;
    }
    result.stats[map_idx] = computeOpStats(times);
  }

  return result;
}

template <template <typename, typename, typename...> class MapTemplate, typename Hash,
          typename... Extra>
auto benchmarkWithHash(std::size_t n, std::mt19937& rng) -> double {
  return benchmarkClustering<MapTemplate<int, int, Hash, Extra...>>(n, rng);
}

// ============================================================================
// Result Formatting
// ============================================================================

void printComparisonTable(const std::vector<BenchResult>& results) {
  std::println("");
  printBoxTop();
  printBoxCenter("PERFORMANCE COMPARISON (ns/op) - median");
  printBoxSep();

  // Compact header for 9 maps
  printBoxLine(" Test      Lin  Quad   Dbl Robin Anti Chain Cuck  Hop  2Ch");
  printBoxSep();

  for (const auto& r : results) {
    auto fmt = [](const OpStats& s) { return std::format("{:>4.0f}", s.p50);
    };

    auto line = std::format(" {:<8} {} {} {} {} {} {} {} {} {}",
                            r.name,
                            fmt(r.stats[0]), fmt(r.stats[1]), fmt(r.stats[2]),
                            fmt(r.stats[3]), fmt(r.stats[4]), fmt(r.stats[5]),
                            fmt(r.stats[6]), fmt(r.stats[7]), fmt(r.stats[8]));
    printBoxLine(line);
  }

  printBoxBottom();
}

// Box-and-whisker plot: min├──[box]──┤max with ● for median above box
void printBoxWhisker(const std::string& label, const std::array<OpStats, kNumMaps>& stats) {
  // Find global range across all maps (use p99 for scale, not max which has outliers)
  double global_min = stats[0].min;
  double global_p99 = stats[0].p99;
  for (const auto& s : stats) {
    global_min = std::min(global_min, s.min);
    global_p99 = std::max(global_p99, s.p99);
  }

  // Use log scale if range is large
  double range_ratio = global_p99 / std::max(global_min, 1.0);
  bool use_log = range_ratio > 50;

  auto scale = [&](double val) -> int {
    constexpr int kWidth = 50;
    // Clamp to p99 range for display
    val = std::min(val, global_p99 * 1.1);
    double display_max = global_p99 * 1.1;

    if (use_log) {
      double log_min = std::log(std::max(global_min, 1.0));
      double log_max = std::log(display_max);
      double log_val = std::log(std::max(val, 1.0));
      return static_cast<int>((log_val - log_min) / (log_max - log_min) * (kWidth - 1));
    } else {
      return static_cast<int>((val - global_min) / (display_max - global_min) * (kWidth - 1));
    }
  };

  constexpr int kWidth = 50;

  std::println("  {} {}:", label, use_log ? "(log scale)" : "");

  for (std::size_t i = 0; i < kNumMaps; ++i) {
    const auto& s = stats[i];
    int pos_min = scale(s.min);
    int pos_q1 = scale(s.q1);
    int pos_p50 = scale(s.p50);
    int pos_p90 = scale(s.p90);
    int pos_p99 = scale(s.p99);

    std::print("    {:>6} ", kMaps[i].short_name);

    // Line 1: median marker above
    for (int j = 0; j < kWidth; ++j) {
      if (j == pos_p50) {
        std::print("{}", kMaps[i].color.ansiPrefix());
        std::print("●");
        std::print("{}", RGB::ansiSuffix());
      } else {
        std::print(" ");
      }
    }
    std::println("");

    // Line 2: the box  min├───[████]───┤p99
    std::print("           ");
    for (int j = 0; j < kWidth; ++j) {
      if (j == pos_min) {
        std::print("├");
      } else if (j == pos_p99) {
        std::print("┤");
      } else if (j > pos_min && j < pos_p99) {
        if (j >= pos_q1 && j <= pos_p90) {
          std::print("{}", kMaps[i].color.ansiPrefix());
          std::print("█");
          std::print("{}", RGB::ansiSuffix());
        } else {
          std::print("─");
        }
      } else {
        std::print(" ");
      }
    }
    std::println(" {:>.0f} ns", s.p50);
  }

  std::println("           {:>.0f}{:>{}.0f} ns", global_min, global_p99, kWidth - 4);
  std::println("");
}

template <typename Map>
auto collectClusteringInsertTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert(key, key);
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// High load factor test - this is where Robin Hood should shine
// With good hash at 80%+ load, some elements naturally get displaced far
// Robin Hood reduces the MAXIMUM displacement, tightening the tail
template <typename Map>
auto collectHighLoadFindTimes(std::size_t capacity, double load_factor,
                               std::mt19937& rng) -> std::vector<std::uint64_t> {
  Map map;

  // Fill to target load factor
  std::size_t target = static_cast<std::size_t>(capacity * load_factor);
  std::vector<int> keys;
  keys.reserve(target);

  for (std::size_t i = 0; i < target; ++i) {
    int key = static_cast<int>(rng());
    map.insert(key, key);
    keys.push_back(key);
  }

  // Shuffle keys for random access pattern
  std::shuffle(keys.begin(), keys.end(), rng);

  // Now measure find times
  std::vector<std::uint64_t> times;
  times.reserve(keys.size());

  volatile int sink = 0;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto* ptr = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (ptr) sink = *ptr;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

void printHighLoadAnalysis() {
  std::println("");
  printBoxTop();
  printBoxCenter("HIGH LOAD FACTOR (good hash, 85% full)");
  printBoxLine("  Robin Hood should reduce tail latency by limiting max displacement");
  printBoxBottom();

  constexpr std::size_t capacity = 100000;  // Reduced for faster benchmarks
  constexpr double load_factor = 0.85;

  BenchResult result;
  result.name = "Find @85% load";

  std::mt19937 rng0{42}, rng1{42}, rng2{42}, rng3{42}, rng4{42}, rng5{42};
  std::mt19937 rng6{42}, rng7{42}, rng8{42};

  auto times0 = collectHighLoadFindTimes<LinearProbingMap<int, int>>(capacity, load_factor, rng0);
  auto times1 = collectHighLoadFindTimes<QuadraticProbingMap<int, int>>(capacity, load_factor, rng1);
  auto times2 = collectHighLoadFindTimes<DoubleHashingMap<int, int>>(capacity, load_factor, rng2);
  auto times3 = collectHighLoadFindTimes<RobinHoodMap<int, int>>(capacity, load_factor, rng3);
  auto times4 = collectHighLoadFindTimes<AntiRobinHoodMap<int, int>>(capacity, load_factor, rng4);
  auto times5 = collectHighLoadFindTimes<SeparateChainingMap<int, int>>(capacity, load_factor, rng5);
  auto times6 = collectHighLoadFindTimes<CuckooMap<int, int>>(capacity, load_factor, rng6);
  auto times7 = collectHighLoadFindTimes<HopscotchMap<int, int>>(capacity, load_factor, rng7);
  auto times8 = collectHighLoadFindTimes<TwoChoiceMap<int, int>>(capacity, load_factor, rng8);

  result.stats[0] = computeOpStats(times0);
  result.stats[1] = computeOpStats(times1);
  result.stats[2] = computeOpStats(times2);
  result.stats[3] = computeOpStats(times3);
  result.stats[4] = computeOpStats(times4);
  result.stats[5] = computeOpStats(times5);
  result.stats[6] = computeOpStats(times6);
  result.stats[7] = computeOpStats(times7);
  result.stats[8] = computeOpStats(times8);

  std::println("");
  printBoxWhisker(result.name, result.stats);

  // Compare Robin vs Anti-Robin specifically
  auto& robin = result.stats[3];
  auto& anti = result.stats[4];

  std::println("  Robin Hood vs Anti-Robin Hood:");
  std::println("    Median: Robin {:.0f} ns vs Anti {:.0f} ns", robin.p50, anti.p50);
  std::println("    P90:    Robin {:.0f} ns vs Anti {:.0f} ns", robin.p90, anti.p90);
  std::println("    P99:    Robin {:.0f} ns vs Anti {:.0f} ns", robin.p99, anti.p99);
  std::println("    Max:    Robin {:.0f} ns vs Anti {:.0f} ns", robin.max, anti.max);

  if (robin.p99 < anti.p99) {
    std::println("    → Robin Hood has {:.1f}x tighter P99 tail", anti.p99 / robin.p99);
  } else {
    std::println("    → No advantage observed (implementation overhead may dominate)");
  }
}

void printClusteringAnalysis() {
  std::println("");
  printBoxTop();
  printBoxCenter("CLUSTERING RESISTANCE (pathological hash → 16 buckets)");
  printBoxLine("  Note: Cuckoo/Hopscotch/TwoChoice skipped - they require good hashes");
  printBoxBottom();

  // Severely reduced - clustering tests are O(n²) for linear probing variants
  // 500 elements into 16 buckets is ~31 per bucket, manageable
  constexpr std::size_t n = 500;

  BenchResult result;
  result.name = "Clustered Insert";

  std::mt19937 rng0{42}, rng1{42}, rng2{42}, rng3{42}, rng4{42}, rng5{42};

  auto times0 = collectClusteringInsertTimes<LinearProbingMap<int, int, SevereClusteringHash>>(n, rng0);
  auto times1 = collectClusteringInsertTimes<QuadraticProbingMap<int, int, SevereClusteringHash>>(n, rng1);
  auto times2 = collectClusteringInsertTimes<DoubleHashingMap<int, int, SevereClusteringHash, ClusteringHash2>>(n, rng2);
  auto times3 = collectClusteringInsertTimes<RobinHoodMap<int, int, SevereClusteringHash>>(n, rng3);
  auto times4 = collectClusteringInsertTimes<AntiRobinHoodMap<int, int, SevereClusteringHash>>(n, rng4);
  auto times5 = collectClusteringInsertTimes<SeparateChainingMap<int, int, SevereClusteringHash>>(n, rng5);

  // Skip Cuckoo, Hopscotch, TwoChoice - they fundamentally break with pathological hashes:
  // - Cuckoo: needs independent h1/h2, pathological hashes cause infinite displacement
  // - Hopscotch: 32-slot neighborhood overflows when 31+ elements hash to same bucket
  // - TwoChoice: power-of-2 relies on h1≠h2, otherwise degrades to single-choice

  result.stats[0] = computeOpStats(times0);
  result.stats[1] = computeOpStats(times1);
  result.stats[2] = computeOpStats(times2);
  result.stats[3] = computeOpStats(times3);
  result.stats[4] = computeOpStats(times4);
  result.stats[5] = computeOpStats(times5);
  // Placeholder stats for skipped maps (won't display meaningfully)
  result.stats[6] = {0, 0, 0, 0, 0, 0, 0};
  result.stats[7] = {0, 0, 0, 0, 0, 0, 0};
  result.stats[8] = {0, 0, 0, 0, 0, 0, 0};

  // Print only first 6 maps for clustering
  std::println("");
  std::println("  {} (first 6 maps only):", result.name);

  // Find global range across tested maps only
  double global_min = result.stats[0].min;
  double global_p99 = result.stats[0].p99;
  for (std::size_t i = 0; i < 6; ++i) {
    global_min = std::min(global_min, result.stats[i].min);
    global_p99 = std::max(global_p99, result.stats[i].p99);
  }

  bool use_log = (global_p99 / std::max(global_min, 1.0)) > 50;
  constexpr int kWidth = 50;

  auto scale = [&](double val) -> int {
    val = std::min(val, global_p99 * 1.1);
    double display_max = global_p99 * 1.1;
    if (use_log) {
      double log_min = std::log(std::max(global_min, 1.0));
      double log_max = std::log(display_max);
      double log_val = std::log(std::max(val, 1.0));
      return static_cast<int>((log_val - log_min) / (log_max - log_min) * (kWidth - 1));
    } else {
      return static_cast<int>((val - global_min) / (display_max - global_min) * (kWidth - 1));
    }
  };

  std::println("  {} {}:", result.name, use_log ? "(log scale)" : "");

  for (std::size_t i = 0; i < 6; ++i) {
    const auto& s = result.stats[i];
    int pos_min = scale(s.min);
    int pos_q1 = scale(s.q1);
    int pos_p50 = scale(s.p50);
    int pos_p90 = scale(s.p90);
    int pos_p99 = scale(s.p99);

    std::print("    {:>6} ", kMaps[i].short_name);

    for (int j = 0; j < kWidth; ++j) {
      if (j == pos_p50) {
        std::print("{}", kMaps[i].color.ansiPrefix());
        std::print("●");
        std::print("{}", RGB::ansiSuffix());
      } else {
        std::print(" ");
      }
    }
    std::println("");

    std::print("           ");
    for (int j = 0; j < kWidth; ++j) {
      if (j == pos_min) {
        std::print("├");
      } else if (j == pos_p99) {
        std::print("┤");
      } else if (j > pos_min && j < pos_p99) {
        if (j >= pos_q1 && j <= pos_p90) {
          std::print("{}", kMaps[i].color.ansiPrefix());
          std::print("█");
          std::print("{}", RGB::ansiSuffix());
        } else {
          std::print("─");
        }
      } else {
        std::print(" ");
      }
    }
    std::println(" {:>.0f} ns", s.p50);
  }

  std::println("           {:>.0f}{:>{}.0f} ns", global_min, global_p99, kWidth - 4);
  std::println("");

  std::println("  Analysis:");
  std::println("    Quadratic escapes clusters by jumping i² slots");
  std::println("    Chaining just grows linked lists (O(n/buckets) per op)");
  std::println("    Linear/Robin/Anti probe linearly → O(n) per op with bad hash");
  std::println("");
  std::println("  Note: Robin Hood reduces probe VARIANCE, not average length.");
  std::println("  Its advantage shows in P99 latency with good hashes, not bad ones.");
}

void printSummary() {
  std::println("");
  printBoxTop();
  printBoxCenter("SUMMARY");
  printBoxSep();
  printBoxLine("");
  printBoxLine("  Linear:      Best cache locality. Suffers primary clustering.");
  printBoxLine("  Quadratic:   Reduces primary clustering. Still has secondary clustering.");
  printBoxLine("  Double:      No clustering (key-dependent steps). Two hash computations.");
  printBoxLine("  Robin Hood:  Minimizes probe variance. Shines with realistic clustering.");
  printBoxLine("  Anti-Robin:  Pathological. Demonstrates why Robin Hood works.");
  printBoxLine("  Chaining:    No clustering, simple deletion. Poor cache locality.");
  printBoxLine("  Cuckoo:      O(1) worst-case lookup. Displacement chains on insert.");
  printBoxLine("  Hopscotch:   Bounded neighborhood. Bitmap-accelerated lookup.");
  printBoxLine("  Two-Choice:  Power of 2 choices. Exponentially better load balance.");
  printBoxLine("");
  printBoxBottom();
}

auto main() -> int {
  printBoxTop();
  printBoxCenter("HASH MAP STRATEGY BENCHMARK");
  printBoxLine("");
  printBoxLine("  Lin | Quad | Double | Robin | Anti | Chain | Cuckoo | Hop | 2-Choice");
  printBoxBottom();

  std::vector<BenchResult> results;
  constexpr std::size_t n = 200000;  // Reduced for faster benchmarks

  results.push_back(runBenchmark("Insert", [n]<typename Map>(std::mt19937& rng) {
    return collectInsertTimes<Map>(n, rng);
  }));

  results.push_back(runBenchmark("Find", [n]<typename Map>(std::mt19937& rng) {
    return collectFindTimes<Map>(n, rng);
  }));

  printComparisonTable(results);

  // Box-and-whisker plots showing distribution
  std::println("");
  printBoxTop();
  printBoxCenter("OPERATION TIME DISTRIBUTION (ns)");
  printBoxLine("  ● = median    ████ = q25-p90 box    ├──┤ = min to p99");
  printBoxBottom();
  std::println("");

  for (const auto& r : results) {
    printBoxWhisker(r.name, r.stats);
  }

  printClusteringAnalysis();
  printHighLoadAnalysis();
  printSummary();

  return 0;
}
