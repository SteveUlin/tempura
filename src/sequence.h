#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <type_traits>

#include "comparison.h"

namespace tempura {

// Infinite lazy view that re-invokes a nullary function for each element — a
// constexpr, coroutine-free std::generator. For a closed-form term k prefer
// std::views::iota | std::views::transform; Generate earns its keep when each
// term is a cheap update of state (a recurrence). Stopping is the consumer's job.
template <std::invocable Fn>
class Generate : public std::ranges::view_interface<Generate<Fn>> {
 public:
  struct Sentinel {};

  // Move-only: the iterator takes ownership of the generating function.
  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::invoke_result_t<Fn>;

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    constexpr Iterator(Iterator&& other) noexcept
        : fn_{std::move(other.fn_)}, value_{std::move(other.value_)} {}
    constexpr auto operator=(Iterator&& other) noexcept -> Iterator& {
      fn_ = std::move(other.fn_);
      value_ = std::move(other.value_);
      return *this;
    }

    constexpr explicit Iterator(Fn fn) : fn_{std::move(fn)}, value_{std::invoke(fn_)} {}

    constexpr auto operator*() const -> decltype(auto) { return value_; }
    constexpr auto operator++() -> Iterator& {
      value_ = std::invoke(fn_);
      return *this;
    }
    constexpr void operator++(int) { ++*this; }
    constexpr auto operator==(Sentinel) const -> bool { return false; }

   private:
    Fn fn_;
    std::invoke_result_t<Fn> value_;
  };

  constexpr Generate(Generate&& other) noexcept : fn_{std::move(other.fn_)} {}
  constexpr auto operator=(Generate&& other) noexcept -> Generate& {
    fn_ = std::move(other.fn_);
    return *this;
  }
  constexpr explicit Generate(Fn fn) : fn_{std::move(fn)} {}

  // Calling begin() moves the function out, so it is single-pass.
  constexpr auto begin() -> Iterator { return Iterator{std::move(fn_)}; }
  constexpr auto end() const -> Sentinel { return {}; }

 private:
  Fn fn_;
};

template <std::invocable Fn>
Generate(Fn) -> Generate<std::remove_cvref_t<Fn>>;

struct TakeFirst {};
constexpr auto operator|(std::ranges::range auto&& rng, TakeFirst /*unused*/) {
  return *std::ranges::begin(rng);
}

// Pulls successive iterates until two consecutive ones are isClose, returning the
// converged value. An infinite generator that never settles must fail loudly, not
// hang — so iterations are capped and exhausting the cap is a hard error.
struct Converges {
  Tolerance tol;
  std::size_t max_iters = 1000;
};
constexpr auto operator|(std::ranges::range auto&& rng, Converges c) {
  auto it = std::ranges::begin(rng);
  const auto last = std::ranges::end(rng);
  assert(it != last);
  auto prev = *it;
  for (std::size_t k = 1; k < c.max_iters; ++k) {
    ++it;
    if (it == last) {
      return prev;  // finite range exhausted — its last value is the estimate
    }
    auto next = *it;
    if (isClose(next, prev, c.tol)) {
      return next;
    }
    prev = next;
  }
  assert(false && "Converges: did not converge within max_iters");
  return prev;
}

// Neumaier-compensated accumulator: keeps the low-order bits a naive running sum
// drops, so summing N terms loses O(ε) accuracy rather than O(Nε).
template <std::floating_point V>
struct CompensatedSum {
  V sum{};
  V correction{};
  constexpr void add(V term) {
    const V t = sum + term;
    // The smaller operand is the one whose low bits are lost in `t`.
    correction += std::abs(sum) >= std::abs(term) ? (sum - t) + term : (term - t) + sum;
    sum = t;
  }
  constexpr auto value() const -> V { return sum + correction; }
};

// Compensated reduction over a finite range.
template <std::ranges::range R>
constexpr auto compensatedSum(R&& terms) {
  CompensatedSum<std::remove_cvref_t<std::ranges::range_value_t<R>>> acc;
  for (auto&& term : terms) {
    acc.add(term);
  }
  return acc.value();
}

// Accumulates series terms with compensation, stopping once two consecutive
// partial sums are isClose. Capped and loud on non-convergence.
struct SumUntilConverged {
  Tolerance tol;
  std::size_t max_iters = 1000;
};
template <std::ranges::range R>
constexpr auto operator|(R&& terms, SumUntilConverged s) {
  using V = std::remove_cvref_t<std::ranges::range_value_t<R>>;
  CompensatedSum<V> acc;
  V prev{};
  auto it = std::ranges::begin(terms);
  const auto last = std::ranges::end(terms);
  for (std::size_t k = 0; k < s.max_iters && it != last; ++it, ++k) {
    acc.add(static_cast<V>(*it));
    const V current = acc.value();
    if (k > 0 && isClose(current, prev, s.tol)) {
      return current;
    }
    prev = current;
  }
  if (it == last) {
    return acc.value();  // ran out of terms — best available partial sum
  }
  assert(false && "SumUntilConverged: did not converge within max_iters");
  return acc.value();
}

// Evaluates a continued fraction b₀ + a₁/(b₁ + a₂/(b₂ + …)) via the Modified
// Lentz algorithm (Numerical Recipes 3rd ed., §5.2), yielding successive
// convergents as a lazy view over a range of (aₖ, bₖ) pairs.
template <std::ranges::range Range, typename FloatT = double>
class ContinuedFraction
    : public std::ranges::view_interface<ContinuedFraction<Range, FloatT>> {
 public:
  struct Sentinel;

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = FloatT;

    constexpr Iterator(std::ranges::iterator_t<Range> iter, ContinuedFraction* parent)
        : iter_{std::move(iter)}, parent_{parent} {
      if (iter_ != std::ranges::end(parent_->range_)) {
        update();
      }
    }

    constexpr auto operator*() const -> const value_type& { return f; }
    constexpr auto operator++() -> Iterator& {
      ++iter_;
      if (iter_ != std::ranges::end(parent_->range_)) {
        update();
      }
      return *this;
    }
    constexpr void operator++(int) { ++(*this); }
    constexpr auto operator==(Sentinel /*unused*/) const -> bool {
      return iter_ == std::ranges::end(parent_->range_);
    }

   private:
    constexpr void update() {
      const auto& [a, b] = *iter_;
      d = b + a * d;
      if (d == 0.0) {
        d = kTiny;
      }
      d = 1.0 / d;
      c = b + a / c;
      if (c == 0.0) {
        c = kTiny;
      }
      f = c * d * f;
    }
    static constexpr value_type kTiny = 1e-30;

    std::ranges::iterator_t<Range> iter_;
    value_type f = kTiny;
    value_type c = kTiny;
    value_type d = 0.0;
    ContinuedFraction* parent_;
  };

  struct Sentinel {};

  template <std::ranges::range R>
  constexpr explicit ContinuedFraction(R&& range) : range_{std::forward<R>(range)} {}

  constexpr auto begin() -> Iterator { return Iterator{std::ranges::begin(range_), this}; }
  constexpr auto end() const -> Sentinel { return {}; }

 private:
  Range range_;
};

template <std::ranges::range Range>
ContinuedFraction(Range&&) -> ContinuedFraction<std::ranges::views::all_t<Range>>;

struct ContinuedFractionFn : std::ranges::range_adaptor_closure<ContinuedFractionFn> {
  template <std::ranges::input_range Range>
  constexpr auto operator()(Range&& range) const {
    return ContinuedFraction(std::forward<Range>(range));
  }
};

constexpr auto continuedFraction() { return ContinuedFractionFn{}; }

// Lazy prefix scan: 1, 2, 3, 4, 5 → 1, 3, 6, 10, 15 (default op std::plus).
template <typename Range, typename BinaryOp>
class InclusiveScanView
    : public std::ranges::view_interface<InclusiveScanView<Range, BinaryOp>> {
 public:
  struct Sentinel;

  template <typename IteratorT, typename SentinelT>
  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_cvref_t<std::ranges::range_value_t<Range>>;

    constexpr Iterator(IteratorT begin, SentinelT end, const InclusiveScanView* parent)
        : current_{begin}, end_{end}, parent_{parent} {
      if (current_ != end) {
        acc_ = *current_;
      }
    }

    constexpr auto operator*() const -> decltype(auto) { return acc_; }
    constexpr auto operator++() -> Iterator& {
      ++current_;
      if (current_ != end_) {
        acc_ = parent_->op_(acc_, *current_);
      }
      return *this;
    }
    constexpr auto operator++(int) -> Iterator {
      auto copy = *this;
      ++(*this);
      return copy;
    }
    constexpr auto operator==(const Sentinel /*unused*/) const -> bool {
      return current_ == end_;
    }

   private:
    value_type acc_ = {};
    IteratorT current_;
    [[no_unique_address]] SentinelT end_;
    const InclusiveScanView* parent_;
  };

  struct Sentinel {};

  constexpr InclusiveScanView(InclusiveScanView&&) = default;
  template <std::ranges::viewable_range Arg>
  constexpr explicit InclusiveScanView(Arg&& range, BinaryOp op = {})
      : range_(std::forward<Arg>(range)), op_(std::move(op)) {}

  constexpr auto begin() const {
    return Iterator(std::ranges::begin(range_), std::ranges::end(range_), this);
  }
  constexpr auto cbegin() const {
    return Iterator(std::ranges::cbegin(range_), std::ranges::cend(range_), this);
  }
  constexpr auto end() const -> Sentinel { return {}; }
  constexpr auto cend() const -> Sentinel { return {}; }

 private:
  Range range_;
  BinaryOp op_ = {};
};

template <std::ranges::viewable_range Rng, typename BinaryOp = std::plus<>>
InclusiveScanView(InclusiveScanView<Rng, BinaryOp>&&) -> InclusiveScanView<Rng, BinaryOp>;

template <std::ranges::viewable_range Rng, typename BinaryOp = std::plus<>>
InclusiveScanView(Rng&&, BinaryOp = {})
    -> InclusiveScanView<std::ranges::views::all_t<Rng>, BinaryOp>;

template <typename BinaryOp>
struct InclusiveScanFn : std::ranges::range_adaptor_closure<InclusiveScanFn<BinaryOp>> {
  template <std::ranges::input_range Range>
  constexpr auto operator()(Range&& range) const {
    return InclusiveScanView{std::forward<Range>(range), op};
  }
  BinaryOp op;
};

template <typename BinaryOp = std::plus<>>
constexpr auto inclusiveScan(BinaryOp op = {}) {
  return InclusiveScanFn{.op = std::move(op)};
}

}  // namespace tempura

template <std::invocable Fn>
constexpr bool std::ranges::enable_borrowed_range<tempura::Generate<Fn>> = true;

template <std::ranges::range Range, typename FloatT>
constexpr bool std::ranges::enable_borrowed_range<tempura::ContinuedFraction<Range, FloatT>> =
    std::ranges::enable_borrowed_range<Range>;
