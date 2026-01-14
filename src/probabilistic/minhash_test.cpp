#include "probabilistic/minhash.h"

#include <set>
#include <string>
#include <vector>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "identical sets have similarity ~1.0"_test = [] {
    MinHash<128> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<128>::estimateSimilarity(sig_a, sig_b);
    // Identical sets should have identical signatures
    expectEq(similarity, 1.0);
  };

  "disjoint sets have similarity ~0.0"_test = [] {
    MinHash<128> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {101, 102, 103, 104, 105, 106, 107, 108, 109, 110};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<128>::estimateSimilarity(sig_a, sig_b);
    // Disjoint sets have J=0, but random collisions may produce small similarity
    // With 128 hashes, random collision probability per hash is very low
    // Tolerance: ~10% accounts for rare coincidental collisions
    expectLT(similarity, 0.1);
  };

  "50% overlap gives ~0.5 similarity"_test = [] {
    MinHash<256> mh;  // More hashes for better accuracy
    // A = {1..100}, B = {51..150}
    // |A ∩ B| = 50, |A ∪ B| = 150, J = 50/150 = 1/3 ≈ 0.333
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 51; i <= 150; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<256>::estimateSimilarity(sig_a, sig_b);
    // Standard error = sqrt(J(1-J)/k) = sqrt(0.333*0.667/256) ≈ 0.029
    // Tolerance of 0.15 is ~5 standard errors
    expectNear(similarity, 0.333, 0.15);
  };

  "75% overlap gives ~0.6 similarity"_test = [] {
    MinHash<256> mh;
    // A = {1..100}, B = {26..125}
    // Overlap: 26-100 = 75 elements
    // |A ∩ B| = 75, |A ∪ B| = 125, J = 75/125 = 0.6
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 26; i <= 125; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<256>::estimateSimilarity(sig_a, sig_b);
    // Standard error = sqrt(0.6*0.4/256) ≈ 0.031
    // Tolerance of 0.15 is ~5 standard errors
    expectNear(similarity, 0.6, 0.15);
  };

  "merge produces union signature"_test = [] {
    MinHash<128> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5};
    std::vector<int> set_b = {6, 7, 8, 9, 10};
    std::vector<int> set_union = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);
    auto sig_union_direct = mh.computeSignature(set_union);
    auto sig_union_merged = MinHash<128>::merge(sig_a, sig_b);

    // Merged signature should equal signature computed directly from union
    expectEq(sig_union_merged, sig_union_direct);
  };

  "merge is commutative and associative"_test = [] {
    MinHash<64> mh;
    std::vector<int> set_a = {1, 2, 3};
    std::vector<int> set_b = {4, 5, 6};
    std::vector<int> set_c = {7, 8, 9};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);
    auto sig_c = mh.computeSignature(set_c);

    // Commutative: merge(a, b) == merge(b, a)
    auto ab = MinHash<64>::merge(sig_a, sig_b);
    auto ba = MinHash<64>::merge(sig_b, sig_a);
    expectEq(ab, ba);

    // Associative: merge(merge(a, b), c) == merge(a, merge(b, c))
    auto ab_c = MinHash<64>::merge(ab, sig_c);
    auto bc = MinHash<64>::merge(sig_b, sig_c);
    auto a_bc = MinHash<64>::merge(sig_a, bc);
    expectEq(ab_c, a_bc);
  };

  "empty set has all-max signature"_test = [] {
    MinHash<64> mh;
    std::vector<int> empty_set;

    auto sig = mh.computeSignature(empty_set);

    // Empty set should have signature with all max values
    for (std::size_t i = 0; i < 64; ++i) {
      expectEq(sig[i], std::numeric_limits<std::uint64_t>::max());
    }
  };

  "empty set similarity with non-empty is 0"_test = [] {
    MinHash<64> mh;
    std::vector<int> empty_set;
    std::vector<int> non_empty = {1, 2, 3};

    auto sig_empty = mh.computeSignature(empty_set);
    auto sig_non_empty = mh.computeSignature(non_empty);

    double similarity = MinHash<64>::estimateSimilarity(sig_empty, sig_non_empty);
    // Empty vs non-empty: signatures won't match (max vs actual hash values)
    expectEq(similarity, 0.0);
  };

  "empty set similarity with empty is 1"_test = [] {
    MinHash<64> mh;
    std::vector<int> empty_a;
    std::vector<int> empty_b;

    auto sig_a = mh.computeSignature(empty_a);
    auto sig_b = mh.computeSignature(empty_b);

    double similarity = MinHash<64>::estimateSimilarity(sig_a, sig_b);
    // Two empty sets have identical signatures
    expectEq(similarity, 1.0);
  };

  "string sets"_test = [] {
    MinHash<128> mh;
    std::vector<std::string> set_a = {"apple", "banana", "cherry", "date"};
    std::vector<std::string> set_b = {"apple", "banana", "cherry", "date"};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<128>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "works with std::set"_test = [] {
    MinHash<128> mh;
    std::set<int> set_a = {5, 3, 1, 4, 2};  // Order shouldn't matter
    std::set<int> set_b = {1, 2, 3, 4, 5};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<128>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "static properties"_test = [] {
    expectEq(MinHash<128>::numHashes(), 128uz);
    expectEq(MinHash<64>::numHashes(), 64uz);
    expectEq(MinHash<256>::numHashes(), 256uz);
  };

  "signature self-similarity is always 1.0"_test = [] {
    MinHash<128> mh;
    std::vector<int> set = {10, 20, 30, 40, 50};

    auto sig = mh.computeSignature(set);

    double similarity = MinHash<128>::estimateSimilarity(sig, sig);
    expectEq(similarity, 1.0);
  };

  "larger sets estimate similarity correctly"_test = [] {
    MinHash<256> mh;
    // Create two sets with exactly 50% Jaccard similarity
    // A = {0..999}, B = {500..1499}
    // |A ∩ B| = 500, |A ∪ B| = 1500, J = 500/1500 = 1/3
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 0; i < 1000; ++i) set_a.push_back(i);
    for (int i = 500; i < 1500; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = MinHash<256>::estimateSimilarity(sig_a, sig_b);
    // Standard error ≈ sqrt(0.333 * 0.667 / 256) ≈ 0.029
    // Tolerance of 0.15 is very generous
    expectNear(similarity, 0.333, 0.15);
  };

  return TestRegistry::result();
}
