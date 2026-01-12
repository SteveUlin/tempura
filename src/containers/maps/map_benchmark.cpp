#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <print>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "containers/maps/anti_robin_hood.h"
// NOTE: coalesced.h excluded - O(n) insert makes it too slow for benchmarks
#include "containers/maps/cuckoo.h"
#include "containers/maps/double_hashing.h"
// NOTE: extendible.h excluded - O(n) operations make it too slow for benchmarks
#include "containers/maps/f14.h"
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
#include "containers/maps/swiss_table_graveyard.h"
#include "containers/maps/swiss_table_prefetch.h"
#include "containers/maps/swiss_table_bloom.h"
#include "containers/maps/swiss_table_reuse.h"
#include "containers/maps/two_choice.h"
#include "plot.h"

using namespace tempura;

// ============================================================================
// ANSI Terminal Control
// ============================================================================

namespace ansi {
inline void clearScreen() { std::print("\033[2J\033[H"); }
inline void moveCursor(int row, int col) { std::print("\033[{};{}H", row, col); }
inline void clearToEOL() { std::print("\033[K"); }
inline void hideCursor() { std::print("\033[?25l"); }
inline void showCursor() { std::print("\033[?25h"); }
inline void saveCursor() { std::print("\033[s"); }
inline void restoreCursor() { std::print("\033[u"); }
}  // namespace ansi

// ============================================================================
// Map Registry
// ============================================================================

struct MapInfo {
  std::string name;
  std::string short_name;
  RGB color;
};

// Added std::unordered_map and std::map for baseline comparison
inline constexpr std::size_t kNumMaps = 24;  // Coalesced/Extendible excluded - O(n) ops

inline const std::array<MapInfo, kNumMaps> kMaps = {{
    // Basic probing
    {"Linear Probing", "Lin", colors::kCyan},
    {"Linear Reuse", "LinR", {0, 200, 200}},         // Darker Cyan
    {"Graveyard (Linear)", "Grave", {0, 150, 150}},  // Teal
    {"Quadratic Probing", "Quad", colors::kGreen},
    {"Double Hashing", "Dbl", colors::kMagenta},
    {"Robin Hood", "Robin", colors::kYellow},
    {"Anti-Robin Hood", "Anti", colors::kRed},
    {"Separate Chaining", "Chain", colors::kBlue},
    // Advanced probing
    {"Cuckoo Hashing", "Cuck", {255, 165, 0}},       // Orange
    {"Hopscotch Hashing", "Hop", {180, 100, 255}},   // Purple
    {"Two-Choice Hash", "2Ch", {100, 200, 150}},     // Teal-green
    // Coalesced/Extendible excluded - O(n) operations make them too slow
    // Swiss Table 16-byte variants
    {"Swiss Table 16", "Sw16", {255, 100, 100}},     // Light Red
    {"Swiss Prefetch", "SwPf", {255, 120, 120}},     // Lighter Red
    {"Swiss Reuse", "SwR", {255, 140, 140}},         // Even Lighter
    {"Swiss Graveyard", "SwGr", {255, 80, 80}},      // Darker Red
    {"Swiss Bloom", "SwBl", {255, 60, 60}},          // Deep Red
    // Swiss Table 32-byte variants
    {"Swiss Table 32", "Sw32", {100, 255, 100}},     // Light Green
    {"Swiss32 Prefetch", "S32P", {120, 255, 120}},   // Lighter Green
    {"Swiss32 Graveyard", "S32G", {80, 255, 80}},    // Darker Green
    {"Swiss32 Pf+Gr", "S32PG", {60, 200, 60}},       // Forest Green
    // Swiss Table 64-byte and F14
    {"Swiss Table 64", "Sw64", {100, 100, 255}},     // Light Blue
    {"F14 Map", "F14", {255, 150, 200}},             // Pink
    // Baselines
    {"std::unordered_map", "UMap", {200, 200, 200}}, // White (baseline)
    {"std::map (RB-Tree)", "Tree", {100, 100, 100}}, // Dark Gray (tree-based)
}};

// ============================================================================
// Box Drawing
// ============================================================================

constexpr int kBoxWidth = 90;  // Wide enough for 15 map columns

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

void printLegend() {
  printBoxTop();
  printBoxCenter("MAP ABBREVIATION LEGEND");
  printBoxSep();
  printBoxLine("");
  printBoxLine("  Open Addressing (probing-based):");
  printBoxLine("    Lin   = Linear Probing        LinR  = Linear + Tombstone Reuse");
  printBoxLine("    Grave = Graveyard Hashing     Quad  = Quadratic Probing");
  printBoxLine("    Dbl   = Double Hashing        Robin = Robin Hood Hashing");
  printBoxLine("    Anti  = Anti-Robin Hood       Hop   = Hopscotch Hashing");
  printBoxLine("    Cuck  = Cuckoo Hashing        2Ch   = Two-Choice Hashing");
  printBoxLine("");
  printBoxLine("  Swiss Table 16-byte (SSE2):");
  printBoxLine("    Sw16  = Base Swiss Table      SwPf  = Swiss + Prefetch");
  printBoxLine("    SwR   = Swiss + Tombstone Reuse");
  printBoxLine("    SwGr  = Swiss + Graveyard");
  printBoxLine("");
  printBoxLine("  Swiss Table 32-byte (AVX2):");
  printBoxLine("    Sw32  = Base Swiss Table 32   S32P  = Swiss32 + Prefetch");
  printBoxLine("    S32G  = Swiss32 + Graveyard   S32PG = Swiss32 + Prefetch + Graveyard");
  printBoxLine("");
  printBoxLine("  Other SIMD:");
  printBoxLine("    Sw64  = Swiss Table (64-byte AVX-512)");
  printBoxLine("    F14   = Facebook F14 (14-slot chunks)");
  printBoxLine("");
  printBoxLine("  Other:");
  printBoxLine("    Chain = Separate Chaining     UMap  = std::unordered_map");
  printBoxLine("    Tree  = std::map (Red-Black Tree)");
  printBoxLine("");
  printBoxBottom();
}

// ============================================================================
// Spinner Animation
// ============================================================================

struct Spinner {
  std::atomic<bool> running_{false};
  std::thread thread_;
  std::string message_;
  int row_{0};
  int col_{0};

  static constexpr std::array<const char*, 10> kFrames = {
      "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};

  void start(const std::string& message, int row, int col) {
    message_ = message;
    row_ = row;
    col_ = col;
    running_ = true;

    thread_ = std::thread([this] {
      std::size_t frame = 0;
      while (running_) {
        ansi::saveCursor();
        ansi::moveCursor(row_, col_);
        std::print("{} {} {}", colors::kCyan.wrap(kFrames[frame]), message_,
                   std::string(20, ' '));
        std::cout << std::flush;
        ansi::restoreCursor();

        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        frame = (frame + 1) % kFrames.size();
      }
    });
  }

  void stop() {
    if (running_) {
      running_ = false;
      if (thread_.joinable()) {
        thread_.join();
      }
    }
  }

  ~Spinner() { stop(); }
};

// ============================================================================
// Benchmark Statistics
// ============================================================================

struct OpStats {
  double min, q1, p50, q3, p90, p99, max;
  double mean;
  double whisker_lo, whisker_hi;  // IQR-based bounds for outlier-robust plotting
  bool completed{false};
};

auto computeOpStats(std::vector<std::uint64_t>& samples) -> OpStats {
  if (samples.empty()) {
    return {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false};
  }

  std::sort(samples.begin(), samples.end());
  std::size_t n = samples.size();

  double sum = 0;
  for (auto s : samples) sum += static_cast<double>(s);

  double q1 = static_cast<double>(samples[n * 25 / 100]);
  double q3 = static_cast<double>(samples[n * 75 / 100]);
  double iqr = q3 - q1;

  // Tukey's fences: 1.5 * IQR beyond quartiles
  double whisker_lo = std::max(static_cast<double>(samples[0]), q1 - 1.5 * iqr);
  double whisker_hi = std::min(static_cast<double>(samples[n - 1]), q3 + 1.5 * iqr);

  return {
      .min = static_cast<double>(samples[0]),
      .q1 = q1,
      .p50 = static_cast<double>(samples[n / 2]),
      .q3 = q3,
      .p90 = static_cast<double>(samples[n * 90 / 100]),
      .p99 = static_cast<double>(samples[n * 99 / 100]),
      .max = static_cast<double>(samples[n - 1]),
      .mean = sum / static_cast<double>(n),
      .whisker_lo = whisker_lo,
      .whisker_hi = whisker_hi,
      .completed = true,
  };
}

struct BenchResult {
  std::string name;
  std::array<OpStats, kNumMaps> stats;
  std::size_t completed_count{0};
};

// ============================================================================
// Progressive Display System
// ============================================================================

struct DisplayState {
  std::vector<BenchResult> results;
  int current_map_idx{0};
  int current_test_idx{0};
  std::string current_map_name;

  void render() const {
    ansi::clearScreen();
    ansi::moveCursor(1, 1);

    // Header
    printBoxTop();
    auto header = std::format("HASH MAP BENCHMARK - Running: {}", current_map_name);
    printBoxCenter(header);
    printBoxSep();

    // Results table (compact - showing key representatives)
    // Indices: 0=Lin 2=Grave 5=Robin 11=Sw16 15=Sw32 16=S32P 19=Sw64 20=F14 21=UMap 22=Tree
    if (!results.empty()) {
      printBoxLine(" Test      Lin  Grave Robin Sw16  Sw32  S32P  Sw64  F14  UMap  Tree");
      printBoxSep();

      for (const auto& r : results) {
        auto fmt = [](const OpStats& s) {
          if (!s.completed) return std::string("  --");
          return std::format("{:>5.0f}", s.p50);
        };

        auto line = std::format(
            " {:<8} {} {} {} {} {} {} {} {} {} {}",
            r.name.substr(0, 8),
            fmt(r.stats[0]), fmt(r.stats[2]), fmt(r.stats[5]),
            fmt(r.stats[11]), fmt(r.stats[15]), fmt(r.stats[16]),
            fmt(r.stats[19]), fmt(r.stats[20]), fmt(r.stats[21]), fmt(r.stats[22]));
        printBoxLine(line);
      }
    }

    printBoxBottom();

    // Progress bar
    std::size_t total_tests = results.size() * kNumMaps;
    std::size_t completed = 0;
    for (const auto& r : results) {
      completed += r.completed_count;
    }

    std::println("");
    std::print("[");
    constexpr int kBarWidth = 50;
    int filled = total_tests > 0 ? static_cast<int>(completed * kBarWidth / total_tests) : 0;
    for (int i = 0; i < kBarWidth; ++i) {
      if (i < filled) {
        std::print("{}", colors::kGreen.wrap("█"));
      } else {
        std::print("░");
      }
    }
    std::println("] {}/{} benchmarks complete", completed, total_tests);
    std::println("");

    std::cout << std::flush;
  }

  void updateMapProgress(std::size_t test_idx, std::size_t map_idx, const OpStats& stats) {
    if (test_idx < results.size()) {
      results[test_idx].stats[map_idx] = stats;
      if (stats.completed && !results[test_idx].stats[map_idx].completed) {
        results[test_idx].completed_count++;
      }
      render();
    }
  }
};

// ============================================================================
// Dynamic Box-Whisker Plot with Progressive Updates
// ============================================================================
//
// Uses Tukey's IQR-based fences to exclude outliers from affecting scale:
//   - Whiskers extend to Q1 - 1.5*IQR and Q3 + 1.5*IQR (or data bounds)
//   - Box shows Q1 to Q3 (interquartile range)
//   - Outliers beyond whiskers don't affect the plot scale

void printProgressiveBoxWhisker(const std::string& label,
                                const std::array<OpStats, kNumMaps>& stats,
                                std::size_t completed_maps) {
  // Find best performer and categorize maps as "competitive" or "slow"
  // Competitive = within 2x of the best median
  double best_median = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < completed_maps; ++i) {
    if (stats[i].completed) {
      best_median = std::min(best_median, stats[i].p50);
    }
  }

  if (best_median == std::numeric_limits<double>::max()) {
    return;  // No data yet
  }

  double competitive_threshold = best_median * 2.0;

  // Separate competitive and slow maps
  std::vector<std::size_t> competitive_indices;
  std::vector<std::size_t> slow_indices;

  for (std::size_t i = 0; i < kNumMaps; ++i) {
    if (!stats[i].completed) continue;
    if (stats[i].p50 <= competitive_threshold) {
      competitive_indices.push_back(i);
    } else {
      slow_indices.push_back(i);
    }
  }

  // Find range for competitive maps only
  double global_lo = std::numeric_limits<double>::max();
  double global_hi = 0;

  for (std::size_t i : competitive_indices) {
    global_lo = std::min(global_lo, stats[i].whisker_lo);
    global_hi = std::max(global_hi, stats[i].whisker_hi);
  }

  if (competitive_indices.empty()) {
    return;
  }

  // Use log scale if range is large
  double range_ratio = global_hi / std::max(global_lo, 1.0);
  bool use_log = range_ratio > 50;

  // Add 10% padding to avoid edge crowding
  double display_min = global_lo * 0.9;
  double display_max = global_hi * 1.1;

  auto scale = [&](double val) -> int {
    constexpr int kWidth = 50;
    val = std::clamp(val, display_min, display_max);

    if (use_log) {
      double log_min = std::log(std::max(display_min, 1.0));
      double log_max = std::log(display_max);
      double log_val = std::log(std::max(val, 1.0));
      return static_cast<int>((log_val - log_min) / (log_max - log_min) * (kWidth - 1));
    } else {
      return static_cast<int>((val - display_min) / (display_max - display_min) * (kWidth - 1));
    }
  };

  constexpr int kWidth = 50;

  std::println("  {} {}:", label, use_log ? "(log scale)" : "");

  // Show detailed box-whisker for competitive maps
  for (std::size_t i : competitive_indices) {
    const auto& s = stats[i];

    int pos_wlo = scale(s.whisker_lo);
    int pos_q1 = scale(s.q1);
    int pos_p50 = scale(s.p50);
    int pos_q3 = scale(s.q3);
    int pos_whi = scale(s.whisker_hi);

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

    // Line 2: whisker_lo ├───[████]───┤ whisker_hi
    std::print("           ");
    for (int j = 0; j < kWidth; ++j) {
      if (j == pos_wlo) {
        std::print("├");
      } else if (j == pos_whi) {
        std::print("┤");
      } else if (j > pos_wlo && j < pos_whi) {
        if (j >= pos_q1 && j <= pos_q3) {
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

  std::println("           {:>.0f}{:>{}.0f} ns", global_lo, global_hi, kWidth - 4);

  // Show compact summary for slow maps (off-scale)
  if (!slow_indices.empty()) {
    std::print("    ────── ");
    std::print("{}", RGB{150, 150, 150}.ansiPrefix());
    std::print("Off-scale (>2x best): ");
    for (std::size_t idx = 0; idx < slow_indices.size(); ++idx) {
      std::size_t i = slow_indices[idx];
      double ratio = stats[i].p50 / best_median;
      std::print("{} ({:.0f}ns, {:.1f}x)", kMaps[i].short_name, stats[i].p50, ratio);
      if (idx + 1 < slow_indices.size()) std::print(", ");
    }
    std::print("{}", RGB::ansiSuffix());
    std::println("");
  }

  std::println("");
}

// ============================================================================
// Timing Collection Functions
// ============================================================================

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

// Sequential keys test cache behavior
template <typename Map>
auto collectSequentialInsertTimes(std::size_t n) -> std::vector<std::uint64_t> {
  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (std::size_t i = 0; i < n; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert(static_cast<int>(i), static_cast<int>(i));
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Zipfian distribution simulates realistic access patterns with hot keys
template <typename Map>
auto collectZipfianFindTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(i);
    map.insert(keys[i], keys[i]);
  }

  // Zipfian with α=1.0 (heavy skew - 80/20 rule)
  std::discrete_distribution<int> zipf{};
  std::vector<double> weights(n);
  for (std::size_t i = 0; i < n; ++i) {
    weights[i] = 1.0 / (i + 1);  // Zipf approximation
  }
  zipf = std::discrete_distribution<int>(weights.begin(), weights.end());

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (std::size_t i = 0; i < n; ++i) {
    int idx = zipf(rng);
    int key = keys[idx];
    auto start = std::chrono::high_resolution_clock::now();
    auto* ptr = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (ptr) sink = *ptr;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Lookup miss - searching for keys that don't exist (important for filters, caches)
template <typename Map>
auto collectFindMissTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  Map map;
  // Insert keys in range [0, n)
  for (std::size_t i = 0; i < n; ++i) {
    map.insert(static_cast<int>(i), static_cast<int>(i));
  }

  // Search for keys in range [n, 2n) - none exist
  std::vector<int> miss_keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    miss_keys[i] = static_cast<int>(n + i);
  }
  std::shuffle(miss_keys.begin(), miss_keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (int key : miss_keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto* ptr = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (ptr) sink = *ptr;  // Should never happen
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Erase benchmark - delete keys from a full map
template <typename Map>
auto collectEraseTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  Map map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert(keys[i], keys[i]);
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.erase(key);
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Mixed workload - alternating insert and erase (steady-state behavior)
template <typename Map>
auto collectMixedTimes(std::size_t n, std::mt19937& rng) -> std::vector<std::uint64_t> {
  Map map;
  // Pre-fill to 50% capacity
  std::vector<int> existing;
  for (std::size_t i = 0; i < n / 2; ++i) {
    int key = static_cast<int>(rng());
    map.insert(key, key);
    existing.push_back(key);
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::uniform_int_distribution<int> op_dist(0, 1);  // 50% insert, 50% erase

  for (std::size_t i = 0; i < n; ++i) {
    bool do_insert = op_dist(rng) == 0 || existing.empty();

    auto start = std::chrono::high_resolution_clock::now();
    if (do_insert) {
      int key = static_cast<int>(rng());
      map.insert(key, key);
      existing.push_back(key);
    } else {
      std::size_t idx = rng() % existing.size();
      int key = existing[idx];
      map.erase(key);
      existing[idx] = existing.back();
      existing.pop_back();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// ============================================================================
// Standard Library Map Adapters
// ============================================================================

// std::unordered_map adapter
auto collectInsertTimesStdUnordered(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::unordered_map<int, int> map;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert({key, key});
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

auto collectFindTimesStdUnordered(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::unordered_map<int, int> map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert({keys[i], keys[i]});
  }

  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

auto collectSequentialInsertTimesStdUnordered(std::size_t n)
    -> std::vector<std::uint64_t> {
  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::unordered_map<int, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert({static_cast<int>(i), static_cast<int>(i)});
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

auto collectZipfianFindTimesStdUnordered(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::unordered_map<int, int> map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(i);
    map.insert({keys[i], keys[i]});
  }

  std::discrete_distribution<int> zipf{};
  std::vector<double> weights(n);
  for (std::size_t i = 0; i < n; ++i) {
    weights[i] = 1.0 / (i + 1);
  }
  zipf = std::discrete_distribution<int>(weights.begin(), weights.end());

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (std::size_t i = 0; i < n; ++i) {
    int idx = zipf(rng);
    int key = keys[idx];
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// std::map adapter (tree-based)
auto collectInsertTimesStdMap(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::map<int, int> map;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert({key, key});
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

auto collectFindTimesStdMap(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::map<int, int> map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert({keys[i], keys[i]});
  }

  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

auto collectSequentialInsertTimesStdMap(std::size_t n)
    -> std::vector<std::uint64_t> {
  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::map<int, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert({static_cast<int>(i), static_cast<int>(i)});
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

auto collectZipfianFindTimesStdMap(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::map<int, int> map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(i);
    map.insert({keys[i], keys[i]});
  }

  std::discrete_distribution<int> zipf{};
  std::vector<double> weights(n);
  for (std::size_t i = 0; i < n; ++i) {
    weights[i] = 1.0 / (i + 1);
  }
  zipf = std::discrete_distribution<int>(weights.begin(), weights.end());

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (std::size_t i = 0; i < n; ++i) {
    int idx = zipf(rng);
    int key = keys[idx];
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Find miss for std::unordered_map
auto collectFindMissTimesStdUnordered(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::unordered_map<int, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    map.insert({static_cast<int>(i), static_cast<int>(i)});
  }

  std::vector<int> miss_keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    miss_keys[i] = static_cast<int>(n + i);
  }
  std::shuffle(miss_keys.begin(), miss_keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (int key : miss_keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Find miss for std::map
auto collectFindMissTimesStdMap(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::map<int, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    map.insert({static_cast<int>(i), static_cast<int>(i)});
  }

  std::vector<int> miss_keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    miss_keys[i] = static_cast<int>(n + i);
  }
  std::shuffle(miss_keys.begin(), miss_keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (int key : miss_keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Erase for std::unordered_map
auto collectEraseTimesStdUnordered(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::unordered_map<int, int> map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert({keys[i], keys[i]});
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.erase(key);
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Erase for std::map
auto collectEraseTimesStdMap(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::map<int, int> map;
  std::vector<int> keys(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys[i] = static_cast<int>(rng());
    map.insert({keys[i], keys[i]});
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  for (int key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.erase(key);
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Mixed workload for std::unordered_map
auto collectMixedTimesStdUnordered(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::unordered_map<int, int> map;
  std::vector<int> existing;
  for (std::size_t i = 0; i < n / 2; ++i) {
    int key = static_cast<int>(rng());
    map.insert({key, key});
    existing.push_back(key);
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);
  std::uniform_int_distribution<int> op_dist(0, 1);

  for (std::size_t i = 0; i < n; ++i) {
    bool do_insert = op_dist(rng) == 0 || existing.empty();

    auto start = std::chrono::high_resolution_clock::now();
    if (do_insert) {
      int key = static_cast<int>(rng());
      map.insert({key, key});
      existing.push_back(key);
    } else {
      std::size_t idx = rng() % existing.size();
      int key = existing[idx];
      map.erase(key);
      existing[idx] = existing.back();
      existing.pop_back();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// Mixed workload for std::map
auto collectMixedTimesStdMap(std::size_t n, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::map<int, int> map;
  std::vector<int> existing;
  for (std::size_t i = 0; i < n / 2; ++i) {
    int key = static_cast<int>(rng());
    map.insert({key, key});
    existing.push_back(key);
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);
  std::uniform_int_distribution<int> op_dist(0, 1);

  for (std::size_t i = 0; i < n; ++i) {
    bool do_insert = op_dist(rng) == 0 || existing.empty();

    auto start = std::chrono::high_resolution_clock::now();
    if (do_insert) {
      int key = static_cast<int>(rng());
      map.insert({key, key});
      existing.push_back(key);
    } else {
      std::size_t idx = rng() % existing.size();
      int key = existing[idx];
      map.erase(key);
      existing[idx] = existing.back();
      existing.pop_back();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// ============================================================================
// String Key Benchmarks
// ============================================================================
//
// String keys test hash function quality and compare/copy overhead.
// SSO strings (≤15 chars on most implementations) behave differently from heap strings.

inline auto generateRandomString(std::size_t len, std::mt19937& rng) -> std::string {
  static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::uniform_int_distribution<int> dist(0, sizeof(charset) - 2);
  std::string s;
  s.reserve(len);
  for (std::size_t i = 0; i < len; ++i) {
    s += charset[dist(rng)];
  }
  return s;
}

// Helper to detect std:: map types
template <typename T>
struct is_std_map : std::false_type {};
template <typename K, typename V, typename... Args>
struct is_std_map<std::unordered_map<K, V, Args...>> : std::true_type {};
template <typename K, typename V, typename... Args>
struct is_std_map<std::map<K, V, Args...>> : std::true_type {};

// String insert benchmark (works for all map types)
template <typename Map>
auto collectStringInsertTimes(std::size_t n, std::size_t str_len, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<std::string> keys;
  keys.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys.push_back(generateRandomString(str_len, rng));
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  Map map;
  for (const auto& key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    if constexpr (is_std_map<Map>::value) {
      map.insert({key, 1});  // std:: syntax
    } else {
      map.insert(key, 1);    // our map syntax
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// String find benchmark (works for all map types)
template <typename Map>
auto collectStringFindTimes(std::size_t n, std::size_t str_len, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<std::string> keys;
  keys.reserve(n);
  Map map;
  for (std::size_t i = 0; i < n; ++i) {
    auto key = generateRandomString(str_len, rng);
    keys.push_back(key);
    if constexpr (is_std_map<Map>::value) {
      map.insert({key, 1});
    } else {
      map.insert(key, 1);
    }
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (const auto& key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    if constexpr (is_std_map<Map>::value) {
      auto it = map.find(key);
      if (it != map.end()) sink = it->second;
    } else {
      auto* ptr = map.find(key);
      if (ptr) sink = *ptr;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// String insert for std::unordered_map
auto collectStringInsertTimesStdUnordered(std::size_t n, std::size_t str_len, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<std::string> keys;
  keys.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys.push_back(generateRandomString(str_len, rng));
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::unordered_map<std::string, int> map;
  for (const auto& key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert({key, 1});
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// String find for std::unordered_map
auto collectStringFindTimesStdUnordered(std::size_t n, std::size_t str_len, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<std::string> keys;
  keys.reserve(n);
  std::unordered_map<std::string, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    auto key = generateRandomString(str_len, rng);
    keys.push_back(key);
    map.insert({key, 1});
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (const auto& key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// String insert for std::map
auto collectStringInsertTimesStdMap(std::size_t n, std::size_t str_len, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<std::string> keys;
  keys.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys.push_back(generateRandomString(str_len, rng));
  }

  std::vector<std::uint64_t> times;
  times.reserve(n);

  std::map<std::string, int> map;
  for (const auto& key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    map.insert({key, 1});
    auto end = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// String find for std::map
auto collectStringFindTimesStdMap(std::size_t n, std::size_t str_len, std::mt19937& rng)
    -> std::vector<std::uint64_t> {
  std::vector<std::string> keys;
  keys.reserve(n);
  std::map<std::string, int> map;
  for (std::size_t i = 0; i < n; ++i) {
    auto key = generateRandomString(str_len, rng);
    keys.push_back(key);
    map.insert({key, 1});
  }
  std::shuffle(keys.begin(), keys.end(), rng);

  std::vector<std::uint64_t> times;
  times.reserve(n);

  volatile int sink = 0;
  for (const auto& key : keys) {
    auto start = std::chrono::high_resolution_clock::now();
    auto it = map.find(key);
    auto end = std::chrono::high_resolution_clock::now();
    if (it != map.end()) sink = it->second;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    times.push_back(static_cast<std::uint64_t>(ns));
  }
  return times;
}

// ============================================================================
// Progressive Benchmark Runner
// ============================================================================

// Fast maps: Swiss variants, F14, std::unordered_map (SIMD-accelerated only)
// Linear probing variants (LinR, Grave, Robin) excluded - can't keep up at 5M scale
inline constexpr std::array<bool, kNumMaps> kFastMaps = {{
    false,  // 0: Linear Probing - slow
    false,  // 1: Linear Reuse - linear probing, slow at scale
    false,  // 2: Graveyard - linear probing, slow at scale
    false,  // 3: Quadratic Probing - slow
    false,  // 4: Double Hashing - slow
    false,  // 5: Robin Hood - linear probing, slow at scale
    false,  // 6: Anti-Robin Hood - slow
    false,  // 7: Separate Chaining - slow
    false,  // 8: Cuckoo - slow
    false,  // 9: Hopscotch - slow
    false,  // 10: Two-Choice - slow
    true,   // 11: Swiss Table 16 - fast
    true,   // 12: Swiss Prefetch - fast
    true,   // 13: Swiss Reuse - fast
    true,   // 14: Swiss Graveyard - fast
    true,   // 15: Swiss Bloom - fast
    true,   // 16: Swiss Table 32 - fast
    true,   // 17: Swiss32 Prefetch - fast
    true,   // 18: Swiss32 Graveyard - fast
    true,   // 19: Swiss32 Pf+Gr - fast
    true,   // 20: Swiss Table 64 - fast
    true,   // 21: F14 - fast
    true,   // 22: std::unordered_map - baseline
    false,  // 23: std::map - slow (tree-based)
}};

template <typename CollectorFn>
auto runProgressiveBenchmark(const std::string& name, CollectorFn&& fn,
                            DisplayState& state, std::size_t test_idx,
                            bool fast_only = false) -> BenchResult {
  BenchResult result;
  result.name = name;
  result.completed_count = 0;

  // Add this result to the display state
  if (test_idx >= state.results.size()) {
    state.results.push_back(result);
  }

  for (std::size_t map_idx = 0; map_idx < kNumMaps; ++map_idx) {
    // Skip slow maps if fast_only mode
    if (fast_only && !kFastMaps[map_idx]) {
      continue;
    }

    state.current_map_idx = static_cast<int>(map_idx);
    state.current_map_name = kMaps[map_idx].name;
    state.render();

    std::mt19937 rng{42};
    std::vector<std::uint64_t> times;

    switch (map_idx) {
      // Basic probing
      case 0: times = fn.template operator()<LinearProbingMap<int, int>>(rng); break;
      case 1: times = fn.template operator()<LinearProbingReuseMap<int, int>>(rng); break;
      case 2: times = fn.template operator()<GraveyardMap<int, int>>(rng); break;
      case 3: times = fn.template operator()<QuadraticProbingMap<int, int>>(rng); break;
      case 4: times = fn.template operator()<DoubleHashingMap<int, int>>(rng); break;
      case 5: times = fn.template operator()<RobinHoodMap<int, int>>(rng); break;
      case 6: times = fn.template operator()<AntiRobinHoodMap<int, int>>(rng); break;
      case 7: times = fn.template operator()<SeparateChainingMap<int, int>>(rng); break;
      // Advanced probing
      case 8: times = fn.template operator()<CuckooMap<int, int>>(rng); break;
      case 9: times = fn.template operator()<HopscotchMap<int, int>>(rng); break;
      case 10: times = fn.template operator()<TwoChoiceMap<int, int>>(rng); break;
      // Coalesced/Extendible excluded - O(n) operations make them too slow
      // Swiss Table 16-byte variants
      case 11: times = fn.template operator()<SwissTable<int, int>>(rng); break;
      case 12: times = fn.template operator()<SwissTablePrefetch<int, int>>(rng); break;
      case 13: times = fn.template operator()<SwissTableReuse<int, int>>(rng); break;
      case 14: times = fn.template operator()<SwissTableGraveyard<int, int>>(rng); break;
      case 15: times = fn.template operator()<SwissTableBloom<int, int>>(rng); break;
      // Swiss Table 32-byte variants
      case 16: times = fn.template operator()<SwissTable32<int, int>>(rng); break;
      case 17: times = fn.template operator()<SwissTable32Prefetch<int, int>>(rng); break;
      case 18: times = fn.template operator()<SwissTable32Graveyard<int, int>>(rng); break;
      case 19: times = fn.template operator()<SwissTable32PrefetchGraveyard<int, int>>(rng); break;
      // Swiss Table 64-byte and F14
      case 20: times = fn.template operator()<SwissTable64<int, int>>(rng); break;
      case 21: times = fn.template operator()<F14Map<int, int>>(rng); break;
      // Baselines
      case 22: times = fn.template operator()<std::unordered_map<int, int>>(rng); break;
      case 23: times = fn.template operator()<std::map<int, int>>(rng); break;
    }

    result.stats[map_idx] = computeOpStats(times);
    result.completed_count++;
    state.updateMapProgress(test_idx, map_idx, result.stats[map_idx]);
  }

  state.results[test_idx] = result;
  return result;
}

// String-key benchmark runner - instantiates maps with <std::string, int>
template <typename CollectorFn>
auto runStringBenchmark(const std::string& name, CollectorFn&& fn,
                        DisplayState& state, std::size_t test_idx) -> BenchResult {
  BenchResult result;
  result.name = name;
  result.completed_count = 0;

  if (test_idx >= state.results.size()) {
    state.results.push_back(result);
  }

  for (std::size_t map_idx = 0; map_idx < kNumMaps; ++map_idx) {
    state.current_map_idx = static_cast<int>(map_idx);
    state.current_map_name = kMaps[map_idx].name;
    state.render();

    std::mt19937 rng{42};
    std::vector<std::uint64_t> times;

    // Instantiate maps with <std::string, int> for string key tests
    switch (map_idx) {
      // Basic probing
      case 0: times = fn.template operator()<LinearProbingMap<std::string, int>>(rng); break;
      case 1: times = fn.template operator()<LinearProbingReuseMap<std::string, int>>(rng); break;
      case 2: times = fn.template operator()<GraveyardMap<std::string, int>>(rng); break;
      case 3: times = fn.template operator()<QuadraticProbingMap<std::string, int>>(rng); break;
      case 4: times = fn.template operator()<DoubleHashingMap<std::string, int>>(rng); break;
      case 5: times = fn.template operator()<RobinHoodMap<std::string, int>>(rng); break;
      case 6: times = fn.template operator()<AntiRobinHoodMap<std::string, int>>(rng); break;
      case 7: times = fn.template operator()<SeparateChainingMap<std::string, int>>(rng); break;
      // Advanced probing
      case 8: times = fn.template operator()<CuckooMap<std::string, int>>(rng); break;
      case 9: times = fn.template operator()<HopscotchMap<std::string, int>>(rng); break;
      case 10: times = fn.template operator()<TwoChoiceMap<std::string, int>>(rng); break;
      // Coalesced/Extendible excluded - O(n) operations make them too slow
      // Swiss Table 16-byte variants
      case 11: times = fn.template operator()<SwissTable<std::string, int>>(rng); break;
      case 12: times = fn.template operator()<SwissTablePrefetch<std::string, int>>(rng); break;
      case 13: times = fn.template operator()<SwissTableReuse<std::string, int>>(rng); break;
      case 14: times = fn.template operator()<SwissTableGraveyard<std::string, int>>(rng); break;
      case 15: times = fn.template operator()<SwissTableBloom<std::string, int>>(rng); break;
      // Swiss Table 32-byte variants
      case 16: times = fn.template operator()<SwissTable32<std::string, int>>(rng); break;
      case 17: times = fn.template operator()<SwissTable32Prefetch<std::string, int>>(rng); break;
      case 18: times = fn.template operator()<SwissTable32Graveyard<std::string, int>>(rng); break;
      case 19: times = fn.template operator()<SwissTable32PrefetchGraveyard<std::string, int>>(rng); break;
      // Swiss Table 64-byte and F14
      case 20: times = fn.template operator()<SwissTable64<std::string, int>>(rng); break;
      case 21: times = fn.template operator()<F14Map<std::string, int>>(rng); break;
      // Baselines
      case 22: times = fn.template operator()<std::unordered_map<std::string, int>>(rng); break;
      case 23: times = fn.template operator()<std::map<std::string, int>>(rng); break;
    }

    result.stats[map_idx] = computeOpStats(times);
    result.completed_count++;
    state.updateMapProgress(test_idx, map_idx, result.stats[map_idx]);
  }

  state.results[test_idx] = result;
  return result;
}

// Specialized versions for different collector function signatures

template <typename Fn>
struct RandomCollectorWrapper {
  std::size_t n;
  Fn fn;

  template <typename Map>
  auto operator()(std::mt19937& rng) -> std::vector<std::uint64_t> {
    if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
      return fn(n, rng);
    } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
      return fn(n, rng);
    } else {
      return collectInsertTimes<Map>(n, rng);
    }
  }
};

template <typename Fn>
struct FindCollectorWrapper {
  std::size_t n;
  Fn fn_custom;
  Fn fn_std_umap;
  Fn fn_std_map;

  template <typename Map>
  auto operator()(std::mt19937& rng) -> std::vector<std::uint64_t> {
    if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
      return fn_std_umap(n, rng);
    } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
      return fn_std_map(n, rng);
    } else {
      return fn_custom(n, rng);
    }
  }
};

template <typename Fn>
struct SequentialCollectorWrapper {
  std::size_t n;
  Fn fn_std_umap;
  Fn fn_std_map;

  template <typename Map>
  auto operator()(std::mt19937& rng) -> std::vector<std::uint64_t> {
    (void)rng;  // Unused for sequential
    if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
      return fn_std_umap(n);
    } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
      return fn_std_map(n);
    } else {
      return collectSequentialInsertTimes<Map>(n);
    }
  }
};

// ============================================================================
// Final Summary and Rankings
// ============================================================================

void printFinalSummary(const std::vector<BenchResult>& results) {
  ansi::clearScreen();
  ansi::moveCursor(1, 1);

  std::println("");
  printBoxTop();
  printBoxCenter("HASH MAP BENCHMARK SUITE");
  printBoxSep();
  printBoxLine("");
  printBoxLine("  Compares 23 hash map implementations across multiple workloads.");
  printBoxLine("  All times are per-operation latency in nanoseconds (median).");
  printBoxLine("");
  printBoxLine("  Sizes:  1K (~16KB)  |  50K (~800KB)  |  500K (~8MB)  |  5M (~80MB)");
  printBoxLine("  Note: 500K and 5M tests run only fast maps (Swiss, F14, etc)");
  printBoxLine("");
  printBoxLine("  Core ops:  Insert, Find (hit), Find (miss) at all 4 sizes");
  printBoxLine("  Specialized:  Erase, Mixed (tombstones), Zipfian, String keys");
  printBoxLine("");
  printBoxSep();

  // Print comparison table (key representatives due to space constraints)
  // Indices: 0=Lin 2=Grave 5=Robin 11=Sw16 15=Sw32 16=S32P 19=Sw64 20=F14 21=UMap 22=Tree
  printBoxLine(" Test      Lin  Grave Robin Sw16  Sw32  S32P  Sw64  F14  UMap  Tree");
  printBoxSep();

  for (const auto& r : results) {
    auto fmt = [](const OpStats& s) {
      if (!s.completed) return std::string("  --");
      return std::format("{:>5.0f}", s.p50);
    };

    auto line = std::format(
        " {:<8} {} {} {} {} {} {} {} {} {} {}",
        r.name.substr(0, 8),
        fmt(r.stats[0]), fmt(r.stats[2]), fmt(r.stats[5]),
        fmt(r.stats[11]), fmt(r.stats[15]), fmt(r.stats[16]),
        fmt(r.stats[19]), fmt(r.stats[20]), fmt(r.stats[21]), fmt(r.stats[22]));
    printBoxLine(line);
  }

  printBoxBottom();

  // Box-whisker plots with descriptions
  std::println("");
  printBoxTop();
  printBoxCenter("OPERATION TIME DISTRIBUTION (ns)");
  printBoxLine("  ● = median    ████ = IQR (Q1-Q3)    ├──┤ = 1.5×IQR whiskers (outliers excluded)");
  printBoxBottom();

  // Test descriptions: what, dataset size, map setup, why it matters
  auto getTestDescription = [](const std::string& name) -> std::vector<std::string> {
    // INSERT tests
    if (name == "Ins-1K") {
      return {
        "INSERT (Small): 1,000 random integer keys into empty map.",
        "Setup: Empty map, ~16KB total. Fits in L1/L2 cache.",
        "Best-case insertion - minimal cache misses, few rehashes."
      };
    } else if (name == "Ins-50K") {
      return {
        "INSERT (Medium): 50,000 random integer keys into empty map.",
        "Setup: Empty map, ~800KB total. Fits in L3 cache.",
        "Typical workload size. Multiple rehashes during growth."
      };
    } else if (name == "Ins-500K") {
      return {
        "INSERT (Large): 500,000 random integer keys into empty map. FAST MAPS ONLY.",
        "Setup: Empty map, ~8MB total. Exceeds L2, partially fills L3.",
        "Shows cache pressure effects. Multiple rehashes during growth."
      };
    } else if (name == "Ins-5M") {
      return {
        "INSERT (Huge): 5,000,000 random integer keys into empty map. FAST MAPS ONLY.",
        "Setup: Empty map, ~80MB total. Exceeds L3 cache entirely.",
        "Memory-bound workload. Shows true DRAM access patterns."
      };
    }
    // FIND (HIT) tests
    else if (name == "Find-1K") {
      return {
        "FIND HIT (Small): 1,000 lookups for existing keys.",
        "Setup: Map pre-filled with 1k keys, 0 tombstones. Fits in L1/L2.",
        "Best-case lookup - hot cache, short probe sequences."
      };
    } else if (name == "Find-50K") {
      return {
        "FIND HIT (Medium): 50,000 lookups for existing keys.",
        "Setup: Map pre-filled with 50k keys, 0 tombstones. Fits in L3.",
        "Typical lookup workload. Some cache misses."
      };
    } else if (name == "Find-500K") {
      return {
        "FIND HIT (Large): 500,000 lookups for existing keys. FAST MAPS ONLY.",
        "Setup: Map pre-filled with 500k keys, 0 tombstones. Exceeds L2 cache.",
        "Cache pressure visible. Working set doesn't fit in L2."
      };
    } else if (name == "Find-5M") {
      return {
        "FIND HIT (Huge): 5,000,000 lookups for existing keys. FAST MAPS ONLY.",
        "Setup: Map pre-filled with 5M keys, 0 tombstones. Exceeds L3 cache.",
        "Memory-bound. Every lookup likely hits DRAM."
      };
    }
    // FIND (MISS) tests
    else if (name == "Miss-1K") {
      return {
        "FIND MISS (Small): 1,000 lookups for non-existent keys.",
        "Setup: Map has keys [0,1k), searching [1k,2k). 0 tombstones.",
        "Must probe to empty slot. Short probes when cache-hot."
      };
    } else if (name == "Miss-50K") {
      return {
        "FIND MISS (Medium): 50,000 lookups for non-existent keys.",
        "Setup: Map has keys [0,50k), searching [50k,100k). 0 tombstones.",
        "Miss lookups are often slower than hits (must find empty)."
      };
    } else if (name == "Miss-500K") {
      return {
        "FIND MISS (Large): 500,000 lookups for non-existent keys. FAST MAPS ONLY.",
        "Setup: Map has keys [0,500k), searching [500k,1M). 0 tombstones.",
        "Miss + cache pressure: must probe to empty slot without L2 help."
      };
    } else if (name == "Miss-5M") {
      return {
        "FIND MISS (Huge): 5,000,000 lookups for non-existent keys. FAST MAPS ONLY.",
        "Setup: Map has keys [0,5M), searching [5M,10M). 0 tombstones.",
        "Memory-bound miss lookups. Must probe to empty slot from DRAM."
      };
    }
    // Specialized tests
    else if (name == "Erase-50K") {
      return {
        "ERASE: 50,000 deletions from a full map, random order.",
        "Setup: Map pre-filled with 50k random keys, no prior deletions.",
        "Tests tombstone creation. Robin Hood requires expensive backward shifts."
      };
    } else if (name == "Mixed-50K") {
      return {
        "MIXED: 50,000 ops (50% insert, 50% erase), randomized.",
        "Setup: Map starts half-full (25k keys). Tombstones accumulate.",
        "Steady-state workload. Tests tombstone reuse and probe degradation."
      };
    } else if (name == "Zipf-1K") {
      return {
        "ZIPFIAN FIND: 1,000 lookups following 80/20 distribution.",
        "Setup: Map pre-filled with 1k sequential keys, no tombstones.",
        "Hot keys stay in cache. Tests real-world access patterns."
      };
    }
    // String tests
    else if (name == "Str8-50K") {
      return {
        "STRING INSERT (SSO): 50,000 random 8-char string keys.",
        "Setup: Empty map. 8-char strings fit in Small String Optimization.",
        "Tests std::hash<string>. SSO avoids heap allocation per key."
      };
    } else if (name == "Str64-50K") {
      return {
        "STRING INSERT (HEAP): 50,000 random 64-char string keys.",
        "Setup: Empty map. 64-char strings require heap allocation.",
        "Stresses hash computation, key comparison, and memory allocation."
      };
    }
    return {name};
  };

  for (const auto& r : results) {
    std::println("");
    auto desc = getTestDescription(r.name);
    for (const auto& line : desc) {
      std::println("  {}", line);
    }
    std::println("");
    printProgressiveBoxWhisker(r.name, r.stats, kNumMaps);
  }

  // Rankings and speedup vs std::unordered_map
  std::println("");
  printBoxTop();
  printBoxCenter("PERFORMANCE RANKINGS (median latency)");
  printBoxSep();

  for (const auto& r : results) {
    std::vector<std::pair<double, std::size_t>> rankings;
    for (std::size_t i = 0; i < kNumMaps; ++i) {
      if (r.stats[i].completed) {
        rankings.push_back({r.stats[i].p50, i});
      }
    }
    std::sort(rankings.begin(), rankings.end());

    std::string line = std::format(" {:<10} ", r.name);
    for (std::size_t i = 0; i < std::min(std::size_t{5}, rankings.size()); ++i) {
      auto [latency, idx] = rankings[i];
      line += std::format("{}. {} ({:.0f}ns)  ",
                         i + 1, kMaps[idx].short_name, latency);
    }
    printBoxLine(line);

    // Speedup vs std::unordered_map baseline (index 21)
    if (r.stats[21].completed && rankings.size() > 0) {
      double baseline = r.stats[21].p50;
      double best = rankings[0].first;
      double speedup = baseline / best;
      auto speedup_line = std::format("           Best is {:.2f}x faster than std::unordered_map",
                                     speedup);
      printBoxLine(speedup_line);
    }
  }

  printBoxBottom();

  // Summary insights
  std::println("");
  printBoxTop();
  printBoxCenter("SUMMARY INSIGHTS");
  printBoxSep();
  printBoxLine("");
  printBoxLine("  Swiss Tables (Sw16/Sw32/Sw64): SIMD-accelerated, production-quality");
  printBoxLine("  S32P (Swiss32+Prefetch): Best-in-class for cache-bound workloads");
  printBoxLine("  Graveyard variants: Strategic tombstones reduce probe lengths");
  printBoxLine("  Robin Hood: Variance reduction shines at high load factors");
  printBoxLine("  Cuckoo/Hopscotch: O(1) worst-case lookup, complex insertion");
  printBoxLine("  Linear/Quad: Simple, cache-friendly, suffer from clustering");
  printBoxLine("  std::unordered_map: Baseline - typically separate chaining");
  printBoxLine("  std::map: Tree-based O(log n), useful for ordered iteration");
  printBoxLine("");
  printBoxBottom();

  // Legend
  std::println("");
  printLegend();
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  ansi::hideCursor();

  DisplayState state;

  // ==========================================================================
  // Test sizes - chosen to show cache effects
  // ==========================================================================
  // AMD Ryzen AI 9 HX 370 (Zen 5) cache hierarchy:
  //   L1 Data: 48 KB/core, L2: 1 MB/core, L3: 24 MB shared
  //   Source: https://www.notebookcheck.net/AMD-Ryzen-AI-9-HX-370-Processor-Benchmarks-and-Specs.836729.0.html
  //
  // Size estimates (key+value+overhead ≈ 16 bytes/entry for open addressing):
  //   Small:  1,000 entries  ≈ 16 KB   → fits in L1 (48 KB)
  //   Medium: 50,000 entries ≈ 800 KB  → fits in L2 (1 MB)
  //   Large:  500,000 entries ≈ 8 MB   → exceeds L2, partially fills L3
  //
  // NOTE: For true memory-bound testing (exceeds 24MB L3), increase kLarge
  // to 2,000,000+ entries. This will take significantly longer (~10+ minutes).
  // ==========================================================================
  constexpr std::size_t kSmall = 1'000;
  constexpr std::size_t kMedium = 50'000;
  constexpr std::size_t kLarge = 500'000;   // Exceeds L2, shows cache pressure
  constexpr std::size_t kHuge = 5'000'000;  // ~80MB, exceeds L3 - fast maps only

  std::size_t test_idx = 0;

  // Helper lambda to create benchmark runners
  auto runInsert = [&](const char* name, std::size_t n, bool fast_only = false) {
    runProgressiveBenchmark(
        name,
        [n]<typename Map>(std::mt19937& rng) {
          if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
            return collectInsertTimesStdUnordered(n, rng);
          } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
            return collectInsertTimesStdMap(n, rng);
          } else {
            return collectInsertTimes<Map>(n, rng);
          }
        },
        state, test_idx++, fast_only);
  };

  auto runFind = [&](const char* name, std::size_t n, bool fast_only = false) {
    runProgressiveBenchmark(
        name,
        [n]<typename Map>(std::mt19937& rng) {
          if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
            return collectFindTimesStdUnordered(n, rng);
          } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
            return collectFindTimesStdMap(n, rng);
          } else {
            return collectFindTimes<Map>(n, rng);
          }
        },
        state, test_idx++, fast_only);
  };

  auto runMiss = [&](const char* name, std::size_t n, bool fast_only = false) {
    runProgressiveBenchmark(
        name,
        [n]<typename Map>(std::mt19937& rng) {
          if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
            return collectFindMissTimesStdUnordered(n, rng);
          } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
            return collectFindMissTimesStdMap(n, rng);
          } else {
            return collectFindMissTimes<Map>(n, rng);
          }
        },
        state, test_idx++, fast_only);
  };

  // ==========================================================================
  // Core operations - all maps at small/medium, fast maps only at large/huge
  // ==========================================================================

  // INSERT: Random keys into empty map
  runInsert("Ins-1K", kSmall);
  runInsert("Ins-50K", kMedium);
  runInsert("Ins-500K", kLarge, true);   // Fast maps only
  runInsert("Ins-5M", kHuge, true);      // Fast maps only

  // FIND (HIT): Lookup existing keys
  runFind("Find-1K", kSmall);
  runFind("Find-50K", kMedium);
  runFind("Find-500K", kLarge, true);    // Fast maps only
  runFind("Find-5M", kHuge, true);       // Fast maps only

  // FIND (MISS): Lookup non-existent keys
  runMiss("Miss-1K", kSmall);
  runMiss("Miss-50K", kMedium);
  runMiss("Miss-500K", kLarge, true);    // Fast maps only
  runMiss("Miss-5M", kHuge, true);       // Fast maps only

  // ==========================================================================
  // Specialized workloads (at appropriate sizes)
  // ==========================================================================

  // Erase benchmark (medium - shows tombstone creation)
  runProgressiveBenchmark(
      "Erase-50K",
      [n = kMedium]<typename Map>(std::mt19937& rng) {
        if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
          return collectEraseTimesStdUnordered(n, rng);
        } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
          return collectEraseTimesStdMap(n, rng);
        } else {
          return collectEraseTimes<Map>(n, rng);
        }
      },
      state, test_idx++);

  // Mixed workload (medium - steady-state with tombstones)
  runProgressiveBenchmark(
      "Mixed-50K",
      [n = kMedium]<typename Map>(std::mt19937& rng) {
        if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
          return collectMixedTimesStdUnordered(n, rng);
        } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
          return collectMixedTimesStdMap(n, rng);
        } else {
          return collectMixedTimes<Map>(n, rng);
        }
      },
      state, test_idx++);

  // Zipfian find (small - hot-key patterns benefit from cache)
  runProgressiveBenchmark(
      "Zipf-1K",
      [n = kSmall]<typename Map>(std::mt19937& rng) {
        if constexpr (std::is_same_v<Map, std::unordered_map<int, int>>) {
          return collectZipfianFindTimesStdUnordered(n, rng);
        } else if constexpr (std::is_same_v<Map, std::map<int, int>>) {
          return collectZipfianFindTimesStdMap(n, rng);
        } else {
          return collectZipfianFindTimes<Map>(n, rng);
        }
      },
      state, test_idx++);

  // ==========================================================================
  // String key tests (medium size)
  // ==========================================================================

  // String keys - SSO size (8 chars fits in Small String Optimization)
  runStringBenchmark(
      "Str8-50K",
      [n = kMedium]<typename Map>(std::mt19937& rng) {
        return collectStringInsertTimes<Map>(n, 8, rng);
      },
      state, test_idx++);

  // String keys - heap allocated (64 chars exceeds SSO)
  runStringBenchmark(
      "Str64-50K",
      [n = kMedium]<typename Map>(std::mt19937& rng) {
        return collectStringInsertTimes<Map>(n, 64, rng);
      },
      state, test_idx++);

  // Final summary display
  printFinalSummary(state.results);

  ansi::showCursor();
  return 0;
}
