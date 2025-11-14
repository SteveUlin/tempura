// Async Task Execution System - Sender/Receiver Model
//
// This file provides a compile-time task expression system based on the P2300
// sender/receiver model. Senders are types that describe asynchronous work
// without executing it. Each sender exposes its value types through a
// `ValueTypes` alias for compile-time type deduction.

#pragma once

#include <concepts>
#include <optional>
#include <print>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>

namespace tempura {

// ============================================================================
// Core Concepts
// ============================================================================

// A type that can can take in some task output and do something with it.
//
// All three completion channels (setValue, setError, setStopped) must be
// noexcept since operation states cannot provide exception safety in tempura's
// no-exceptions environment.
template <typename R, typename... Args>
concept ReceiverOf =
    std::move_constructible<R> &&
    requires(R&& r, Args&&... args, std::error_code ec) {
      {
        std::forward<R>(r).setValue(std::forward<Args>(args)...)
      } noexcept -> std::same_as<void>;
      { std::forward<R>(r).setError(ec) } noexcept -> std::same_as<void>;
      { std::forward<R>(r).setStopped() } noexcept -> std::same_as<void>;
    };

// Schedulers create senders that represent the scheduling of work.
//
// Note: Equality comparison is not required in this simplified Phase 1
// implementation. P2300 requires it for scheduler optimization, but we defer
// that complexity to Phase 4.
template <typename S>
concept Scheduler =
    std::copy_constructible<std::remove_cvref_t<S>> && requires(const S& s) {
      { s.schedule() };  // Returns a sender
    };

// OperationState represents a stateful asynchronous operation that can be
// started.
template <typename O>
concept OperationState = requires(O& o) { o.start(); };

// Sender lazily describes a unit of asynchronous work.
//
// All senders must expose a `ValueTypes` alias (typically std::tuple<Args...>)
// to enable compile-time type deduction of their output values.
template <typename S>
concept Sender = std::move_constructible<S> && requires {
  typename std::remove_cvref_t<S>::ValueTypes;
};

// SenderTo checks that a sender can be connected to a specific receiver.
//
// Note: We don't check ReceiverOf here because we'd need to know the sender's
// value types. The connect() requirement provides sufficient validation - if
// the receiver can't accept the sender's values, connect() will fail to
// compile.
template <typename S, typename R>
concept SenderTo = Sender<S> && std::move_constructible<R> &&
                   requires(S&& s, R&& r) {
                     {
                       std::forward<S>(s).connect(std::forward<R>(r))
                     } -> OperationState;
                   };

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

// ============================================================================
// Fundamental Sender Types
// ============================================================================

// TODO: Implement Schedule sender

// Operation state for JustSender - unpacks tuple and forwards to receiver
template <typename R, typename... Args>
class JustOperationState {
 public:
  JustOperationState(std::tuple<Args...> values, R r)
      : values_(std::move(values)), receiver_(std::move(r)) {}

  // Operation States are not copyable nor movable
  JustOperationState(const JustOperationState&) = delete;
  auto operator=(const JustOperationState&) -> JustOperationState& = delete;
  JustOperationState(JustOperationState&&) = delete;
  auto operator=(JustOperationState&&) -> JustOperationState& = delete;

  void start() noexcept {
    // Unpack tuple and call variadic setValue
    std::apply(
        [this](auto&&... args) {
          std::forward<R>(receiver_).setValue(
              std::forward<decltype(args)>(args)...);
        },
        std::move(values_));
  }

 private:
  std::tuple<Args...> values_;
  R receiver_;
};

template <typename... Args>
  requires(std::move_constructible<Args> && ...)
class JustSender {
 public:
  using ValueTypes = std::tuple<Args...>;

  JustSender(Args&&... args) : values_{std::forward<Args>(args)...} {}

  template <ReceiverOf<Args...> R>
  auto connect(R&& receiver) && {
    return JustOperationState<R, Args...>{std::move(values_),
                                          std::forward<R>(receiver)};
  }

 private:
  std::tuple<Args...> values_;
};

template <typename... Args>
JustSender(Args&&...) -> JustSender<std::decay_t<Args>...>;

// Helper function to create a JustSender
template <typename... Args>
auto just(Args&&... args) {
  return JustSender{std::forward<Args>(args)...};
}

// TODO: Implement Transfer sender

// ============================================================================
// Composition Senders
// ============================================================================

template <typename F, typename R>
class ThenReceiver {
 public:
  ThenReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    auto result = (*func_)(std::forward<Args>(args)...);
    receiver_->setValue(std::move(result));
  }

  void setError(std::error_code ec) noexcept { receiver_->setError(ec); }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class ThenOperationState {
 public:
  using InnerReceiver = ThenReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  ThenOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  ThenOperationState(const ThenOperationState&) = delete;
  auto operator=(const ThenOperationState&) -> ThenOperationState& = delete;
  ThenOperationState(ThenOperationState&&) = delete;
  auto operator=(ThenOperationState&&) -> ThenOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class ThenSender {
 public:
  using ValueTypes = std::tuple<decltype(std::apply(
      std::declval<F>(), std::declval<typename S::ValueTypes>()))>;

  ThenSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return ThenOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
ThenSender(S, F) -> ThenSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct ThenAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return ThenSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto then(S&& sender, F&& func) {
  return ThenSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a Then adaptor for use with operator|
template <typename F>
auto then(F&& func) {
  return ThenAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
  requires requires(S&& s, const ThenAdaptor<F>& adaptor) {
    adaptor(std::forward<S>(s));
  }
auto operator|(S&& sender, const ThenAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

template <typename F, typename R>
class UponErrorReceiver {
 public:
  UponErrorReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    receiver_->setValue(std::forward<Args>(args)...);
  }

  void setError(std::error_code ec) noexcept {
    auto result = (*func_)(ec);
    receiver_->setValue(std::move(result));
  }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class UponErrorOperationState {
 public:
  using InnerReceiver = UponErrorReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  UponErrorOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  UponErrorOperationState(const UponErrorOperationState&) = delete;
  auto operator=(const UponErrorOperationState&)
      -> UponErrorOperationState& = delete;
  UponErrorOperationState(UponErrorOperationState&&) = delete;
  auto operator=(UponErrorOperationState&&)
      -> UponErrorOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class UponErrorSender {
 public:
  using ValueTypes =
      std::tuple<decltype(std::declval<F>()(std::declval<std::error_code>()))>;

  UponErrorSender(S sender, F func)
      : sender_(std::move(sender)), func_(std::move(func)) {}

  template <typename R>
  auto connect(R&& receiver) && {
    return UponErrorOperationState<S, F, std::remove_cvref_t<R>>{
        std::move(sender_), std::move(func_), std::forward<R>(receiver)};
  }

 private:
  S sender_;
  F func_;
};

template <typename S, typename F>
UponErrorSender(S, F)
    -> UponErrorSender<std::remove_cvref_t<S>, std::remove_cvref_t<F>>;

// Adaptor for pipe operator - wraps a transformation function
template <typename F>
struct UponErrorAdaptor {
  F func;

  template <typename S>
  auto operator()(S&& sender) const {
    return UponErrorSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
auto uponError(S&& sender, F&& func) {
  return UponErrorSender{std::forward<S>(sender), std::forward<F>(func)};
}

// Helper to create a UponError adaptor for use with operator|
template <typename F>
auto uponError(F&& func) {
  return UponErrorAdaptor<std::decay_t<F>>{std::forward<F>(func)};
}

// Pipe operator for sender | adaptor syntax
template <typename S, typename F>
  requires requires(S&& s, const UponErrorAdaptor<F>& adaptor) {
    adaptor(std::forward<S>(s));
  }
auto operator|(S&& sender, const UponErrorAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

// TODO: Implement uponStopped sender

// ============================================================================
// letValue - chains asynchronous operations by returning new senders
// ============================================================================

template <typename F, typename R>
class LetValueReceiver {
 public:
  LetValueReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    // Apply function to get a new sender
    auto inner_sender = (*func_)(std::forward<Args>(args)...);

    // Connect and start the inner sender with our downstream receiver
    auto op = std::move(inner_sender).connect(std::move(*receiver_));
    op.start();
  }

  void setError(std::error_code ec) noexcept { receiver_->setError(ec); }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class LetValueOperationState {
 public:
  using InnerReceiver = LetValueReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  LetValueOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  LetValueOperationState(const LetValueOperationState&) = delete;
  auto operator=(const LetValueOperationState&)
      -> LetValueOperationState& = delete;
  LetValueOperationState(LetValueOperationState&&) = delete;
  auto operator=(LetValueOperationState&&) -> LetValueOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class LetValueSender {
 public:
  // The result type is the ValueTypes of the sender returned by F
  using InnerSender = decltype(std::apply(
      std::declval<F>(), std::declval<typename S::ValueTypes>()));
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
  requires requires(S&& s, const LetValueAdaptor<F>& adaptor) {
    adaptor(std::forward<S>(s));
  }
auto operator|(S&& sender, const LetValueAdaptor<F>& adaptor) {
  return adaptor(std::forward<S>(sender));
}

// ============================================================================
// letError - chains asynchronous error recovery by returning new senders
// ============================================================================

template <typename F, typename R>
class LetErrorReceiver {
 public:
  LetErrorReceiver(F* func, R* receiver) : func_(func), receiver_(receiver) {}

  template <typename... Args>
  void setValue(Args&&... args) noexcept {
    receiver_->setValue(std::forward<Args>(args)...);
  }

  void setError(std::error_code ec) noexcept {
    // Apply function to get a new sender
    auto inner_sender = (*func_)(ec);

    // Connect and start the inner sender with our downstream receiver
    auto op = std::move(inner_sender).connect(std::move(*receiver_));
    op.start();
  }

  void setStopped() noexcept { receiver_->setStopped(); }

 private:
  F* func_;
  R* receiver_;
};

template <typename S, typename F, typename R>
class LetErrorOperationState {
 public:
  using InnerReceiver = LetErrorReceiver<F, R>;
  using InnerOpState =
      decltype(std::declval<S>().connect(std::declval<InnerReceiver>()));

  LetErrorOperationState(S sender, F func, R receiver)
      : func_(std::move(func)),
        receiver_(std::move(receiver)),
        inner_op_(
            std::move(sender).connect(InnerReceiver{&func_, &receiver_})) {}

  // Operation States are not copyable nor movable
  LetErrorOperationState(const LetErrorOperationState&) = delete;
  auto operator=(const LetErrorOperationState&)
      -> LetErrorOperationState& = delete;
  LetErrorOperationState(LetErrorOperationState&&) = delete;
  auto operator=(LetErrorOperationState&&) -> LetErrorOperationState& = delete;

  void start() noexcept { inner_op_.start(); }

 private:
  F func_;
  R receiver_;
  InnerOpState inner_op_;
};

template <typename S, typename F>
class LetErrorSender {
 public:
  // The result type is the ValueTypes of the sender returned by F
  using InnerSender = decltype(std::declval<F>()(std::declval<std::error_code>()));
  using ValueTypes = typename InnerSender::ValueTypes;

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
  auto operator()(S&& sender) const {
    return LetErrorSender{std::forward<S>(sender), func};
  }
};

template <typename S, typename F>
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

// ============================================================================
// Parallel Composition
// ============================================================================

// TODO: Implement whenAll sender

// TODO: Implement whenAny sender

// ============================================================================
// Scheduler Implementations
// ============================================================================

// TODO: Implement InlineScheduler

// TODO: Implement ThreadPoolScheduler

// TODO: Implement SingleThreadScheduler

// ============================================================================
// Stop Token Support
// ============================================================================

// TODO: Implement StopToken

// TODO: Implement StopSource

// ============================================================================
// Algorithms
// ============================================================================

// Synchronously wait for a sender to complete and return its value.
//
// This function blocks until the sender completes, then returns an optional
// containing the values produced by the sender. If the sender calls setError()
// or setStopped(), the optional will be empty.
//
// Returns: std::optional<Sender::ValueTypes>
//
// Example:
//   auto result = syncWait(just(42, "hello"));
//   if (result) {
//     auto [num, str] = *result;  // num=42, str="hello"
//   }
template <typename S>
auto syncWait(S&& sender) {
  using ValueTuple = typename std::remove_cvref_t<S>::ValueTypes;

  // Create storage for the result
  std::optional<ValueTuple> result;

  // Create a receiver that will store values into our optional
  // Use type_identity to unpack tuple type into parameter pack
  auto receiver = []<typename... Args>(
                      std::type_identity<std::tuple<Args...>>,
                      std::optional<std::tuple<Args...>>& opt) {
    return ValueReceiver<Args...>{opt};
  }(std::type_identity<ValueTuple>{}, result);

  // Connect the sender to the receiver to create an operation state
  auto op = std::forward<S>(sender).connect(std::move(receiver));

  // Start the operation (this is synchronous for JustSender)
  op.start();

  // Return the result (will be empty if error or stopped)
  return result;
}

// TODO: Implement startDetached

// TODO: Implement connect customization point

// TODO: Implement start customization point
}  // namespace tempura
