#include "probabilistic/one_permutation_hash.h"

#include <set>
#include <string>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "identical sets have similarity 1.0"_test = [] {
    OnePermutationHash<128> oph;
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<128>::estimateSimilarity(sig_a, sig_b);
    // Identical sets produce identical signatures
    expectEq(similarity, 1.0);
  };

  "disjoint sets have similarity ~0.0"_test = [] {
    OnePermutationHash<128> oph;
    // Use large disjoint sets to fill more bins
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 0; i < 500; ++i) set_a.push_back(i);
    for (int i = 1000; i < 1500; ++i) set_b.push_back(i);

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<128>::estimateSimilarity(sig_a, sig_b);
    // Disjoint sets should have very low similarity
    // Some random collisions possible, but rare
    expectLT(similarity, 0.1);
  };

  "overlapping sets estimate similarity correctly"_test = [] {
    OnePermutationHash<128> oph;
    // A = {0..499}, B = {250..749}
    // |A n B| = 250, |A u B| = 750, J = 250/750 = 1/3 ~ 0.333
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 0; i < 500; ++i) set_a.push_back(i);
    for (int i = 250; i < 750; ++i) set_b.push_back(i);

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<128>::estimateSimilarity(sig_a, sig_b);
    // Standard error ~ sqrt(0.333 * 0.667 / 128) ~ 0.042
    // Tolerance of 0.15 is ~3.5 standard errors
    expectNear(similarity, 0.333, 0.15);
  };

  "high overlap gives high similarity"_test = [] {
    OnePermutationHash<128> oph;
    // A = {0..499}, B = {100..599}
    // |A n B| = 400, |A u B| = 600, J = 400/600 = 2/3 ~ 0.667
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 0; i < 500; ++i) set_a.push_back(i);
    for (int i = 100; i < 600; ++i) set_b.push_back(i);

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<128>::estimateSimilarity(sig_a, sig_b);
    // Standard error ~ sqrt(0.667 * 0.333 / 128) ~ 0.042
    expectNear(similarity, 0.667, 0.15);
  };

  "merge produces union signature"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<int> set_a;
    std::vector<int> set_b;
    std::vector<int> set_union;
    for (int i = 0; i < 100; ++i) set_a.push_back(i);
    for (int i = 100; i < 200; ++i) set_b.push_back(i);
    for (int i = 0; i < 200; ++i) set_union.push_back(i);

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);
    auto sig_union_direct = oph.computeSignature(set_union);
    auto sig_union_merged = OnePermutationHash<64>::merge(sig_a, sig_b);

    // Merged signature should equal signature computed directly from union
    expectEq(sig_union_merged, sig_union_direct);
  };

  "merge is commutative and associative"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<int> set_a;
    std::vector<int> set_b;
    std::vector<int> set_c;
    for (int i = 0; i < 50; ++i) set_a.push_back(i);
    for (int i = 50; i < 100; ++i) set_b.push_back(i);
    for (int i = 100; i < 150; ++i) set_c.push_back(i);

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);
    auto sig_c = oph.computeSignature(set_c);

    // Commutative: merge(a, b) == merge(b, a)
    auto ab = OnePermutationHash<64>::merge(sig_a, sig_b);
    auto ba = OnePermutationHash<64>::merge(sig_b, sig_a);
    expectEq(ab, ba);

    // Associative: merge(merge(a, b), c) == merge(a, merge(b, c))
    auto ab_c = OnePermutationHash<64>::merge(ab, sig_c);
    auto bc = OnePermutationHash<64>::merge(sig_b, sig_c);
    auto a_bc = OnePermutationHash<64>::merge(sig_a, bc);
    expectEq(ab_c, a_bc);
  };

  "empty set has all-empty signature"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<int> empty_set;

    auto sig = oph.computeSignature(empty_set);

    for (std::size_t i = 0; i < 64; ++i) {
      expectEq(sig[i], OnePermutationHash<64>::kEmpty);
    }
  };

  "empty set similarity with non-empty is 0"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<int> empty_set;
    std::vector<int> non_empty;
    for (int i = 0; i < 100; ++i) non_empty.push_back(i);

    auto sig_empty = oph.computeSignature(empty_set);
    auto sig_non_empty = oph.computeSignature(non_empty);

    double similarity = OnePermutationHash<64>::estimateSimilarity(sig_empty, sig_non_empty);
    // No valid bins (empty set has no non-empty bins)
    expectEq(similarity, 0.0);
  };

  "empty set similarity with empty is 1"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<int> empty_a;
    std::vector<int> empty_b;

    auto sig_a = oph.computeSignature(empty_a);
    auto sig_b = oph.computeSignature(empty_b);

    double similarity = OnePermutationHash<64>::estimateSimilarity(sig_a, sig_b);
    // Two empty sets are considered identical
    expectEq(similarity, 1.0);
  };

  "countNonEmpty returns correct count"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<int> small_set = {1, 2, 3, 4, 5};
    std::vector<int> large_set;
    for (int i = 0; i < 1000; ++i) large_set.push_back(i);

    auto sig_small = oph.computeSignature(small_set);
    auto sig_large = oph.computeSignature(large_set);

    // Small set fills few bins
    std::size_t small_count = OnePermutationHash<64>::countNonEmpty(sig_small);
    expectLE(small_count, 5uz);  // At most 5 bins (one per element)
    expectGT(small_count, 0uz);

    // Large set should fill most bins
    std::size_t large_count = OnePermutationHash<64>::countNonEmpty(sig_large);
    expectEq(large_count, 64uz);  // With 1000 elements, all 64 bins should be filled
  };

  "densify fills empty bins"_test = [] {
    OnePermutationHash<64> oph;
    // Small set will have many empty bins
    std::vector<int> small_set = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig = oph.computeSignature(small_set);
    std::size_t before = OnePermutationHash<64>::countNonEmpty(sig);
    expectLT(before, 64uz);  // Should have some empty bins

    bool success = OnePermutationHash<64>::densify(sig);
    expectTrue(success);

    std::size_t after = OnePermutationHash<64>::countNonEmpty(sig);
    expectEq(after, 64uz);  // All bins now filled
  };

  "densify fails on empty signature"_test = [] {
    std::vector<int> empty_set;
    OnePermutationHash<64> oph;

    auto sig = oph.computeSignature(empty_set);
    bool success = OnePermutationHash<64>::densify(sig);

    expectFalse(success);
  };

  "densified signatures estimate similarity correctly"_test = [] {
    OnePermutationHash<128> oph;
    // Use smaller sets that leave empty bins
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    std::vector<int> set_b = set_a;  // Identical

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    OnePermutationHash<128>::densify(sig_a);
    OnePermutationHash<128>::densify(sig_b);

    double similarity = OnePermutationHash<128>::estimateSimilarity(sig_a, sig_b);
    // After densification, identical sets should still be identical
    expectEq(similarity, 1.0);
  };

  "signature self-similarity is always 1.0"_test = [] {
    OnePermutationHash<128> oph;
    std::vector<int> set;
    for (int i = 0; i < 200; ++i) set.push_back(i);

    auto sig = oph.computeSignature(set);

    double similarity = OnePermutationHash<128>::estimateSimilarity(sig, sig);
    expectEq(similarity, 1.0);
  };

  "string sets"_test = [] {
    OnePermutationHash<64> oph;
    std::vector<std::string> set_a = {"apple", "banana", "cherry", "date", "elderberry"};
    std::vector<std::string> set_b = {"apple", "banana", "cherry", "date", "elderberry"};

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<64>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "works with std::set"_test = [] {
    OnePermutationHash<64> oph;
    std::set<int> set_a = {5, 3, 1, 4, 2, 10, 8, 6, 9, 7};
    std::set<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<64>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "static properties"_test = [] {
    expectEq(OnePermutationHash<128>::numBins(), 128uz);
    expectEq(OnePermutationHash<64>::numBins(), 64uz);
    expectEq(OnePermutationHash<256>::numBins(), 256uz);
  };

  "larger sets give more accurate estimates"_test = [] {
    OnePermutationHash<256> oph;
    // A = {0..999}, B = {500..1499}
    // |A n B| = 500, |A u B| = 1500, J = 500/1500 = 1/3
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 0; i < 1000; ++i) set_a.push_back(i);
    for (int i = 500; i < 1500; ++i) set_b.push_back(i);

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    double similarity = OnePermutationHash<256>::estimateSimilarity(sig_a, sig_b);
    // Standard error ~ sqrt(0.333 * 0.667 / 256) ~ 0.029
    // Tolerance of 0.1 is ~3.5 standard errors
    expectNear(similarity, 0.333, 0.1);
  };

  "sparse sets handle empty bins gracefully"_test = [] {
    OnePermutationHash<256> oph;
    // Very sparse: only 10 elements in 256 bins
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = oph.computeSignature(set_a);
    auto sig_b = oph.computeSignature(set_b);

    // Most bins will be empty, but valid bins should match
    double similarity = OnePermutationHash<256>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);

    // Verify sparsity
    std::size_t non_empty = OnePermutationHash<256>::countNonEmpty(sig_a);
    expectLE(non_empty, 10uz);  // At most 10 bins filled
  };

  return TestRegistry::result();
}
