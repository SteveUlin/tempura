#pragma once

#include <generator>
#include <ranges>

namespace tempura {

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
