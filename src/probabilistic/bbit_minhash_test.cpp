#include "probabilistic/bbit_minhash.h"

#include <set>
#include <string>
#include <vector>

#include "probabilistic/minhash.h"
#include "unit.h"

using namespace tempura;

auto main() -> int {
  "identical sets have similarity ~1.0"_test = [] {
    BbitMinHash<256, 1> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<256, 1>::estimateSimilarity(sig_a, sig_b);
    // Identical sets should have identical signatures
    expectEq(similarity, 1.0);
  };

  "disjoint sets have similarity ~0.0"_test = [] {
    BbitMinHash<256, 1> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {101, 102, 103, 104, 105, 106, 107, 108, 109, 110};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<256, 1>::estimateSimilarity(sig_a, sig_b);
    // Disjoint sets have J=0, with b-bit we expect roughly 0 but may have noise.
    // With 256 hashes, random collision on 1-bit values ~50% raw match rate,
    // but the correction formula adjusts this down toward 0.
    expectLT(similarity, 0.15);
  };

  "50% overlap gives correct similarity estimate"_test = [] {
    BbitMinHash<512, 1> mh;  // More hashes for better accuracy
    // A = {1..100}, B = {51..150}
    // |A ∩ B| = 50, |A ∪ B| = 150, J = 50/150 = 1/3 ≈ 0.333
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 51; i <= 150; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<512, 1>::estimateSimilarity(sig_a, sig_b);
    // b-bit has higher variance than standard MinHash
    // Tolerance of 0.2 accounts for this
    expectNear(similarity, 0.333, 0.2);
  };

  "75% overlap gives correct similarity estimate"_test = [] {
    BbitMinHash<512, 1> mh;
    // A = {1..100}, B = {26..125}
    // |A ∩ B| = 75, |A ∪ B| = 125, J = 75/125 = 0.6
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 26; i <= 125; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<512, 1>::estimateSimilarity(sig_a, sig_b);
    expectNear(similarity, 0.6, 0.2);
  };

  "empty set has all-max signature bits"_test = [] {
    BbitMinHash<64, 1> mh;
    std::vector<int> empty_set;

    auto sig = mh.computeSignature(empty_set);

    // Empty set starts with all max values, so all lowest bits are 1
    // (since max uint64_t has all bits set)
    expectEq(sig[0], std::numeric_limits<std::uint64_t>::max());
  };

  "empty set similarity with non-empty is 0"_test = [] {
    BbitMinHash<64, 1> mh;
    std::vector<int> empty_set;
    std::vector<int> non_empty = {1, 2, 3};

    auto sig_empty = mh.computeSignature(empty_set);
    auto sig_non_empty = mh.computeSignature(non_empty);

    double similarity = BbitMinHash<64, 1>::estimateSimilarity(sig_empty, sig_non_empty);
    // Empty vs non-empty should be very different
    expectLT(similarity, 0.3);
  };

  "empty set similarity with empty is 1"_test = [] {
    BbitMinHash<64, 1> mh;
    std::vector<int> empty_a;
    std::vector<int> empty_b;

    auto sig_a = mh.computeSignature(empty_a);
    auto sig_b = mh.computeSignature(empty_b);

    double similarity = BbitMinHash<64, 1>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "signature self-similarity is always 1.0"_test = [] {
    BbitMinHash<128, 1> mh;
    std::vector<int> set = {10, 20, 30, 40, 50};

    auto sig = mh.computeSignature(set);

    double similarity = BbitMinHash<128, 1>::estimateSimilarity(sig, sig);
    expectEq(similarity, 1.0);
  };

  "countMatches returns k for identical signatures"_test = [] {
    BbitMinHash<256, 1> mh;
    std::vector<int> set = {1, 2, 3, 4, 5};

    auto sig = mh.computeSignature(set);

    std::size_t matches = BbitMinHash<256, 1>::countMatches(sig, sig);
    expectEq(matches, 256uz);
  };

  "space savings: b=1 signature is 64x smaller than standard"_test = [] {
    // Standard MinHash with 256 hashes: 256 * 8 = 2048 bytes
    // b-bit with b=1 and 256 hashes: 256/8 = 32 bytes
    constexpr std::size_t standard_size = 256 * sizeof(std::uint64_t);
    constexpr std::size_t bbit_size = BbitMinHash<256, 1>::signatureSizeBytes();

    expectEq(standard_size, 2048uz);
    expectEq(bbit_size, 32uz);
    expectEq(standard_size / bbit_size, 64uz);
  };

  "compression ratio is correct for different bit widths"_test = [] {
    expectEq(BbitMinHash<256, 1>::compressionRatio(), 64.0);
    expectEq(BbitMinHash<256, 2>::compressionRatio(), 32.0);
    expectEq(BbitMinHash<256, 4>::compressionRatio(), 16.0);
    expectEq(BbitMinHash<256, 8>::compressionRatio(), 8.0);
    expectEq(BbitMinHash<256, 16>::compressionRatio(), 4.0);
    expectEq(BbitMinHash<256, 32>::compressionRatio(), 2.0);
    expectEq(BbitMinHash<256, 64>::compressionRatio(), 1.0);
  };

  "static properties"_test = [] {
    expectEq(BbitMinHash<256, 1>::numHashes(), 256uz);
    expectEq(BbitMinHash<256, 1>::numBits(), 1uz);
    expectEq(BbitMinHash<128, 4>::numHashes(), 128uz);
    expectEq(BbitMinHash<128, 4>::numBits(), 4uz);
  };

  "b=2 gives correct estimates"_test = [] {
    BbitMinHash<512, 2> mh;
    std::vector<int> set_a;
    std::vector<int> set_b;
    // J = 0.5 exactly: A = {1..100}, B = {51..150}, overlap = 50
    // |A ∩ B| = 50, |A ∪ B| = 150, J = 1/3
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 51; i <= 150; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<512, 2>::estimateSimilarity(sig_a, sig_b);
    expectNear(similarity, 0.333, 0.2);
  };

  "b=4 gives correct estimates"_test = [] {
    BbitMinHash<512, 4> mh;
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 51; i <= 150; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<512, 4>::estimateSimilarity(sig_a, sig_b);
    expectNear(similarity, 0.333, 0.2);
  };

  "b=8 gives correct estimates"_test = [] {
    BbitMinHash<256, 8> mh;
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 51; i <= 150; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<256, 8>::estimateSimilarity(sig_a, sig_b);
    expectNear(similarity, 0.333, 0.15);
  };

  "comparison with standard MinHash accuracy"_test = [] {
    // Both should give roughly the same estimate for identical sets
    MinHash<256> standard_mh;
    BbitMinHash<256, 1> bbit_mh;

    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto standard_sig_a = standard_mh.computeSignature(set_a);
    auto standard_sig_b = standard_mh.computeSignature(set_b);
    double standard_sim =
        MinHash<256>::estimateSimilarity(standard_sig_a, standard_sig_b);

    auto bbit_sig_a = bbit_mh.computeSignature(set_a);
    auto bbit_sig_b = bbit_mh.computeSignature(set_b);
    double bbit_sim =
        BbitMinHash<256, 1>::estimateSimilarity(bbit_sig_a, bbit_sig_b);

    // Both should be exactly 1.0 for identical sets
    expectEq(standard_sim, 1.0);
    expectEq(bbit_sim, 1.0);
  };

  "b-bit signature size is smaller than standard"_test = [] {
    using Standard = MinHash<256>;
    using Bbit = BbitMinHash<256, 1>;

    constexpr std::size_t standard_size = sizeof(Standard::Signature);
    constexpr std::size_t bbit_size = sizeof(Bbit::Signature);

    expectEq(standard_size, 2048uz);  // 256 * 8 bytes
    expectEq(bbit_size, 32uz);        // 256 bits = 32 bytes
    expectLT(bbit_size, standard_size);
  };

  "string sets work correctly"_test = [] {
    BbitMinHash<128, 1> mh;
    std::vector<std::string> set_a = {"apple", "banana", "cherry", "date"};
    std::vector<std::string> set_b = {"apple", "banana", "cherry", "date"};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<128, 1>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "works with std::set"_test = [] {
    BbitMinHash<128, 1> mh;
    std::set<int> set_a = {5, 3, 1, 4, 2};
    std::set<int> set_b = {1, 2, 3, 4, 5};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<128, 1>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "larger sets estimate similarity correctly"_test = [] {
    BbitMinHash<512, 2> mh;
    // A = {0..999}, B = {500..1499}
    // |A ∩ B| = 500, |A ∪ B| = 1500, J = 1/3
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 0; i < 1000; ++i) set_a.push_back(i);
    for (int i = 500; i < 1500; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<512, 2>::estimateSimilarity(sig_a, sig_b);
    expectNear(similarity, 0.333, 0.15);
  };

  "high similarity sets (90% Jaccard)"_test = [] {
    BbitMinHash<512, 1> mh;
    // A = {1..100}, B = {1..90} ∪ {101..110}
    // |A ∩ B| = 90, |A ∪ B| = 110, J = 90/110 ≈ 0.818
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 1; i <= 90; ++i) set_b.push_back(i);
    for (int i = 101; i <= 110; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<512, 1>::estimateSimilarity(sig_a, sig_b);
    // High similarity is where b-bit shines - lower variance
    expectNear(similarity, 0.818, 0.15);
  };

  "odd number of hashes works correctly"_test = [] {
    BbitMinHash<127, 1> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5};
    std::vector<int> set_b = {1, 2, 3, 4, 5};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<127, 1>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);

    std::size_t matches = BbitMinHash<127, 1>::countMatches(sig_a, sig_b);
    expectEq(matches, 127uz);
  };

  "non-power-of-2 bits (b=3) works"_test = [] {
    BbitMinHash<256, 3> mh;
    std::vector<int> set_a = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> set_b = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<256, 3>::estimateSimilarity(sig_a, sig_b);
    expectEq(similarity, 1.0);
  };

  "non-power-of-2 bits (b=5) works"_test = [] {
    BbitMinHash<256, 5> mh;
    std::vector<int> set_a;
    std::vector<int> set_b;
    for (int i = 1; i <= 100; ++i) set_a.push_back(i);
    for (int i = 51; i <= 150; ++i) set_b.push_back(i);

    auto sig_a = mh.computeSignature(set_a);
    auto sig_b = mh.computeSignature(set_b);

    double similarity = BbitMinHash<256, 5>::estimateSimilarity(sig_a, sig_b);
    expectNear(similarity, 0.333, 0.2);
  };

  return TestRegistry::result();
}
