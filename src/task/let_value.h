// LetValueSender - chains asynchronous operations by returning new senders

#pragma once

#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

#include "concepts.h"
#include "meta/manual_lifetime.h"

namespace tempura {

// ============================================================================
// letValue - chains asynchronous operations by returning new senders
// ============================================================================

template <typename S, typename F, typename R>
class LetValueOperationState;

template <typename F, typename R, typename OuterOp>
class LetValueReceiver {
 public:
  LetValueReceiver(OuterOp* outer_op) : outer_op_(outer_op) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    outer_op_->transitionToInner(std::forward<Args>(args)...);
  }

  void setError(std::error_code ec) noexcept {
    outer_op_->forwardError(ec);
  }

  void setStopped() noexcept { outer_op_->forwardStopped(); }

 private:
  OuterOp* outer_op_;
};

template <typename S, typename F, typename R>
class LetValueOperationState {
 public:
  using InnerReceiver = LetValueReceiver<F, R, LetValueOperationState>;
  using OuterOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  // Compute inner operation type at compile time!
  // InnerSender = return type of F when called with S's value types
  using InnerSender = decltype(std::apply(
      std::declval<F&>(), std::declval<typename S::ValueTypes>()));
  // InnerOpState = connecting that sender to our receiver (moved)
  using InnerOpState = decltype(std::declval<InnerSender&&>().connect(std::declval<R&&>()));

  enum class State : unsigned char {
    kOuter,  // Outer operation is active
    kInner,  // Inner operation is active
    kEmpty   // Transitioning
  };

  LetValueOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        state_(State::kOuter) {
    // Construct outer operation in union storage using factory
    storage_.outer_.constructWith([&] {
      return std::move(sender).connect(InnerReceiver{this});
    });
  }

  ~LetValueOperationState() {
    if (state_ == State::kOuter) {
      storage_.outer_.destruct();
    } else if (state_ == State::kInner) {
      storage_.inner_.destruct();
    }
  }

  // Operation States are not copyable nor movable
  LetValueOperationState(const LetValueOperationState&) = delete;
  auto operator=(const LetValueOperationState&) -> LetValueOperationState& = delete;
  LetValueOperationState(LetValueOperationState&&) = delete;
  auto operator=(LetValueOperationState&&) -> LetValueOperationState& = delete;

  void start() noexcept {
    storage_.outer_->start();
  }

  // Transition from outer to inner operation
  // Called by LetValueReceiver::setValue
  template <typename... Args>
  void transitionToInner(Args&&... args) noexcept {
    // Step 1: Destroy outer operation (frees union storage)
    storage_.outer_.destruct();
    state_ = State::kEmpty;

    // Step 2: Call function to get inner sender
    auto inner_sender = func_(std::forward<Args>(args)...);

    // Step 3: Construct inner operation in the SAME memory (union reuse)
    storage_.inner_.constructWith([&] {
      return std::move(inner_sender).connect(std::move(receiver_));
    });
    state_ = State::kInner;

    // Step 4: Start the inner operation
    storage_.inner_->start();
  }

  void forwardError(std::error_code ec) noexcept {
    receiver_.setError(ec);
  }

  void forwardStopped() noexcept {
    receiver_.setStopped();
  }

 private:
  F func_;
  R receiver_;
  State state_;

  // Union: only one operation active at a time
  // Both types are known at compile time!
  union Storage {
    Storage() {}
    ~Storage() {}
    ManualLifetime<OuterOpState> outer_;
    ManualLifetime<InnerOpState> inner_;
  } storage_;
};

template <typename S, typename F>
class LetValueSender {
 public:
  // The result type is the ValueTypes of the sender returned by F
  using InnerSender = decltype(std::apply(
      std::declval<F>(), std::declval<typename S::ValueTypes>()));

  // Validate that the function returns a valid Sender type
  static_assert(Sender<InnerSender>,
                "The function passed to letValue must return a Sender type. "
                "Ensure your lambda/function returns a sender (e.g., just(...)).");

  using ValueTypes = typename InnerSender::ValueTypes;

  LetValueSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return LetValueOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
LetValueSender(S, F)
    -> LetValueSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct LetValueAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return LetValueSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto letValue(S&& sender, F&& func) {
  return LetValueSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a LetValue adaptor for use with operator|
template <typename F>
auto letValue(F&& func) {
  return LetValueAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
auto operator|(S&& sender, const LetValueAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
