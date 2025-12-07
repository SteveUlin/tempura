// LetErrorSender - chains asynchronous error recovery by returning new senders

#pragma once

#include "concepts.h"
#include "completion_signatures.h"
#include "meta/manual_lifetime.h"

#include <functional>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tempura {

// ============================================================================
// Concept: Check if sender has error signatures
// ============================================================================

template <typename S, typename Env = EmptyEnv>
concept HasErrorSignature = requires {
  typename GetCompletionSignaturesT<S, Env>;
  requires Size_v<ErrorSignaturesT<GetCompletionSignaturesT<S, Env>>> > 0;
};

// Helper: Extract error tuple from sender's first error signature
template <typename S, typename Env = EmptyEnv>
struct GetErrorTuple {
  using CompletionSigs = GetCompletionSignaturesT<S, Env>;
  using ErrorSigs = ErrorSignaturesT<CompletionSigs>;

  static_assert(Size_v<ErrorSigs> > 0,
                "Sender must have at least one error signature for letError");

  using FirstErrorSig = Get_t<0, ErrorSigs>;
  using ErrorArgs = SignatureArgsT<FirstErrorSig>;
  using Type = ListToTupleT<ErrorArgs>;
};

template <typename S, typename Env = EmptyEnv>
using GetErrorTupleT = typename GetErrorTuple<S, Env>::Type;

// ============================================================================
// letError - chains asynchronous error recovery by returning new senders
// ============================================================================

template <typename S, typename F, typename R>
class LetErrorOperationState;

template <typename F, typename R, typename OuterOp>
class LetErrorReceiver {
 public:
  LetErrorReceiver(OuterOp* outer_op) : outer_op_(outer_op) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    outer_op_->forwardValue(std::forward<Args>(args)...);
  }

  template <typename... ErrorArgs>
  void setError(ErrorArgs&&... args) noexcept {
    outer_op_->transitionToInner(std::forward<ErrorArgs>(args)...);
  }

  void setStopped() noexcept { outer_op_->forwardStopped(); }

 private:
  OuterOp* outer_op_;
};

template <typename S, typename F, typename R>
class LetErrorOperationState {
 public:
  using InnerReceiver = LetErrorReceiver<F, R, LetErrorOperationState>;
  using OuterOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  // Compute inner operation type at compile time!
  // InnerSender = return type of F when called with S's error tuple
  using ErrorTuple = GetErrorTupleT<S>;
  using InnerSender = decltype(std::apply(std::declval<F&>(), std::declval<ErrorTuple>()));
  // InnerOpState = connecting that sender to our receiver (moved)
  using InnerOpState = decltype(std::declval<InnerSender&&>().connect(std::declval<R&&>()));

  enum class State : unsigned char {
    kOuter,  // Outer operation is active
    kInner,  // Inner operation is active
    kEmpty   // Transitioning
  };

  LetErrorOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        state_(State::kOuter) {
    // Construct outer operation in union storage (guaranteed copy elision via lambda)
    storage_.outer_.constructWith([&] {
      return std::move(sender).connect(InnerReceiver{this});
    });
  }

  ~LetErrorOperationState() {
    if (state_ == State::kOuter) {
      storage_.outer_.destruct();
    } else if (state_ == State::kInner) {
      storage_.inner_.destruct();
    }
  }

  // Operation States are not copyable nor movable
  LetErrorOperationState(const LetErrorOperationState&) = delete;
  auto operator=(const LetErrorOperationState&) -> LetErrorOperationState& = delete;
  LetErrorOperationState(LetErrorOperationState&&) = delete;
  auto operator=(LetErrorOperationState&&) -> LetErrorOperationState& = delete;

  void start() noexcept {
    storage_.outer_->start();
  }

  // Transition from outer to inner operation
  // Called by LetErrorReceiver::setError
  template <typename... ErrorArgs>
  void transitionToInner(ErrorArgs&&... args) noexcept {
    // Step 1: Destroy outer operation (frees union storage)
    storage_.outer_.destruct();
    state_ = State::kEmpty;

    // Step 2: Call function to get inner sender
    auto inner_sender = std::invoke(func_, std::forward<ErrorArgs>(args)...);

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
class LetErrorSender {
 public:
  // Compute completion signatures from the inner sender returned by F
  template <typename Env = EmptyEnv>
  struct CompletionSignaturesHelper {
    // Get the error tuple from outer sender's first error signature
    using ErrorTuple = GetErrorTupleT<S, Env>;

    // Compute the inner sender type by applying F to the error tuple
    using InnerSender = decltype(std::apply(std::declval<F>(), std::declval<ErrorTuple>()));

    // Validate that F returns a Sender
    static_assert(Sender<InnerSender>,
                  "The function passed to letError must return a Sender type. "
                  "Ensure your lambda/function returns a sender (e.g., just(...)).");

    // Get inner sender's completion signatures
    using InnerCompletionSigs = GetCompletionSignaturesT<InnerSender, Env>;

    // Get outer sender's value signatures (to propagate values that bypass letError)
    using OuterCompletionSigs = GetCompletionSignaturesT<S, Env>;
    using OuterValueSigs = ValueSignaturesT<OuterCompletionSigs>;

    // Merge inner signatures with outer values
    using Type = MergeCompletionSignaturesT<InnerCompletionSigs,
                                            ListToCompletionSignaturesT<OuterValueSigs>>;
  };

  template <typename Env = EmptyEnv>
  using CompletionSignatures = typename CompletionSignaturesHelper<Env>::Type;

  LetErrorSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return LetErrorOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
LetErrorSender(S, F)
    -> LetErrorSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct LetErrorAdaptor {
  F func;

  template <typename S>
    requires HasErrorSignature<S>
  auto operator()(S&& sender) const {
    return LetErrorSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
  requires HasErrorSignature<S>
auto letError(S&& sender, F&& func) {
  return LetErrorSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a LetError adaptor for use with operator|
template <typename F>
auto letError(F&& func) {
  return LetErrorAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
  requires requires(S&& s, const LetErrorAdaptor<F>& adaptor) {
    adaptor(std::forward<S>(s));
  }
auto operator|(S&& sender, const LetErrorAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

// TODO: Implement letStopped sender

}  // namespace tempura
