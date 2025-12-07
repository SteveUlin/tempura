// LetStoppedSender - chains asynchronous stopped recovery by returning new senders

#pragma once

#include "concepts.h"
#include "completion_signatures.h"
#include "meta/manual_lifetime.h"

#include <functional>
#include <type_traits>
#include <utility>

namespace tempura {

// ============================================================================
// Concept: Check if sender has stopped signature
// ============================================================================

template <typename S, typename Env = EmptyEnv>
concept HasStoppedSig = requires {
  typename GetCompletionSignaturesT<S, Env>;
  requires kHasStoppedSignature<GetCompletionSignaturesT<S, Env>>;
};

// ============================================================================
// letStopped - chains asynchronous stopped recovery by returning new senders
// ============================================================================

template <typename S, typename F, typename R>
class LetStoppedOperationState;

template <typename F, typename R, typename OuterOp>
class LetStoppedReceiver {
 public:
  LetStoppedReceiver(OuterOp* outer_op) : outer_op_(outer_op) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    outer_op_->forwardValue(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    outer_op_->forwardError(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept {
    outer_op_->transitionToInner();
  }

 private:
  OuterOp* outer_op_;
};

template <typename S, typename F, typename R>
class LetStoppedOperationState {
 public:
  using InnerReceiver = LetStoppedReceiver<F, R, LetStoppedOperationState>;
  using OuterOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  // Compute inner operation type at compile time
  // InnerSender = return type of F when called with no arguments
  using InnerSender = std::invoke_result_t<F>;
  // InnerOpState = connecting that sender to our receiver (moved)
  using InnerOpState = decltype(std::declval<InnerSender&&>().connect(std::declval<R&&>()));

  enum class State : unsigned char {
    kOuter,  // Outer operation is active
    kInner,  // Inner operation is active
    kEmpty   // Transitioning
  };

  LetStoppedOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        state_(State::kOuter) {
    // Construct outer operation in union storage (guaranteed copy elision via lambda)
    storage_.outer_.constructWith([&] {
      return std::move(sender).connect(InnerReceiver{this});
    });
  }

  ~LetStoppedOperationState() {
    if (state_ == State::kOuter) {
      storage_.outer_.destruct();
    } else if (state_ == State::kInner) {
      storage_.inner_.destruct();
    }
  }

  // Operation States are not copyable nor movable
  LetStoppedOperationState(const LetStoppedOperationState&) = delete;
  auto operator=(const LetStoppedOperationState&) -> LetStoppedOperationState& = delete;
  LetStoppedOperationState(LetStoppedOperationState&&) = delete;
  auto operator=(LetStoppedOperationState&&) -> LetStoppedOperationState& = delete;

  void start() noexcept {
    storage_.outer_->start();
  }

  // Transition from outer to inner operation
  // Called by LetStoppedReceiver::setStopped
  void transitionToInner() noexcept {
    // Step 1: Destroy outer operation (frees union storage)
    storage_.outer_.destruct();
    state_ = State::kEmpty;

    // Step 2: Call function to get inner sender (no arguments for stopped)
    auto inner_sender = std::invoke(func_);

    // Step 3: Construct inner operation in the SAME memory (union reuse, guaranteed copy elision via lambda)
    storage_.inner_.constructWith([&] {
      return std::move(inner_sender).connect(std::move(receiver_));
    });
    state_ = State::kInner;

    // Step 4: Start the inner operation
    storage_.inner_->start();
  }

  template <typename... Args>
  void forwardValue(Args&&... args) noexcept {
    receiver_.setValue(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void forwardError(ErrorArgs&&... args) noexcept {
    receiver_.setError(std::forward<ErrorArgs>(args)...);
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
class LetStoppedSender {
 public:
  // Compute completion signatures from the inner sender returned by F
  template <typename Env = EmptyEnv>
  struct CompletionSignaturesHelper {
    // Compute the inner sender type by invoking F with no arguments
    using InnerSender = std::invoke_result_t<F>;

    // Validate that F returns a Sender
    static_assert(Sender<InnerSender>,
                  "The function passed to letStopped must return a Sender type. "
                  "Ensure your lambda/function returns a sender (e.g., just(...)).");

    // Get inner sender's completion signatures
    using InnerCompletionSigs = GetCompletionSignaturesT<InnerSender, Env>;

    // Get outer sender's value and error signatures (to propagate completions that bypass letStopped)
    using OuterCompletionSigs = GetCompletionSignaturesT<S, Env>;
    using OuterValueSigs = ValueSignaturesT<OuterCompletionSigs>;
    using OuterErrorSigs = ErrorSignaturesT<OuterCompletionSigs>;

    // Merge inner signatures with outer values and errors
    using Type = MergeCompletionSignaturesT<InnerCompletionSigs,
                                            ListToCompletionSignaturesT<OuterValueSigs>,
                                            ListToCompletionSignaturesT<OuterErrorSigs>>;
  };

  template <typename Env = EmptyEnv>
  using CompletionSignatures = typename CompletionSignaturesHelper<Env>::Type;

  LetStoppedSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return LetStoppedOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
LetStoppedSender(S, F)
    -> LetStoppedSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct LetStoppedAdaptor {
  F func;

  template <typename S>
    requires HasStoppedSig<S>
  auto operator()(S&& sender) const {
    return LetStoppedSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
  requires HasStoppedSig<S>
auto letStopped(S&& sender, F&& func) {
  return LetStoppedSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a LetStopped adaptor for use with operator|
template <typename F>
auto letStopped(F&& func) {
  return LetStoppedAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
  requires requires(S&& s, const LetStoppedAdaptor<F>& adaptor) {
    adaptor(std::forward<S>(s));
  }
auto operator|(S&& sender, const LetStoppedAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

}  // namespace tempura
