#include "hash/fnv.h"
#include "hash/hash.h"
#include "hash/murmur3.h"
#include "hash/wyhash.h"

#include <array>
#include <bit>
#include <print>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  // ==========================================================================
  // Utility Function Tests
  // ==========================================================================

  "rotl32 basic"_test = [] {
    // Rotating 0x80000000 left by 1 should give 0x00000001
    expectEq(rotl32(0x80000000, 1), 0x00000001u);

    // Rotating 0x00000001 left by 1 should give 0x00000002
    expectEq(rotl32(0x00000001, 1), 0x00000002u);

    // Full rotation (32 bits) returns original
    expectEq(rotl32(0xDEADBEEF, 32), 0xDEADBEEFu);

    // Zero rotation returns original
    expectEq(rotl32(0x12345678, 0), 0x12345678u);
  };

  "rotl64 basic"_test = [] {
    expectEq(rotl64(0x8000000000000000ULL, 1), 0x0000000000000001ULL);
    expectEq(rotl64(0x0000000000000001ULL, 1), 0x0000000000000002ULL);
    expectEq(rotl64(0xDEADBEEFCAFEBABEULL, 64), 0xDEADBEEFCAFEBABEULL);
  };

  "read32LE / read64LE"_test = [] {
    // Test little-endian reading
    std::array<std::uint8_t, 8> data = {0x01, 0x02, 0x03, 0x04,
                                        0x05, 0x06, 0x07, 0x08};

    // Little-endian: 0x04030201
    expectEq(read32LE(data.data()), 0x04030201u);

    // Little-endian: 0x0807060504030201
    expectEq(read64LE(data.data()), 0x0807060504030201ULL);
  };

  "hashCombine prevents self-cancellation"_test = [] {
    // XOR alone would give: hash(x,x) = 0
    // hashCombine should NOT have this property
    std::size_t seed1 = 0;
    std::size_t seed2 = 0;

    hashCombine(seed1, 42);
    hashCombine(seed1, 42);

    hashCombine(seed2, 42);

    // After combining 42 twice, seed should NOT be back to initial state
    expectNeq(seed1, std::size_t{0});

    // And should be different from combining once
    expectNeq(seed1, seed2);
  };

  "hashCombine is order-dependent"_test = [] {
    std::size_t seed1 = 0;
    std::size_t seed2 = 0;

    hashCombine(seed1, 1);
    hashCombine(seed1, 2);

    hashCombine(seed2, 2);
    hashCombine(seed2, 1);

    // hash(1, 2) should differ from hash(2, 1)
    expectNeq(seed1, seed2);
  };

  // ==========================================================================
  // FNV-1a Tests
  // ==========================================================================

  "FNV-1a empty string"_test = [] {
    // Empty string should return the offset basis
    auto hash32 = fnv1a32(nullptr, 0);
    expectEq(hash32, Fnv32Constants::kOffsetBasis);

    auto hash64 = fnv1a64(nullptr, 0);
    expectEq(hash64, Fnv64Constants::kOffsetBasis);
  };

  "FNV-1a known test vectors"_test = [] {
    // These are well-known test vectors from the FNV specification
    // Hash of "a"
    const std::uint8_t a[] = {'a'};
    auto hash_a = fnv1a32(a, 1);
    expectEq(hash_a, 0xe40c292cU);

    // Hash of "foobar"
    const std::uint8_t foobar[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    auto hash_foobar = fnv1a32(foobar, 6);
    expectEq(hash_foobar, 0xbf9cf968U);
  };

  "FNV-1a string_view hasher"_test = [] {
    Fnv1aHash32<std::string_view> hasher32;
    Fnv1aHash64<std::string_view> hasher64;

    auto h1 = hasher32("hello");
    auto h2 = hasher32("hello");
    auto h3 = hasher32("world");

    // Deterministic
    expectEq(h1, h2);
    // Different inputs -> different hashes
    expectNeq(h1, h3);

    std::println("FNV-1a32('hello') = 0x{:08x}", h1);
    std::println("FNV-1a64('hello') = 0x{:016x}", hasher64("hello"));
  };

  "FNV-1a integer hasher"_test = [] {
    Fnv1aHash32<std::uint32_t> hasher32;

    auto h1 = hasher32(0);
    auto h2 = hasher32(1);
    auto h3 = hasher32(0xFFFFFFFF);

    // Different integers should hash differently
    expectNeq(h1, h2);
    expectNeq(h2, h3);
    expectNeq(h1, h3);

    std::println("FNV-1a32(0) = 0x{:08x}", h1);
    std::println("FNV-1a32(1) = 0x{:08x}", h2);
  };

  // ==========================================================================
  // MurmurHash3 Tests
  // ==========================================================================

  "MurmurHash3 finalizer fmix32"_test = [] {
    // The finalizer should have full avalanche
    // Every input bit should affect every output bit
    auto h1 = fmix32(0);
    auto h2 = fmix32(1);

    // Single bit change should affect many output bits
    auto diff = std::popcount(h1 ^ h2);
    std::println("fmix32: 1 bit input change -> {} bit output change", diff);

    // Expect at least 25% of bits to change (strong avalanche would be ~50%)
    expectGE(diff, 8);
  };

  "MurmurHash3 empty string"_test = [] {
    auto hash = murmurHash3_32(nullptr, 0, 0);
    // With seed=0 and empty input, MurmurHash3 produces 0
    // This is correct: fmix32(0 ^ 0) = fmix32(0) = 0
    std::println("MurmurHash3_32('', seed=0) = 0x{:08x}", hash);
    expectEq(hash, 0u);  // Expected: 0 for empty with seed=0

    // With non-zero seed, should produce non-zero
    auto hash_seeded = murmurHash3_32(nullptr, 0, 42);
    expectNeq(hash_seeded, 0u);
    std::println("MurmurHash3_32('', seed=42) = 0x{:08x}", hash_seeded);
  };

  "MurmurHash3 seed affects output"_test = [] {
    const std::uint8_t data[] = {'t', 'e', 's', 't'};

    auto h1 = murmurHash3_32(data, 4, 0);
    auto h2 = murmurHash3_32(data, 4, 1);
    auto h3 = murmurHash3_32(data, 4, 42);

    // Different seeds should produce different hashes
    expectNeq(h1, h2);
    expectNeq(h2, h3);
    expectNeq(h1, h3);

    std::println("MurmurHash3_32('test', seed=0) = 0x{:08x}", h1);
    std::println("MurmurHash3_32('test', seed=1) = 0x{:08x}", h2);
    std::println("MurmurHash3_32('test', seed=42) = 0x{:08x}", h3);
  };

  "MurmurHash3 tail handling"_test = [] {
    // Test inputs of various lengths to exercise tail handling (1-3 bytes)
    const std::uint8_t one[] = {'a'};
    const std::uint8_t two[] = {'a', 'b'};
    const std::uint8_t three[] = {'a', 'b', 'c'};
    const std::uint8_t four[] = {'a', 'b', 'c', 'd'};

    auto h1 = murmurHash3_32(one, 1);
    auto h2 = murmurHash3_32(two, 2);
    auto h3 = murmurHash3_32(three, 3);
    auto h4 = murmurHash3_32(four, 4);

    // All should be different
    expectNeq(h1, h2);
    expectNeq(h2, h3);
    expectNeq(h3, h4);

    std::println("MurmurHash3 tail test: len=1: 0x{:08x}, len=2: 0x{:08x}, "
                 "len=3: 0x{:08x}, len=4: 0x{:08x}",
                 h1, h2, h3, h4);
  };

  "MurmurHash3 64-bit variant"_test = [] {
    MurmurHash3_64<std::string_view> hasher;

    auto h1 = hasher("hello world");
    auto h2 = hasher("hello world!");

    expectNeq(h1, h2);

    std::println("MurmurHash3_64('hello world') = 0x{:016x}", h1);
  };

  // ==========================================================================
  // wyhash Tests
  // ==========================================================================

  "wymum basic mixing"_test = [] {
    // wymum(0, 0) should still produce non-zero output due to XOR folding
    auto h = wymum(0, 0);
    expectEq(h, 0ULL);  // Actually 0*0 XOR-folded is 0

    // But with any non-zero input, we get mixing
    auto h1 = wymum(1, 1);
    auto h2 = wymum(2, 1);
    expectNeq(h1, h2);
    expectNeq(h1, 0ULL);

    std::println("wymum(1, 1) = 0x{:016x}", h1);
  };

  "wyhash empty string"_test = [] {
    auto hash = wyhash(nullptr, 0);
    std::println("wyhash('', seed=0) = 0x{:016x}", hash);
    // Empty string with seed=0 now produces a non-trivial hash
    // from mixing the seed with the prime
    expectNeq(hash, 0ULL);
  };

  "wyhash short strings"_test = [] {
    // Test various short string lengths (hot path in hash tables)
    WyHash<std::string_view> hasher;

    auto h1 = hasher("a");
    auto h2 = hasher("ab");
    auto h3 = hasher("abc");
    auto h4 = hasher("abcd");
    auto h8 = hasher("abcdefgh");
    auto h16 = hasher("abcdefghijklmnop");

    // All should be different
    std::unordered_set<std::size_t> hashes = {h1, h2, h3, h4, h8, h16};
    expectEq(hashes.size(), 6UL);

    std::println("wyhash short strings: len=1: 0x{:016x}, len=4: 0x{:016x}, "
                 "len=8: 0x{:016x}",
                 h1, h4, h8);
  };

  "wyhash long strings"_test = [] {
    // Test strings longer than 48 bytes to exercise bulk processing
    std::string long_str(100, 'x');
    std::string long_str2(100, 'y');

    auto h1 = wyhash(reinterpret_cast<const std::uint8_t*>(long_str.data()),
                     long_str.size());
    auto h2 = wyhash(reinterpret_cast<const std::uint8_t*>(long_str2.data()),
                     long_str2.size());

    expectNeq(h1, h2);
    std::println("wyhash(100 x's) = 0x{:016x}", h1);
  };

  "wyhash integer hasher"_test = [] {
    WyHash<std::uint64_t> hasher;

    auto h0 = hasher(0);
    auto h1 = hasher(1);
    auto hmax = hasher(std::numeric_limits<std::uint64_t>::max());

    expectNeq(h0, h1);
    expectNeq(h1, hmax);

    std::println("WyHash(0) = 0x{:016x}", h0);
    std::println("WyHash(1) = 0x{:016x}", h1);
    std::println("WyHash(MAX) = 0x{:016x}", hmax);
  };

  // ==========================================================================
  // Distribution Quality Tests
  // ==========================================================================

  "Distribution: sequential integers"_test = [] {
    // Hash sequential integers and check bucket distribution
    // Poor hash functions cluster consecutive integers
    constexpr int kNumKeys = 10000;
    constexpr int kNumBuckets = 100;

    std::array<int, kNumBuckets> fnv_buckets{};
    std::array<int, kNumBuckets> murmur_buckets{};
    std::array<int, kNumBuckets> wy_buckets{};

    Fnv1aHash64<std::uint64_t> fnv;
    MurmurHash3_64<std::uint64_t> murmur;
    WyHash<std::uint64_t> wy;

    for (int i = 0; i < kNumKeys; ++i) {
      fnv_buckets[fnv(i) % kNumBuckets]++;
      murmur_buckets[murmur(i) % kNumBuckets]++;
      wy_buckets[wy(i) % kNumBuckets]++;
    }

    // Check variance - ideal would be kNumKeys/kNumBuckets = 100 per bucket
    auto calcVariance = [](const std::array<int, kNumBuckets>& buckets) {
      double mean = kNumKeys / static_cast<double>(kNumBuckets);
      double variance = 0;
      for (int count : buckets) {
        double diff = count - mean;
        variance += diff * diff;
      }
      return variance / kNumBuckets;
    };

    double fnv_var = calcVariance(fnv_buckets);
    double murmur_var = calcVariance(murmur_buckets);
    double wy_var = calcVariance(wy_buckets);

    std::println("Distribution variance (lower is better):");
    std::println("  FNV-1a:    {:.1f}", fnv_var);
    std::println("  MurmurHash3: {:.1f}", murmur_var);
    std::println("  wyhash:    {:.1f}", wy_var);

    // All should have reasonable distribution (variance < 200 for random)
    expectLT(fnv_var, 500.0);
    expectLT(murmur_var, 500.0);
    expectLT(wy_var, 500.0);
  };

  "Avalanche: single bit changes"_test = [] {
    // Test avalanche effect: changing 1 input bit should change ~50% output bits
    WyHash<std::uint64_t> hasher;

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
    std::println("wyhash avalanche: avg bits flipped per 1-bit input change: "
                 "{:.1f}/64 ({:.1f}%)",
                 avg_flips, avg_flips * 100 / 64);

    // Expect at least 40% of bits to flip (ideal is 50%)
    expectGT(avg_flips, 25.0);
  };

  // ==========================================================================
  // Constexpr Tests
  // ==========================================================================

  "Constexpr FNV-1a"_test = [] {
    // FNV-1a should be fully constexpr
    constexpr std::uint8_t data[] = {'t', 'e', 's', 't'};
    constexpr auto hash = fnv1a32(data, 4);

    static_assert(hash != 0, "FNV-1a should work at compile time");
    std::println("Constexpr FNV-1a32('test') = 0x{:08x}", hash);
  };

  "Constexpr MurmurHash3"_test = [] {
    constexpr std::uint8_t data[] = {'t', 'e', 's', 't'};
    constexpr auto hash = murmurHash3_32(data, 4);

    static_assert(hash != 0, "MurmurHash3 should work at compile time");
    std::println("Constexpr MurmurHash3_32('test') = 0x{:08x}", hash);
  };

  "Constexpr wyhash"_test = [] {
    constexpr std::uint8_t data[] = {'t', 'e', 's', 't'};
    constexpr auto hash = wyhash(data, 4);

    static_assert(hash != 0, "wyhash should work at compile time");
    std::println("Constexpr wyhash('test') = 0x{:016x}", hash);
  };

  return TestRegistry::result();
}
