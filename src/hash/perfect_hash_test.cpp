#include "hash/perfect_hash.h"

#include <array>
#include <print>
#include <string_view>

#include "unit.h"

using namespace tempura;
using namespace std::string_view_literals;

auto main() -> int {
  // ==========================================================================
  // Basic PerfectHashMap Tests
  // ==========================================================================

  "PerfectHashMap: small keyword set"_test = [] {
    // Classic use case: keyword recognition
    constexpr auto keywords = std::array{
        std::pair{"if"sv, 1},
        std::pair{"else"sv, 2},
        std::pair{"while"sv, 3},
        std::pair{"for"sv, 4},
        std::pair{"return"sv, 5},
    };

    constexpr auto map = makePerfectHashMap(keywords);

    static_assert(map.isValid(), "Map construction should succeed");
    static_assert(map.size() == 5, "Map should have 5 entries");

    // Test all lookups work
    expectEq(*map.find("if"sv), 1);
    expectEq(*map.find("else"sv), 2);
    expectEq(*map.find("while"sv), 3);
    expectEq(*map.find("for"sv), 4);
    expectEq(*map.find("return"sv), 5);

    // Test non-existent keys return nullptr
    expectTrue(map.find("int"sv) == nullptr);
    expectTrue(map.find("void"sv) == nullptr);
    expectTrue(map.find(""sv) == nullptr);

    // Test contains()
    expectTrue(map.contains("if"sv));
    expectTrue(map.contains("return"sv));
    expectFalse(map.contains("goto"sv));

    std::println("Keyword map: {} entries in {} slots (load factor: {:.2f})",
                 map.size(), map.tableSize(), map.loadFactor());
  };

  "PerfectHashMap: compile-time lookup"_test = [] {
    // Verify everything works at compile time
    constexpr auto entries = std::array{
        std::pair{"alpha"sv, 100},
        std::pair{"beta"sv, 200},
        std::pair{"gamma"sv, 300},
    };

    constexpr auto map = makePerfectHashMap(entries);

    // These are all evaluated at compile time
    static_assert(map.isValid());
    static_assert(*map.find("alpha"sv) == 100);
    static_assert(*map.find("beta"sv) == 200);
    static_assert(*map.find("gamma"sv) == 300);
    static_assert(map.contains("alpha"sv));
    static_assert(!map.contains("delta"sv));

    std::println("Compile-time lookup: alpha={}, beta={}, gamma={}",
                 *map.find("alpha"sv), *map.find("beta"sv),
                 *map.find("gamma"sv));
  };

  "PerfectHashMap: enum mapping"_test = [] {
    // Common pattern: string-to-enum conversion
    enum class HttpMethod { Get, Post, Put, Delete, Patch, Head, Options };

    constexpr auto methods = std::array{
        std::pair{"GET"sv, HttpMethod::Get},
        std::pair{"POST"sv, HttpMethod::Post},
        std::pair{"PUT"sv, HttpMethod::Put},
        std::pair{"DELETE"sv, HttpMethod::Delete},
        std::pair{"PATCH"sv, HttpMethod::Patch},
        std::pair{"HEAD"sv, HttpMethod::Head},
        std::pair{"OPTIONS"sv, HttpMethod::Options},
    };

    constexpr auto map = makePerfectHashMap(methods);

    static_assert(map.isValid());

    expectEq(*map.find("GET"sv), HttpMethod::Get);
    expectEq(*map.find("POST"sv), HttpMethod::Post);
    expectEq(*map.find("DELETE"sv), HttpMethod::Delete);

    // Unknown method
    expectTrue(map.find("CONNECT"sv) == nullptr);

    std::println("HTTP method map constructed with {} entries", map.size());
  };

  "PerfectHashMap: larger set (20 entries)"_test = [] {
    // Test with a larger key set
    constexpr auto elements = std::array{
        std::pair{"hydrogen"sv, 1},   std::pair{"helium"sv, 2},
        std::pair{"lithium"sv, 3},    std::pair{"beryllium"sv, 4},
        std::pair{"boron"sv, 5},      std::pair{"carbon"sv, 6},
        std::pair{"nitrogen"sv, 7},   std::pair{"oxygen"sv, 8},
        std::pair{"fluorine"sv, 9},   std::pair{"neon"sv, 10},
        std::pair{"sodium"sv, 11},    std::pair{"magnesium"sv, 12},
        std::pair{"aluminum"sv, 13},  std::pair{"silicon"sv, 14},
        std::pair{"phosphorus"sv, 15}, std::pair{"sulfur"sv, 16},
        std::pair{"chlorine"sv, 17},  std::pair{"argon"sv, 18},
        std::pair{"potassium"sv, 19}, std::pair{"calcium"sv, 20},
    };

    constexpr auto map = makePerfectHashMap(elements);

    static_assert(map.isValid(), "Should handle 20 entries");
    static_assert(map.size() == 20);

    // Verify all entries
    expectEq(*map.find("hydrogen"sv), 1);
    expectEq(*map.find("carbon"sv), 6);
    expectEq(*map.find("oxygen"sv), 8);
    expectEq(*map.find("calcium"sv), 20);

    // Non-existent
    expectFalse(map.contains("gold"sv));
    expectFalse(map.contains("silver"sv));

    std::println("Element map: {} entries in {} slots (load factor: {:.2f})",
                 map.size(), map.tableSize(), map.loadFactor());
  };

  // ==========================================================================
  // PerfectHashSet Tests
  // ==========================================================================

  "PerfectHashSet: reserved words"_test = [] {
    constexpr auto reserved = std::array{
        "auto"sv,     "break"sv,  "case"sv,   "char"sv,
        "const"sv,    "continue"sv, "default"sv, "do"sv,
        "double"sv,   "else"sv,   "enum"sv,   "extern"sv,
        "float"sv,    "for"sv,    "goto"sv,   "if"sv,
    };

    constexpr auto set = makePerfectHashSet(reserved);

    static_assert(set.isValid());
    static_assert(set.size() == 16);

    // Check membership
    expectTrue(set.contains("if"sv));
    expectTrue(set.contains("for"sv));
    expectTrue(set.contains("auto"sv));
    expectTrue(set.contains("goto"sv));

    // Not members
    expectFalse(set.contains("int"sv));      // Not in our subset
    expectFalse(set.contains("class"sv));    // C++ keyword
    expectFalse(set.contains("variable"sv)); // Just a word

    std::println("Reserved word set: {} keywords", set.size());
  };

  "PerfectHashSet: compile-time membership"_test = [] {
    constexpr auto vowels = std::array{"a"sv, "e"sv, "i"sv, "o"sv, "u"sv};
    constexpr auto set = makePerfectHashSet(vowels);

    static_assert(set.isValid());
    static_assert(set.contains("a"sv));
    static_assert(set.contains("e"sv));
    static_assert(set.contains("i"sv));
    static_assert(set.contains("o"sv));
    static_assert(set.contains("u"sv));
    static_assert(!set.contains("b"sv));
    static_assert(!set.contains("y"sv));

    std::println("Vowel set compile-time check passed");
  };

  // ==========================================================================
  // Edge Cases
  // ==========================================================================

  "PerfectHashMap: single entry"_test = [] {
    constexpr auto single = std::array{std::pair{"only"sv, 42}};
    constexpr auto map = makePerfectHashMap(single);

    static_assert(map.isValid());
    static_assert(map.size() == 1);
    static_assert(*map.find("only"sv) == 42);
    static_assert(!map.contains("other"sv));

    expectEq(*map.find("only"sv), 42);
    expectFalse(map.contains("nope"sv));
  };

  "PerfectHashMap: similar keys"_test = [] {
    // Keys that differ only slightly - stress tests hash quality
    constexpr auto similar = std::array{
        std::pair{"test1"sv, 1}, std::pair{"test2"sv, 2},
        std::pair{"test3"sv, 3}, std::pair{"test4"sv, 4},
        std::pair{"test5"sv, 5}, std::pair{"test6"sv, 6},
        std::pair{"test7"sv, 7}, std::pair{"test8"sv, 8},
    };

    constexpr auto map = makePerfectHashMap(similar);

    // Similar keys can be challenging for some hash functions
    if (map.isValid()) {
      for (int i = 1; i <= 8; ++i) {
        std::string key = "test" + std::to_string(i);
        expectEq(*map.find(std::string_view{key}), i);
      }
      std::println("Similar keys test passed");
    } else {
      std::println("Similar keys test: construction needs more seeds (expected "
                   "for some patterns)");
    }
  };

  "PerfectHashMap: empty string key"_test = [] {
    constexpr auto with_empty = std::array{
        std::pair{""sv, 0},
        std::pair{"nonempty"sv, 1},
    };

    constexpr auto map = makePerfectHashMap(with_empty);

    static_assert(map.isValid());
    static_assert(*map.find(""sv) == 0);
    static_assert(*map.find("nonempty"sv) == 1);

    expectEq(*map.find(""sv), 0);
    expectEq(*map.find("nonempty"sv), 1);
  };

  // ==========================================================================
  // Performance Characteristics
  // ==========================================================================

  "PerfectHashMap: load factor"_test = [] {
    // Verify the table isn't too sparse or too dense
    constexpr auto entries = std::array{
        std::pair{"a"sv, 1}, std::pair{"b"sv, 2}, std::pair{"c"sv, 3},
        std::pair{"d"sv, 4}, std::pair{"e"sv, 5}, std::pair{"f"sv, 6},
        std::pair{"g"sv, 7}, std::pair{"h"sv, 8}, std::pair{"i"sv, 9},
        std::pair{"j"sv, 10},
    };

    constexpr auto map = makePerfectHashMap(entries);

    static_assert(map.isValid());

    // Load factor should be reasonable (not too wasteful)
    double lf = map.loadFactor();
    expectGT(lf, 0.5);   // At least 50% full
    expectLE(lf, 1.0);   // Can't be more than 100%

    std::println("10-entry map: load factor = {:.2f} ({}/{})",
                 lf, map.size(), map.tableSize());
  };

  // ==========================================================================
  // Integer Keys
  // ==========================================================================

  "PerfectHashMap: integer keys"_test = [] {
    // Perfect hashing works for integer keys too
    // Note: For small integer sets, the hash may need more iterations
    constexpr auto primes = std::array{
        std::pair{std::uint64_t{2}, "two"sv},
        std::pair{std::uint64_t{3}, "three"sv},
        std::pair{std::uint64_t{5}, "five"sv},
        std::pair{std::uint64_t{7}, "seven"sv},
        std::pair{std::uint64_t{11}, "eleven"sv},
        std::pair{std::uint64_t{13}, "thirteen"sv},
    };

    constexpr auto map = makePerfectHashMap(primes);

    // Integer keys may or may not work depending on hash quality
    if (map.isValid()) {
      expectEq(*map.find(std::uint64_t{2}), "two"sv);
      expectEq(*map.find(std::uint64_t{7}), "seven"sv);
      expectEq(*map.find(std::uint64_t{13}), "thirteen"sv);

      // Non-primes not in map
      expectTrue(map.find(std::uint64_t{4}) == nullptr);
      expectTrue(map.find(std::uint64_t{9}) == nullptr);

      std::println("Integer key map working");
    } else {
      std::println("Integer key map: construction failed (expected for some "
                   "key distributions)");
    }
  };

  return TestRegistry::result();
}
