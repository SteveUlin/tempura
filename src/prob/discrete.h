#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <numeric>
#include <random>
#include <type_traits>

namespace tempura::prob {

// ============================================================================
// Enum traits for discrete distributions
// ============================================================================
//
// To use Discrete<MyEnum>, either:
// 1. Define a contiguous enum starting at 0 with a kCount sentinel:
//      enum class Color { Red, Green, Blue, kCount };
//
// 2. Specialize EnumTraits<MyEnum>:
//      template<> struct EnumTraits<MyEnum> {
//        static constexpr std::size_t count = 3;
//        static constexpr std::array<MyEnum, 3> values = {A, B, C};
//      };
//
// ============================================================================

// Primary template - uses kCount sentinel pattern
template <typename E>
struct EnumTraits {
  static_assert(std::is_enum_v<E>, "EnumTraits requires an enum type");

  // Attempt to use kCount sentinel (common pattern)
  static constexpr std::size_t count = static_cast<std::size_t>(E::kCount);

  // Convert enum to index
  static constexpr auto toIndex(E e) -> std::size_t {
    return static_cast<std::size_t>(static_cast<std::underlying_type_t<E>>(e));
  }

  // Convert index to enum
  static constexpr auto fromIndex(std::size_t i) -> E {
    assert(i < count);
    return static_cast<E>(i);
  }
};

// Concept for valid enum types
template <typename E>
concept DiscreteEnum = std::is_enum_v<E> && requires {
  { EnumTraits<E>::count } -> std::convertible_to<std::size_t>;
  { EnumTraits<E>::toIndex(std::declval<E>()) } -> std::convertible_to<std::size_t>;
  { EnumTraits<E>::fromIndex(0) } -> std::same_as<E>;
};

// ============================================================================
// Discrete<Enum> - Type-safe categorical distribution over enum values
// ============================================================================
//
// Usage:
//   enum class Coin { Heads, Tails, kCount };
//   Discrete<Coin> fair_coin({0.5, 0.5});
//
//   Coin result = fair_coin.sample(rng);
//   double p = fair_coin.prob(Coin::Heads);
//
// ============================================================================

template <DiscreteEnum E>
class Discrete {
 public:
  static constexpr std::size_t K = EnumTraits<E>::count;
  using enum_type = E;
  using Traits = EnumTraits<E>;

  // Construct from array of probabilities
  constexpr explicit Discrete(std::array<double, K> probs) : probs_{probs} {
    normalizeIfNeeded();
  }

  // Construct from initializer list
  constexpr Discrete(std::initializer_list<double> probs) {
    assert(probs.size() == K);
    std::copy(probs.begin(), probs.end(), probs_.begin());
    normalizeIfNeeded();
  }

  // Variadic constructor
  template <typename... Ts>
    requires(sizeof...(Ts) == K && (std::convertible_to<Ts, double> && ...))
  constexpr Discrete(Ts... ps) : probs_{static_cast<double>(ps)...} {
    normalizeIfNeeded();
  }

  // P(X = e)
  constexpr auto prob(E e) const -> double {
    return probs_[Traits::toIndex(e)];
  }

  // log P(X = e)
  constexpr auto logProb(E e) const -> double {
    return std::log(probs_[Traits::toIndex(e)]);
  }

  // Unnormalized log-prob (same as logProb for discrete)
  constexpr auto unnormalizedLogProb(E e) const -> double {
    return logProb(e);
  }

  // Sample an enum value
  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> E {
    constexpr double scale = 1.0 / static_cast<double>(RNG::max() - RNG::min());
    double u = static_cast<double>(rng() - RNG::min()) * scale;

    double cumsum = 0.0;
    for (std::size_t k = 0; k < K - 1; ++k) {
      cumsum += probs_[k];
      if (u < cumsum) return Traits::fromIndex(k);
    }
    return Traits::fromIndex(K - 1);
  }

  // Mode: most likely value
  auto mode() const -> E {
    std::size_t best = 0;
    for (std::size_t k = 1; k < K; ++k) {
      if (probs_[k] > probs_[best]) best = k;
    }
    return Traits::fromIndex(best);
  }

  // Get probability at index (for iteration)
  constexpr auto probAt(std::size_t k) const -> double {
    assert(k < K);
    return probs_[k];
  }

  // Get value at index
  static constexpr auto valueAt(std::size_t k) -> E {
    return Traits::fromIndex(k);
  }

  auto probs() const -> const std::array<double, K>& { return probs_; }
  static constexpr auto numValues() -> std::size_t { return K; }

  // Entropy: H = -sum p_k log p_k
  auto entropy() const -> double {
    double h = 0.0;
    for (std::size_t k = 0; k < K; ++k) {
      if (probs_[k] > 0) {
        h -= probs_[k] * std::log(probs_[k]);
      }
    }
    return h;
  }

 private:
  std::array<double, K> probs_;

  constexpr void normalizeIfNeeded() {
    double sum = 0.0;
    for (auto p : probs_) sum += p;
    if (std::abs(sum - 1.0) > 1e-10) {
      for (auto& p : probs_) p /= sum;
    }
  }
};

// ============================================================================
// Uniform discrete distribution
// ============================================================================

template <DiscreteEnum E>
auto uniformDiscrete() -> Discrete<E> {
  constexpr std::size_t K = EnumTraits<E>::count;
  std::array<double, K> probs;
  probs.fill(1.0 / static_cast<double>(K));
  return Discrete<E>{probs};
}

// ============================================================================
// Binary<E0, E1> - Specialized Bernoulli with named outcomes
// ============================================================================
//
// For binary enums, provides a cleaner interface than Discrete<E>.
//
// Usage:
//   enum class Result { Fail, Pass, kCount };
//   Binary<Result> test(0.8);  // 80% pass rate
//   Result r = test.sample(rng);
//
// ============================================================================

template <DiscreteEnum E>
  requires(EnumTraits<E>::count == 2)
class Binary {
 public:
  using enum_type = E;
  using Traits = EnumTraits<E>;

  // p = P(X = second value), so P(first) = 1-p
  constexpr explicit Binary(double p) : p_{p} {
    assert(p >= 0.0 && p <= 1.0);
  }

  constexpr auto prob(E e) const -> double {
    return (Traits::toIndex(e) == 1) ? p_ : (1.0 - p_);
  }

  constexpr auto logProb(E e) const -> double {
    return std::log(prob(e));
  }

  template <std::uniform_random_bit_generator RNG>
  auto sample(RNG& rng) const -> E {
    constexpr double scale = 1.0 / static_cast<double>(RNG::max() - RNG::min());
    double u = static_cast<double>(rng() - RNG::min()) * scale;
    return Traits::fromIndex(u < (1.0 - p_) ? 0 : 1);
  }

  auto mode() const -> E {
    return Traits::fromIndex(p_ >= 0.5 ? 1 : 0);
  }

  constexpr auto p() const -> double { return p_; }

  // Named accessors for the two values
  static constexpr auto first() -> E { return Traits::fromIndex(0); }
  static constexpr auto second() -> E { return Traits::fromIndex(1); }

  constexpr auto probFirst() const -> double { return 1.0 - p_; }
  constexpr auto probSecond() const -> double { return p_; }

 private:
  double p_;  // Probability of second value
};

// ============================================================================
// Helpers for common enum patterns
// ============================================================================

// Create distribution from enum-keyed map-like structure
// Usage: fromWeights<Color>({{Color::Red, 2.0}, {Color::Green, 1.0}, {Color::Blue, 1.0}})
template <DiscreteEnum E>
auto fromWeights(std::initializer_list<std::pair<E, double>> weights) -> Discrete<E> {
  constexpr std::size_t K = EnumTraits<E>::count;
  std::array<double, K> probs{};
  for (const auto& [e, w] : weights) {
    probs[EnumTraits<E>::toIndex(e)] = w;
  }
  return Discrete<E>{probs};
}

// ============================================================================
// EnumSet - Helper for iterating over enum values
// ============================================================================

template <DiscreteEnum E>
struct EnumRange {
  struct Iterator {
    std::size_t index;

    constexpr auto operator*() const -> E {
      return EnumTraits<E>::fromIndex(index);
    }
    constexpr auto operator++() -> Iterator& {
      ++index;
      return *this;
    }
    constexpr auto operator!=(const Iterator& other) const -> bool {
      return index != other.index;
    }
  };

  static constexpr auto begin() -> Iterator { return {0}; }
  static constexpr auto end() -> Iterator { return {EnumTraits<E>::count}; }
};

// Usage: for (Color c : EnumRange<Color>{}) { ... }
template <DiscreteEnum E>
constexpr auto enumValues() -> EnumRange<E> {
  return {};
}

}  // namespace tempura::prob
