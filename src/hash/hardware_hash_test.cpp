#include "hash/aes_hash.h"
#include "hash/crc32c.h"
#include "hash/fibonacci.h"
#include "hash/rolling_hash.h"

#include <array>
#include <bit>
#include <print>
#include <string_view>
#include <vector>

#include "unit.h"

using namespace tempura;
using namespace std::string_view_literals;

auto main() -> int {
  // ==========================================================================
  // CRC32C Tests
  // ==========================================================================

  "CRC32C: basic functionality"_test = [] {
    const std::uint8_t data[] = {'h', 'e', 'l', 'l', 'o'};
    auto h1 = crc32c(data, 5);
    auto h2 = crc32c(data, 5);

    // Deterministic
    expectEq(h1, h2);
    std::println("CRC32C('hello') = 0x{:08x}", h1);
  };

  "CRC32C: different seeds"_test = [] {
    const std::uint8_t data[] = {'t', 'e', 's', 't'};

    auto h0 = crc32c(data, 4, 0);
    auto h1 = crc32c(data, 4, 1);
    auto h42 = crc32c(data, 4, 42);

    // Different seeds should give different results
    expectNeq(h0, h1);
    expectNeq(h1, h42);

    std::println("CRC32C seeds: 0→0x{:08x}, 1→0x{:08x}, 42→0x{:08x}", h0, h1,
                 h42);
  };

  "CRC32C: integer hasher"_test = [] {
    Crc32cHash<std::uint64_t> hasher;

    auto h0 = hasher(0);
    auto h1 = hasher(1);
    auto hmax = hasher(~0ULL);

    expectNeq(h0, h1);
    expectNeq(h1, hmax);

    std::println("CRC32C integers: 0→0x{:08x}, 1→0x{:08x}", h0, h1);
  };

  "CRC32C: mixed version has better avalanche"_test = [] {
    // Compare raw CRC32C vs mixed version
    const std::uint8_t d1[] = {0};
    const std::uint8_t d2[] = {1};

    auto raw1 = crc32c(d1, 1);
    auto raw2 = crc32c(d2, 1);
    int raw_diff = std::popcount(raw1 ^ raw2);

    auto mix1 = crc32cMixed(d1, 1);
    auto mix2 = crc32cMixed(d2, 1);
    int mix_diff = std::popcount(mix1 ^ mix2);

    std::println("Single bit change: raw CRC32C flips {} bits, mixed flips {} bits",
                 raw_diff, mix_diff);

    // Mixed should generally have better avalanche
    expectGT(mix_diff, 8);  // Expect decent mixing
  };

  // ==========================================================================
  // AES Hash Tests
  // ==========================================================================

  "AES Hash: basic functionality"_test = [] {
    const std::uint8_t data[] = {'h', 'e', 'l', 'l', 'o'};
    auto h1 = aesHash64(data, 5);
    auto h2 = aesHash64(data, 5);

    // Deterministic
    expectEq(h1, h2);
    std::println("AES Hash('hello') = 0x{:016x}", h1);
  };

  "AES Hash: different inputs"_test = [] {
    AesHash<std::string_view> hasher;

    auto h1 = hasher("hello"sv);
    auto h2 = hasher("world"sv);
    auto h3 = hasher("hello world"sv);

    expectNeq(h1, h2);
    expectNeq(h2, h3);
    expectNeq(h1, h3);

    std::println("AES Hash strings: 'hello'→0x{:016x}, 'world'→0x{:016x}", h1, h2);
  };

  "AES Hash: avalanche effect"_test = [] {
    AesHash<std::uint64_t> hasher;

    int total_flips = 0;
    int tests = 0;

    for (std::uint64_t base = 1; base != 0; base <<= 1) {
      auto h1 = hasher(0);
      auto h2 = hasher(base);
      int flipped = std::popcount(h1 ^ h2);
      total_flips += flipped;
      ++tests;
    }

    double avg_flips = static_cast<double>(total_flips) / tests;
    std::println("AES Hash avalanche: avg {:.1f}/64 bits ({:.1f}%)",
                 avg_flips, avg_flips * 100 / 64);

    // AES should have excellent avalanche
    expectGT(avg_flips, 28.0);  // Expect near 50%
  };

  "AES Hash: empty and short inputs"_test = [] {
    auto h_empty = aesHash64(nullptr, 0);
    const std::uint8_t one[] = {'a'};
    auto h_one = aesHash64(one, 1);

    expectNeq(h_empty, h_one);
    expectNeq(h_empty, 0ULL);

    std::println("AES Hash: empty→0x{:016x}, 'a'→0x{:016x}", h_empty, h_one);
  };

  // ==========================================================================
  // Rolling Hash Tests
  // ==========================================================================

  "Rolling Hash: basic polynomial"_test = [] {
    auto h1 = rollingHash("abc"sv);
    auto h2 = rollingHash("abc"sv);

    expectEq(h1, h2);
    std::println("Rolling hash('abc') = {}", h1);
  };

  "Rolling Hash: window sliding"_test = [] {
    std::string_view text = "abcdef";
    constexpr std::size_t window_size = 3;

    // Compute hash of "abc" using the class
    RollingHash rh(window_size);
    for (std::size_t i = 0; i < window_size; ++i) {
      rh.append(static_cast<std::uint8_t>(text[i]));
    }
    auto h_abc = rh.hash();

    // Slide to "bcd"
    rh.slide(static_cast<std::uint8_t>(text[0]),
             static_cast<std::uint8_t>(text[3]));
    auto h_bcd = rh.hash();

    // Compare with direct computation
    auto h_abc_direct = rollingHash("abc"sv);
    auto h_bcd_direct = rollingHash("bcd"sv);

    expectEq(h_abc, h_abc_direct);
    expectEq(h_bcd, h_bcd_direct);

    std::println("Rolling hash: 'abc'={}, 'bcd'={}", h_abc, h_bcd);
  };

  "Rabin-Karp search: find pattern"_test = [] {
    std::string_view text = "the quick brown fox jumps over the lazy dog";
    std::string_view pattern = "the";

    std::vector<std::size_t> matches;
    rabinKarpSearch(text, pattern, std::back_inserter(matches));

    // Should find "the" at positions 0 and 31
    expectEq(matches.size(), 2UL);
    if (matches.size() >= 2) {
      expectEq(matches[0], 0UL);
      expectEq(matches[1], 31UL);
    }

    std::println("Rabin-Karp found '{}' at {} positions", pattern,
                 matches.size());
  };

  "Rabin-Karp search: no matches"_test = [] {
    std::string_view text = "hello world";
    std::string_view pattern = "xyz";

    std::vector<std::size_t> matches;
    rabinKarpSearch(text, pattern, std::back_inserter(matches));

    expectEq(matches.size(), 0UL);
  };

  "Rabin-Karp search: overlapping matches"_test = [] {
    std::string_view text = "aaaa";
    std::string_view pattern = "aa";

    std::vector<std::size_t> matches;
    rabinKarpSearch(text, pattern, std::back_inserter(matches));

    // Should find "aa" at positions 0, 1, 2
    expectEq(matches.size(), 3UL);
  };

  // ==========================================================================
  // Fibonacci Hashing Tests
  // ==========================================================================

  "Fibonacci index: basic"_test = [] {
    // For a 16-bucket table (2^4)
    auto idx0 = fibonacciIndex(0, 16);
    auto idx1 = fibonacciIndex(1, 16);
    auto idx2 = fibonacciIndex(2, 16);

    // Should be well-distributed, not sequential
    expectNeq(idx1, idx0 + 1);  // Not just incrementing

    std::println("Fibonacci indices (16 buckets): 0→{}, 1→{}, 2→{}", idx0, idx1,
                 idx2);
  };

  "Fibonacci index: distribution"_test = [] {
    constexpr std::size_t table_size = 64;
    constexpr std::size_t num_keys = 1000;

    std::array<int, table_size> buckets{};

    FibonacciReducer reducer(table_size);
    for (std::size_t i = 0; i < num_keys; ++i) {
      std::size_t idx = reducer(i);
      expectLT(idx, table_size);
      buckets[idx]++;
    }

    // Check variance
    double mean = static_cast<double>(num_keys) / table_size;
    double variance = 0;
    for (int count : buckets) {
      double diff = count - mean;
      variance += diff * diff;
    }
    variance /= table_size;

    std::println("Fibonacci distribution: variance = {:.1f} (ideal ~{})",
                 variance, mean);

    // Should have reasonable distribution
    expectLT(variance, mean * 3);  // Variance should be manageable
  };

  "Fibonacci hash: integer mixing"_test = [] {
    // Test that sequential integers get well-mixed
    auto h0 = fibonacciHash64(0);
    auto h1 = fibonacciHash64(1);
    auto h2 = fibonacciHash64(2);

    // All should be different
    expectNeq(h0, h1);
    expectNeq(h1, h2);
    expectNeq(h0, h2);

    // Should have good bit differences
    int diff01 = std::popcount(h0 ^ h1);
    int diff12 = std::popcount(h1 ^ h2);

    std::println("Fibonacci hash: 0→0x{:016x}, 1→0x{:016x} (diff={} bits)",
                 h0, h1, diff01);

    expectGT(diff01, 16);  // Expect good mixing
    expectGT(diff12, 16);
  };

  "Fast range reduction: non-power-of-2"_test = [] {
    // Test with table size that isn't power of 2
    constexpr std::uint64_t range = 100;
    constexpr int num_tests = 10000;

    std::array<int, 100> buckets{};

    FastRangeReducer reducer(range);
    for (int i = 0; i < num_tests; ++i) {
      // Use a simple hash of i
      std::uint64_t hash = static_cast<std::uint64_t>(i) * 0x9E3779B97F4A7C15ULL;
      auto idx = reducer(hash);
      expectLT(idx, range);
      buckets[idx]++;
    }

    // Check that all buckets are used
    int empty_buckets = 0;
    for (int count : buckets) {
      if (count == 0) ++empty_buckets;
    }

    std::println("Fast range (100): empty buckets = {}/100", empty_buckets);
    expectLT(empty_buckets, 10);  // Most buckets should be used
  };

  "Fast range reduction: edge cases"_test = [] {
    FastRangeReducer reducer(1);
    // Any hash should map to 0 when range is 1
    expectEq(reducer(0), 0UL);
    expectEq(reducer(12345), 0UL);
    expectEq(reducer(~0ULL), 0UL);
  };

  // ==========================================================================
  // Comparison Tests
  // ==========================================================================

  "Comparison: distribution of all hash functions"_test = [] {
    constexpr int num_keys = 10000;
    constexpr int num_buckets = 100;

    std::array<int, num_buckets> crc_buckets{};
    std::array<int, num_buckets> aes_buckets{};

    Crc32cHash<std::uint64_t> crc_hash;
    AesHash<std::uint64_t> aes_hash;

    for (int i = 0; i < num_keys; ++i) {
      crc_buckets[crc_hash(i) % num_buckets]++;
      aes_buckets[aes_hash(i) % num_buckets]++;
    }

    auto calcVariance = [](const std::array<int, num_buckets>& buckets) {
      double mean = num_keys / static_cast<double>(num_buckets);
      double variance = 0;
      for (int count : buckets) {
        double diff = count - mean;
        variance += diff * diff;
      }
      return variance / num_buckets;
    };

    double crc_var = calcVariance(crc_buckets);
    double aes_var = calcVariance(aes_buckets);

    std::println("Distribution variance (lower is better):");
    std::println("  CRC32C: {:.1f}", crc_var);
    std::println("  AES Hash: {:.1f}", aes_var);

    expectLT(crc_var, 500.0);
    expectLT(aes_var, 500.0);
  };

  return TestRegistry::result();
}
