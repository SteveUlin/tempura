#include "probabilistic/count_min_sketch.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "empty sketch returns 0"_test = [] {
    CountMinSketch<1024, 4> sketch;

    // All queries should return 0
    expectEq(sketch.estimate(0), 0u);
    expectEq(sketch.estimate(42), 0u);
    expectEq(sketch.estimate(UINT64_MAX), 0u);
    expectEq(sketch.totalCount(), 0ull);
  };

  "single item tracking is exact"_test = [] {
    CountMinSketch<1024, 4> sketch;

    sketch.add(42);
    expectEq(sketch.estimate(42), 1u);

    sketch.add(42, 5);
    expectEq(sketch.estimate(42), 6u);

    sketch.add(42, 10);
    expectEq(sketch.estimate(42), 16u);

    // Other items should still be 0
    expectEq(sketch.estimate(0), 0u);
    expectEq(sketch.estimate(100), 0u);
  };

  "heavy hitter detection"_test = [] {
    // Smaller width to force some collisions
    CountMinSketch<256, 4> sketch;

    // Add one heavy hitter with count 1000
    uint64_t heavy_hitter = 12345;
    sketch.add(heavy_hitter, 1000);

    // Add many light items with count 1 each
    for (uint64_t i = 0; i < 100; ++i) {
      if (i != heavy_hitter) {
        sketch.add(i);
      }
    }

    // Heavy hitter should be clearly distinguishable
    uint32_t heavy_estimate = sketch.estimate(heavy_hitter);
    expectGE(heavy_estimate, 1000u);  // Never underestimates

    // Light items should have much smaller estimates
    // Due to collisions, might be > 1, but should be << 1000
    for (uint64_t i = 0; i < 100; ++i) {
      if (i != heavy_hitter) {
        uint32_t light_estimate = sketch.estimate(i);
        expectGE(light_estimate, 1u);  // Never underestimates
        expectLT(light_estimate, 100u);  // Should be small (some collision expected)
      }
    }
  };

  "merge combines sketches"_test = [] {
    CountMinSketch<1024, 4> sketch1;
    CountMinSketch<1024, 4> sketch2;

    sketch1.add(100, 5);
    sketch1.add(200, 10);

    sketch2.add(100, 3);
    sketch2.add(300, 7);

    sketch1.merge(sketch2);

    // Item in both: should have combined count
    expectGE(sketch1.estimate(100), 8u);  // 5 + 3 = 8

    // Item only in sketch1
    expectGE(sketch1.estimate(200), 10u);

    // Item only in sketch2
    expectGE(sketch1.estimate(300), 7u);

    // Total count should be sum
    expectEq(sketch1.totalCount(), 25ull);  // 5 + 10 + 3 + 7 = 25
  };

  "estimates never underestimate"_test = [] {
    CountMinSketch<512, 4> sketch;

    // Add known counts for various items
    struct {
      uint64_t item;
      uint32_t count;
    } items[] = {
        {1, 10},   {2, 20},   {3, 30},    {100, 100},
        {999, 50}, {1000, 1}, {9999, 200}
    };

    for (auto [item, count] : items) {
      sketch.add(item, count);
    }

    // Verify: estimate >= true count for all items
    for (auto [item, count] : items) {
      expectGE(sketch.estimate(item), count);
    }
  };

  "clear resets sketch"_test = [] {
    CountMinSketch<1024, 4> sketch;

    sketch.add(1, 100);
    sketch.add(2, 200);
    expectGE(sketch.estimate(1), 100u);
    expectEq(sketch.totalCount(), 300ull);

    sketch.clear();

    expectEq(sketch.estimate(1), 0u);
    expectEq(sketch.estimate(2), 0u);
    expectEq(sketch.totalCount(), 0ull);

    // Can add again after clear
    sketch.add(3, 50);
    expectGE(sketch.estimate(3), 50u);
    expectEq(sketch.totalCount(), 50ull);
  };

  "total count tracks all additions"_test = [] {
    CountMinSketch<1024, 4> sketch;

    sketch.add(1);       // +1
    sketch.add(2, 5);    // +5
    sketch.add(1, 10);   // +10
    sketch.add(3, 100);  // +100

    expectEq(sketch.totalCount(), 116ull);
  };

  "dimension accessors"_test = [] {
    CountMinSketch<2048, 5> sketch;
    expectEq(sketch.width(), 2048uz);
    expectEq(sketch.depth(), 5uz);

    CountMinSketch<> default_sketch;
    expectEq(default_sketch.width(), 65536uz);
    expectEq(default_sketch.depth(), 4uz);
  };

  return TestRegistry::result();
}
