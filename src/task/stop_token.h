#pragma once

#include <atomic>
#include <concepts>
#include <type_traits>

namespace tempura {

// Concept for stop tokens
template <typename T>
concept StopToken = requires(const T& token) {
  { token.stop_requested() } noexcept -> std::same_as<bool>;
  { token.stop_possible() } noexcept -> std::same_as<bool>;
};

// Stop token that can never be stopped (zero overhead)
class NeverStopToken {
 public:
  static constexpr auto stop_requested() noexcept -> bool { return false; }

  static constexpr auto stop_possible() noexcept -> bool { return false; }

  friend constexpr bool operator==(NeverStopToken, NeverStopToken) noexcept {
    return true;
  }
};

// Simple in-place stop source/token pair
// Uses single atomic bool for stop state (no callbacks yet)
class InplaceStopSource {
  std::atomic<bool> stop_requested_{false};

 public:
  class Token {
    InplaceStopSource* source_;

   public:
    constexpr Token() noexcept : source_(nullptr) {}
    explicit constexpr Token(InplaceStopSource* s) noexcept : source_(s) {}

    auto stop_requested() const noexcept -> bool {
      return source_ &&
             source_->stop_requested_.load(std::memory_order_acquire);
    }

    auto stop_possible() const noexcept -> bool { return source_ != nullptr; }

    friend bool operator==(const Token& a, const Token& b) noexcept {
      return a.source_ == b.source_;
    }
  };

  constexpr InplaceStopSource() noexcept = default;

  // Non-copyable, non-movable (owns stop state)
  InplaceStopSource(const InplaceStopSource&) = delete;
  auto operator=(const InplaceStopSource&) -> InplaceStopSource& = delete;
  InplaceStopSource(InplaceStopSource&&) = delete;
  auto operator=(InplaceStopSource&&) -> InplaceStopSource& = delete;

  auto get_token() noexcept -> Token { return Token{this}; }

  // Returns true if this call actually requested stop (first call)
  // Returns false if stop was already requested
  auto request_stop() noexcept -> bool {
    return !stop_requested_.exchange(true, std::memory_order_acq_rel);
  }

  auto stop_requested() const noexcept -> bool {
    return stop_requested_.load(std::memory_order_acquire);
  }
};

// Type alias for consistency with P2300
using InplaceStopToken = InplaceStopSource::Token;

static_assert(StopToken<NeverStopToken>);
static_assert(StopToken<InplaceStopToken>);

}  // namespace tempura
