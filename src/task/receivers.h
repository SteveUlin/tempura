// Receiver implementations for the sender/receiver model

#pragma once

#include <latch>
#include <optional>
#include <print>
#include <system_error>
#include <tuple>
#include <utility>

#include "concepts.h"
#include "env.h"

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

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    // For backwards compatibility, print std::error_code if that's the only arg
    if constexpr (sizeof...(ErrorArgs) == 1 &&
                  (std::same_as<std::decay_t<ErrorArgs>, std::error_code> &&
                   ...)) {
      std::println("Error occurred: {}", std::forward<ErrorArgs>(args).message()...);
    } else {
      std::println("Error occurred with {} arguments", sizeof...(ErrorArgs));
    }
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

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    opt_->reset();
  }

  void setStopped() noexcept { opt_->reset(); }

 private:
  // Pointer (not reference) allows safe moves while preserving external storage
  std::optional<std::tuple<Args...>>* opt_;
};
static_assert(ReceiverOf<ValueReceiver<int>, int>);

// Blocking receiver that signals completion via latch
//
// Provides InlineScheduler via environment since syncWait executes on the
// calling thread. This allows child operations to query the scheduler if needed.
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

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    opt_->reset();
    latch_->count_down();
  }

  void setStopped() noexcept {
    opt_->reset();
    latch_->count_down();
  }

  // Provide environment with InlineScheduler for child operations
  [[nodiscard]] constexpr auto get_env() const noexcept {
    return EnvWithScheduler{InlineScheduler{}};
  }

 private:
  std::optional<std::tuple<Args...>>* opt_;
  std::latch* latch_;
};

static_assert(ReceiverOf<BlockingReceiver<int>, int>);

}  // namespace tempura
