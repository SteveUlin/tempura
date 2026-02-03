#include "prob/discrete.h"

#include <random>

#include "unit.h"

using namespace tempura;
using namespace tempura::prob;

// Test enums
enum class Coin { Heads, Tails, kCount };
enum class Color { Red, Green, Blue, kCount };
enum class Direction { North, East, South, West, kCount };

auto main() -> int {
  "Discrete basic construction"_test = [] {
    Discrete<Coin> fair_coin(0.5, 0.5);

    expectNear(fair_coin.prob(Coin::Heads), 0.5, 1e-10);
    expectNear(fair_coin.prob(Coin::Tails), 0.5, 1e-10);
  };

  "Discrete from array"_test = [] {
    std::array<double, 3> probs = {0.2, 0.3, 0.5};
    Discrete<Color> dist{probs};

    expectNear(dist.prob(Color::Red), 0.2, 1e-10);
    expectNear(dist.prob(Color::Green), 0.3, 1e-10);
    expectNear(dist.prob(Color::Blue), 0.5, 1e-10);
  };

  "Discrete normalizes weights"_test = [] {
    Discrete<Color> dist(2.0, 3.0, 5.0);  // Sum = 10

    expectNear(dist.prob(Color::Red), 0.2, 1e-10);
    expectNear(dist.prob(Color::Green), 0.3, 1e-10);
    expectNear(dist.prob(Color::Blue), 0.5, 1e-10);
  };

  "Discrete logProb"_test = [] {
    Discrete<Coin> biased(0.7, 0.3);

    expectNear(biased.logProb(Coin::Heads), std::log(0.7), 1e-10);
    expectNear(biased.logProb(Coin::Tails), std::log(0.3), 1e-10);
  };

  "Discrete mode"_test = [] {
    Discrete<Color> dist(0.1, 0.6, 0.3);

    expectEq(dist.mode(), Color::Green);
  };

  "Discrete sampling"_test = [] {
    Discrete<Coin> biased(0.8, 0.2);
    std::mt19937_64 rng{42};

    std::size_t heads = 0;
    constexpr std::size_t n = 10000;
    for (std::size_t i = 0; i < n; ++i) {
      if (biased.sample(rng) == Coin::Heads) ++heads;
    }

    double empirical_p = static_cast<double>(heads) / n;
    // SE = sqrt(0.8 * 0.2 / 10000) ~ 0.004, 0.05 is ~12 SEs
    expectNear(empirical_p, 0.8, 0.05);
  };

  "Discrete entropy"_test = [] {
    // Uniform has maximum entropy: log(K)
    auto uniform = uniformDiscrete<Color>();
    expectNear(uniform.entropy(), std::log(3.0), 1e-10);

    // Degenerate has zero entropy
    Discrete<Color> degenerate(1.0, 0.0, 0.0);
    expectNear(degenerate.entropy(), 0.0, 1e-10);
  };

  "Binary distribution"_test = [] {
    Binary<Coin> fair(0.5);

    expectNear(fair.prob(Coin::Heads), 0.5, 1e-10);
    expectNear(fair.prob(Coin::Tails), 0.5, 1e-10);
    expectNear(fair.probFirst(), 0.5, 1e-10);
    expectNear(fair.probSecond(), 0.5, 1e-10);

    Binary<Coin> biased(0.8);  // 80% tails
    expectNear(biased.prob(Coin::Heads), 0.2, 1e-10);
    expectNear(biased.prob(Coin::Tails), 0.8, 1e-10);
  };

  "Binary mode"_test = [] {
    Binary<Coin> biased(0.7);  // 70% tails
    expectEq(biased.mode(), Coin::Tails);

    Binary<Coin> other(0.3);  // 30% tails
    expectEq(other.mode(), Coin::Heads);
  };

  "Binary sampling"_test = [] {
    Binary<Coin> biased(0.7);  // 70% tails
    std::mt19937_64 rng{123};

    std::size_t tails = 0;
    constexpr std::size_t n = 10000;
    for (std::size_t i = 0; i < n; ++i) {
      if (biased.sample(rng) == Coin::Tails) ++tails;
    }

    double empirical_p = static_cast<double>(tails) / n;
    expectNear(empirical_p, 0.7, 0.05);
  };

  "uniformDiscrete"_test = [] {
    auto uniform = uniformDiscrete<Direction>();

    for (Direction d : enumValues<Direction>()) {
      expectNear(uniform.prob(d), 0.25, 1e-10);
    }
  };

  "fromWeights"_test = [] {
    auto dist = fromWeights<Color>({
        {Color::Red, 1.0},
        {Color::Green, 2.0},
        {Color::Blue, 1.0},
    });

    expectNear(dist.prob(Color::Red), 0.25, 1e-10);
    expectNear(dist.prob(Color::Green), 0.5, 1e-10);
    expectNear(dist.prob(Color::Blue), 0.25, 1e-10);
  };

  "EnumRange iteration"_test = [] {
    std::size_t count = 0;
    for ([[maybe_unused]] Color c : enumValues<Color>()) {
      ++count;
    }
    expectEq(count, 3UL);
  };

  "four-valued enum"_test = [] {
    Discrete<Direction> dist(0.1, 0.2, 0.3, 0.4);

    expectNear(dist.prob(Direction::North), 0.1, 1e-10);
    expectNear(dist.prob(Direction::East), 0.2, 1e-10);
    expectNear(dist.prob(Direction::South), 0.3, 1e-10);
    expectNear(dist.prob(Direction::West), 0.4, 1e-10);

    expectEq(dist.mode(), Direction::West);
  };

  return TestRegistry::result();
}
