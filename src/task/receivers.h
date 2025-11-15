// Receiver implementations for the sender/receiver model

#pragma once

#include <latch>
#include <optional>
#include <print>
#include <system_error>
#include <tuple>
#include <utility>

#include "concepts.h"

namespace tempura {

// ============================================================================
// Completion Channels
// ============================================================================

// Print an argument when a value is received.
//
// This is useful for debugging purposes.
template <typename T>
class PrintReceiver {
 public:
  void setValue(const T& value) noexcept {
    std::println("Received value: {}", value);
  }

  void setError(std::error_code ec) noexcept {
    std::println("Error occurred: {}", ec.message());
  }

  void setStopped() noexcept { std::println("Operation was stopped."); }
};
static_assert(ReceiverOf<PrintReceiver<int>, int>);

// A simple receiver that stores values in an optional tuple.
//
// If a stop or error is received, the optional is reset.
template <typename... Args>
class ValueReceiver {
 public:
  explicit ValueReceiver(std::optional<std::tuple<Args...>>& opt)
      : opt_(&opt) {}

  ValueReceiver(const ValueReceiver&) = delete;
  auto operator=(const ValueReceiver&) -> ValueReceiver& = delete;

  ValueReceiver(ValueReceiver&&) = default;
  auto operator=(ValueReceiver&&) -> ValueReceiver& = default;

  void setValue(Args&&... args) noexcept {
    opt_->emplace(std::forward<Args>(args)...);
  }

  void setError(std::error_code ec) noexcept { opt_->reset(); }

  void setStopped() noexcept { opt_->reset(); }

 private:
  // Pointer (not reference) allows safe moves while preserving external storage
  std::optional<std::tuple<Args...>>* opt_;
};
static_assert(ReceiverOf<ValueReceiver<int>, int>);

// Blocking receiver that signals completion via latch
template <typename... Args>
class BlockingReceiver {
 public:
  BlockingReceiver(std::optional<std::tuple<Args...>>& opt, std::latch& latch)
      : opt_(&opt), latch_(&latch) {}

  BlockingReceiver(const BlockingReceiver&) = delete;
  auto operator=(const BlockingReceiver&) -> BlockingReceiver& = delete;

  BlockingReceiver(BlockingReceiver&&) = default;
  auto operator=(BlockingReceiver&&) -> BlockingReceiver& = default;

  void setValue(Args&&... args) noexcept {
    opt_->emplace(std::forward<Args>(args)...);
    latch_->count_down();
  }

  void setError(std::error_code ec) noexcept {
    opt_->reset();
    latch_->count_down();
  }

  void setStopped() noexcept {
    opt_->reset();
    latch_->count_down();
  }

 private:
  std::optional<std::tuple<Args...>>* opt_;
  std::latch* latch_;
};

static_assert(ReceiverOf<BlockingReceiver<int>, int>);

}  // namespace tempura
