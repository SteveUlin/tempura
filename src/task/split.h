// SplitSender - makes a sender multi-shot by caching its result
//
// The split() algorithm transforms a single-shot sender into a multi-shot
// sender. The underlying sender is executed at most once, and its result is
// broadcast to all connected receivers.
//
// Semantics:
// - First start() triggers the underlying sender
// - Result is cached in shared state
// - Subsequent receivers get the cached result
// - Thread-safe: can be used with concurrent schedulers
//
// Example:
//   auto shared = split(just(42) | then([](int x) { return x * 2; }));
//   auto a = syncWait(shared);  // Computes 84
//   auto b = syncWait(shared);  // Returns cached 84, no recomputation

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "completion_signatures.h"
#include "concepts.h"
#include "env.h"
#include "meta/manual_lifetime.h"
#include "on.h"  // For GetOnValueTupleT

namespace tempura {

// Forward declarations
template <typename S>
class SplitSender;

template <typename S>
class SplitSharedState;

template <typename S, typename R>
class SplitOperationState;

template <typename S>
class SplitInnerReceiver;

// ============================================================================
// SplitSharedState - Shared state for all split receivers
// ============================================================================

template <typename S>
class SplitSharedState {
 public:
  using ValueTuple = GetOnValueTupleT<S>;
  using CompletionSigs = GetCompletionSignaturesT<S, EmptyEnv>;
  using ErrorSigs = ErrorSignaturesT<CompletionSigs>;

  // Error variant type (or void if no errors)
  template <typename List>
  struct ErrorVariantFromList;

  template <typename... Sigs>
  struct ErrorVariantFromList<TypeList<Sigs...>> {
    template <typename Sig>
    struct GetErrorArg;

    template <typename E>
    struct GetErrorArg<SetErrorTag(E)> {
      using Type = E;
    };

    using Type = std::variant<typename GetErrorArg<Sigs>::Type...>;
  };

  // Use std::monostate if no error signatures
  using ErrorVariant = std::conditional_t<
      Size_v<ErrorSigs> == 0,
      std::monostate,
      typename ErrorVariantFromList<ErrorSigs>::Type>;

  enum class State {
    kNotStarted,  // Sender not yet started
    kRunning,     // Sender is running
    kValue,       // Completed with value
    kError,       // Completed with error
    kStopped      // Completed with stopped
  };

  explicit SplitSharedState(S sender)
      : sender_(std::move(sender)),
        state_(State::kNotStarted),
        inner_op_constructed_(false) {}

  ~SplitSharedState() {
    if (inner_op_constructed_) {
      inner_op_.destruct();
    }
  }

  // Non-copyable, non-movable
  SplitSharedState(const SplitSharedState&) = delete;
  auto operator=(const SplitSharedState&) = delete;
  SplitSharedState(SplitSharedState&&) = delete;
  auto operator=(SplitSharedState&&) = delete;

  // Try to start the sender (first caller wins)
  // Returns true if this caller started the sender
  auto tryStart() -> bool {
    std::lock_guard lock(mutex_);
    if (state_ != State::kNotStarted) {
      return false;
    }
    state_ = State::kRunning;
    return true;
  }

  // Actually start the inner operation (must be called after tryStart returns
  // true)
  void startInner() {
    inner_op_.constructWith([this] {
      return std::move(sender_).connect(SplitInnerReceiver<S>{this});
    });
    inner_op_constructed_ = true;
    inner_op_->start();
  }

  // Called when inner sender completes with value
  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    {
      std::lock_guard lock(mutex_);
      value_.emplace(std::forward<Args>(args)...);
      state_ = State::kValue;
    }
    notifyWaiters();
  }

  // Called when inner sender completes with error
  template <typename Error>
  void setError(Error&& error) noexcept {
    {
      std::lock_guard lock(mutex_);
      if constexpr (!std::is_same_v<ErrorVariant, std::monostate>) {
        error_.emplace(std::forward<Error>(error));
      }
      state_ = State::kError;
    }
    notifyWaiters();
  }

  // Called when inner sender is stopped
  void setStopped() noexcept {
    {
      std::lock_guard lock(mutex_);
      state_ = State::kStopped;
    }
    notifyWaiters();
  }

  // Register a waiter and get current state
  // If state is terminal, returns immediately without registering
  template <typename Waiter>
  auto registerWaiter(Waiter* waiter) -> State {
    std::lock_guard lock(mutex_);
    if (state_ == State::kValue || state_ == State::kError ||
        state_ == State::kStopped) {
      return state_;
    }
    waiters_.push_back([waiter] { waiter->notify(); });
    return state_;
  }

  // Accessors for completed values
  [[nodiscard]] auto getValue() const noexcept -> const ValueTuple& {
    return *value_;
  }

  [[nodiscard]] auto getError() const noexcept -> const ErrorVariant& {
    return *error_;
  }

  [[nodiscard]] auto getState() const noexcept -> State {
    std::lock_guard lock(mutex_);
    return state_;
  }

 private:
  void notifyWaiters() {
    std::vector<std::function<void()>> to_notify;
    {
      std::lock_guard lock(mutex_);
      to_notify = std::move(waiters_);
      waiters_.clear();
    }
    for (auto& waiter : to_notify) {
      waiter();
    }
  }

  S sender_;
  mutable std::mutex mutex_;
  State state_;
  std::optional<ValueTuple> value_;
  std::optional<ErrorVariant> error_;
  std::vector<std::function<void()>> waiters_;

  // Inner operation state (constructed when sender is started)
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<SplitInnerReceiver<S>>()));
  ManualLifetime<InnerOpState> inner_op_;
  bool inner_op_constructed_;
};

// ============================================================================
// SplitInnerReceiver - Receiver that forwards to shared state
// ============================================================================

template <typename S>
class SplitInnerReceiver {
 public:
  using SharedState = SplitSharedState<S>;

  explicit SplitInnerReceiver(SharedState* state) : state_(state) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    state_->setValue(std::forward<Args>(args)...);
  }

  template <typename Error>
  void setError(Error&& error) noexcept {
    state_->setError(std::forward<Error>(error));
  }

  void setStopped() noexcept { state_->setStopped(); }

 private:
  SharedState* state_;
};

// ============================================================================
// SplitOperationState - Operation state for each connected receiver
// ============================================================================

template <typename S, typename R>
class SplitOperationState {
 public:
  using SharedState = SplitSharedState<S>;
  using ValueTuple = typename SharedState::ValueTuple;
  using ErrorVariant = typename SharedState::ErrorVariant;

  SplitOperationState(std::shared_ptr<SharedState> state, R receiver)
      : state_(std::move(state)), receiver_(std::move(receiver)) {}

  // Non-copyable, non-movable
  SplitOperationState(const SplitOperationState&) = delete;
  auto operator=(const SplitOperationState&) = delete;
  SplitOperationState(SplitOperationState&&) = delete;
  auto operator=(SplitOperationState&&) = delete;

  void start() noexcept {
    // Try to be the one to start the sender
    if (state_->tryStart()) {
      // We're responsible for starting
      state_->startInner();
    }

    // Register as waiter (or get immediate result)
    auto current_state = state_->registerWaiter(this);

    // If already complete, deliver result immediately
    if (current_state == SharedState::State::kValue ||
        current_state == SharedState::State::kError ||
        current_state == SharedState::State::kStopped) {
      deliverResult(current_state);
    }
    // Otherwise, notify() will be called when complete
  }

  // Called by shared state when result is ready
  void notify() noexcept { deliverResult(state_->getState()); }

 private:
  void deliverResult(typename SharedState::State state) noexcept {
    switch (state) {
      case SharedState::State::kValue:
        deliverValue(std::make_index_sequence<
                     std::tuple_size_v<ValueTuple>>{});
        break;
      case SharedState::State::kError:
        if constexpr (!std::is_same_v<ErrorVariant, std::monostate>) {
          std::visit(
              [this](auto&& error) {
                receiver_.setError(std::forward<decltype(error)>(error));
              },
              state_->getError());
        }
        break;
      case SharedState::State::kStopped:
        receiver_.setStopped();
        break;
      default:
        // Should not happen
        break;
    }
  }

  template <std::size_t... Is>
  void deliverValue(std::index_sequence<Is...>) noexcept {
    // Copy values from the cached tuple (split can be consumed multiple times)
    auto value = state_->getValue();
    receiver_.setValue(std::get<Is>(std::move(value))...);
  }

  std::shared_ptr<SharedState> state_;
  R receiver_;
};

// ============================================================================
// SplitSender - Multi-shot sender wrapper
// ============================================================================

template <typename S>
class SplitSender {
 public:
  using SharedState = SplitSharedState<S>;

  // Completion signatures: same as underlying sender
  template <typename Env = EmptyEnv>
  using CompletionSignatures = GetCompletionSignaturesT<S, Env>;

  explicit SplitSender(S sender)
      : state_(std::make_shared<SharedState>(std::move(sender))) {}

  // Copy constructor - shares the state
  SplitSender(const SplitSender&) = default;
  auto operator=(const SplitSender&) -> SplitSender& = default;

  // Move constructor
  SplitSender(SplitSender&&) = default;
  auto operator=(SplitSender&&) -> SplitSender& = default;

  template <typename R>
  auto connect(R&& receiver) const& {
    return SplitOperationState<S, std::remove_cvref_t<R>>{
        state_, std::forward<R>(receiver)};
  }

  template <typename R>
  auto connect(R&& receiver) && {
    return SplitOperationState<S, std::remove_cvref_t<R>>{
        std::move(state_), std::forward<R>(receiver)};
  }

 private:
  std::shared_ptr<SharedState> state_;
};

template <typename S>
SplitSender(S) -> SplitSender<std::remove_cvref_t<S>>;

// Helper function
template <Sender S>
auto split(S&& sender) {
  return SplitSender{std::forward<S>(sender)};
}

}  // namespace tempura
