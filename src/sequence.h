#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <ranges>
#include <type_traits>

namespace tempura {

// FnGenerator repeatedly calls a function and returning the results as a view.
//
// This class differs from std::generator as it uses regular functions instead
// of coroutines.
//
// All FnGenerators are infinite in length, users must implement their own
// stopping logic.
template <std::invocable Fn>
class FnGenerator : std::ranges::view_interface<FnGenerator<Fn>> {
 public:
  struct Sentinel;

  // Iterators are move only objects that take ownership of the generator.
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

    constexpr Iterator(Fn fn) : fn_{std::move(fn)}, value_{std::invoke(fn_)} {}

    constexpr auto operator*() const -> decltype(auto) { return value_; }

    constexpr auto operator++() -> Iterator& {
      value_ = std::invoke(fn_);
      return *this;
    }

    constexpr void operator++(int) { ++*this; }

    constexpr auto operator==(Sentinel /*unused*/) const -> bool {
      return false;
    }

    constexpr auto operator!=(Sentinel /*unused*/) const -> bool {
      return true;
    }

   private:
    Fn fn_;
    std::invoke_result_t<Fn> value_;
  };

  struct Sentinel {
    constexpr auto operator==(const Iterator& /*unused*/) const {
      return false;
    }

    template <typename IteratorT, typename SentinelT>
    constexpr auto operator!=(const Iterator& /*unused*/) const {
      return true;
    }
  };

  constexpr FnGenerator(Fn fn) : fn_{std::move(fn)} {}

  // Calling .begin() multiple times is undefined behavior;
  constexpr auto begin() -> Iterator { return Iterator{std::move(fn_)}; }

  constexpr auto end() const -> Sentinel { return {}; }

 private:
  Fn fn_;
};

template <std::invocable Fn>
FnGenerator(Fn) -> FnGenerator<std::remove_cvref_t<Fn>>;

struct TakeFirst {};

constexpr auto operator|(std::ranges::range auto&& rng, TakeFirst /*unused*/) {
  return *std::ranges::begin(rng);
}

template <typename FloatT>
struct Converges {
  FloatT epsilon;
};

template <typename FloatT>
constexpr auto operator|(std::ranges::range auto&& rng,
                         Converges<FloatT> converges) {
  auto itr = std::ranges::begin(rng);
  auto prev = *itr;
  while (true) {
    ++itr;
    auto next = *itr;
    if (std::abs(next - prev) < converges.epsilon * std::abs(next)) {
      return next;
    }
    prev = next;
  }
}

// Evaluates a continued fraction at each partial denominator.
// x = a1 / (b1 + a2 / (b2 + a3 / (b3 + ...)))
//
// Implements the Modified Lentz Algorithm for evaluating continued fractions.
// See Numerical Recipes Third Edition, Section 5.2.
template <std::ranges::range Range, typename FloatT = double>
class Continuantes : public std::ranges::view_interface<Continuantes<Range>> {
 public:
  class Iterator;
  struct Sentinel {
    constexpr auto operator==(const Iterator& iter) const -> bool {
      return iter == *this;
    }
    constexpr auto operator!=(const Iterator& iter) const -> bool {
      return iter != *this;
    }
  };

  class Iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using value_type = FloatT;

    constexpr Iterator(std::ranges::iterator_t<Range> iter,
                       Continuantes* parent)
        : iter_{iter}, parent_{parent} {
      if (iter_ != std::ranges::end(parent_->range_)) {
        update();
      }
    }

    constexpr auto operator*() const -> const value_type& { return f; }

    constexpr auto operator++() -> Iterator& {
      ++iter_;
      if (iter_ == std::ranges::end(parent_->range_)) {
        return *this;
      }
      update();
      return *this;
    }

    constexpr void operator++(int) { ++(*this); }

    constexpr auto operator==(Sentinel /*unused*/) const -> bool {
      return iter_ == std::ranges::end(parent_->range_);
    }

    constexpr auto operator!=(Sentinel /*unused*/) const -> bool {
      return iter_ != std::ranges::end(parent_->range_);
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
    static constexpr value_type kTiny = 10e-30;

    std::ranges::iterator_t<Range> iter_;
    value_type f = kTiny;
    value_type c = kTiny;
    value_type d = 0.0;

    Continuantes* parent_;
  };

  // You can still move the view instead of using the constructor below
  constexpr Continuantes(Continuantes&&) = default;
  constexpr auto operator=(Continuantes&&) -> Continuantes& = default;

  template <std::ranges::range R>
  constexpr Continuantes(R&& range) : range_{std::forward<R&&>(range)} {}

  constexpr auto begin() -> Iterator {
    return Iterator{std::ranges::begin(range_), this};
  }

  constexpr auto end() const -> Sentinel { return {}; }

 private:
  Range range_;
};

template <std::ranges::range Range>
Continuantes(Continuantes<Range>&&) -> Continuantes<Range>;

template <std::ranges::range Range>
Continuantes(Range&&) -> Continuantes<std::ranges::views::all_t<Range>>;

struct ContinuantesFn : std::ranges::range_adaptor_closure<ContinuantesFn> {
  template <std::ranges::input_range Range>
  constexpr auto operator()(Range&& range) const {
    return Continuantes(std::forward<Range>(range));
  }
};

constexpr auto continuants() { return ContinuantesFn{}; }

// Returns the rolling sum of the input range.
// 1, 2, 3, 4, 5 ->
// 1, 3, 6, 10, 15
template <typename Range, typename BinaryOp>
class InclusiveScanView
    : public std::ranges::view_interface<InclusiveScanView<Range, BinaryOp>> {
 public:
  struct Sentinel;

  template <typename IteratorT, typename SentinelT>
  class Iterator {
   public:
    // Required for std::ranges::iterator_traits
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_cvref_t<std::ranges::range_value_t<Range>>;

    constexpr Iterator(IteratorT begin, SentinelT end,
                       const InclusiveScanView* parent)
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

    constexpr auto operator!=(const Sentinel /*unused*/) const -> bool {
      return current_ != end_;
    }

   private:
    std::remove_cvref_t<std::ranges::range_value_t<Range>> acc_ = {};
    IteratorT current_;
    [[no_unique_address]] SentinelT end_;
    const InclusiveScanView* parent_;
  };

  struct Sentinel {
    template <typename IteratorT, typename SentinelT>
    constexpr auto operator==(const Iterator<IteratorT, SentinelT>& itr) const {
      return itr == *this;
    }

    template <typename IteratorT, typename SentinelT>
    constexpr auto operator!=(const Iterator<IteratorT, SentinelT>& itr) const {
      return itr != *this;
    }
  };

  // You can still move the view instead of using the constructor below
  constexpr InclusiveScanView(InclusiveScanView&&) = default;

  template <std::ranges::viewable_range Arg>
  constexpr explicit InclusiveScanView(Arg&& range, BinaryOp op = {})
      : range_(std::forward<Arg>(range)), op_(std::move(op)) {}

  constexpr auto begin() const {
    return Iterator(std::ranges::begin(range_), std::ranges::end(range_), this);
  }

  constexpr auto cbegin() const {
    return Iterator(std::ranges::cbegin(range_), std::ranges::cend(range_),
                    this);
  }

  constexpr auto end() const -> Sentinel { return {}; }
  constexpr auto cend() const -> Sentinel { return {}; }

 private:
  Range range_ = std::ranges::empty;
  BinaryOp op_ = {};
};

template <std::ranges::viewable_range Rng, typename BinaryOp = std::plus<>>
InclusiveScanView(InclusiveScanView<Rng, BinaryOp>&&)
    -> InclusiveScanView<Rng, BinaryOp>;

template <std::ranges::viewable_range Rng, typename BinaryOp = std::plus<>>
InclusiveScanView(Rng&&, BinaryOp = {})
    -> InclusiveScanView<std::ranges::views::all_t<Rng>, BinaryOp>;

template <typename BinaryOp>
struct InclusiveScanFn
    : std::ranges::range_adaptor_closure<InclusiveScanFn<BinaryOp>> {
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
constexpr bool std::ranges::enable_borrowed_range<tempura::FnGenerator<Fn>> =
    true;

template <std::ranges::range Range, typename FloatT>
constexpr bool
    std::ranges::enable_borrowed_range<tempura::Continuantes<Range, FloatT>> =
        std::ranges::enable_borrowed_range<Range>;
