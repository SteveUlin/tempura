#pragma once

#include <atomic>
#include <concepts>
#include <type_traits>
#include <utility>

namespace tempura {

// Forward declarations
class InplaceStopSource;
template <typename Callback>
class InplaceStopCallback;

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

// Base class for callback list nodes (type-erased)
struct InplaceStopCallbackBase {
  InplaceStopCallbackBase* next_{nullptr};
  InplaceStopCallbackBase* prev_{nullptr};
  std::atomic<bool> callback_completed_{false};

  virtual ~InplaceStopCallbackBase() = default;
  virtual void execute() noexcept = 0;
};

// In-place stop source with callback support
// Manages intrusive doubly-linked list of callbacks
class InplaceStopSource {
  // Callback list head (protected by atomic operations)
  std::atomic<InplaceStopCallbackBase*> callbacks_{nullptr};
  std::atomic<bool> stop_requested_{false};

  // Allow InplaceStopCallback to access private members
  template <typename>
  friend class InplaceStopCallback;

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

   private:
    friend class InplaceStopSource;
    template <typename>
    friend class InplaceStopCallback;

    auto get_source() const noexcept -> InplaceStopSource* { return source_; }
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
    // Atomically set stop_requested and get previous value
    bool was_stopped = stop_requested_.exchange(true, std::memory_order_acq_rel);

    if (was_stopped) {
      return false;  // Already stopped
    }

    // Invoke all registered callbacks synchronously
    InplaceStopCallbackBase* head = callbacks_.load(std::memory_order_acquire);
    while (head != nullptr) {
      head->execute();
      head->callback_completed_.store(true, std::memory_order_release);
      head = head->next_;
    }

    return true;  // We initiated the stop
  }

  auto stop_requested() const noexcept -> bool {
    return stop_requested_.load(std::memory_order_acquire);
  }

 private:
  // Register a callback (called by InplaceStopCallback constructor)
  auto register_callback(InplaceStopCallbackBase* callback) noexcept -> bool {
    // If already stopped, don't register - callback will be invoked immediately
    if (stop_requested_.load(std::memory_order_acquire)) {
      return false;
    }

    // Add to head of list using compare-exchange loop
    InplaceStopCallbackBase* old_head = callbacks_.load(std::memory_order_relaxed);
    do {
      callback->next_ = old_head;
      if (old_head) {
        old_head->prev_ = callback;
      }
    } while (!callbacks_.compare_exchange_weak(
        old_head, callback,
        std::memory_order_release,
        std::memory_order_relaxed));

    // Check again if stop was requested while we were registering
    if (stop_requested_.load(std::memory_order_acquire)) {
      // Need to remove ourselves and return false
      unregister_callback(callback);
      return false;
    }

    return true;
  }

  // Unregister a callback (called by InplaceStopCallback destructor)
  void unregister_callback(InplaceStopCallbackBase* callback) noexcept {
    // If stop was requested, we might be in the middle of callback invocation
    // In that case, we need to wait for our callback to complete
    if (stop_requested_.load(std::memory_order_acquire)) {
      // Spin until our callback completes (or was already completed)
      while (!callback->callback_completed_.load(std::memory_order_acquire)) {
        // Wait for callback invocation to finish
        // This prevents use-after-free if request_stop() is invoking our callback
      }
      return;  // Don't need to unlink, callback already executed
    }

    // Stop not requested yet - safe to unlink from list
    // Try to atomically remove from list
    InplaceStopCallbackBase* expected = callback;

    // If we're the head, try to update head pointer
    if (callback->prev_ == nullptr) {
      if (callbacks_.compare_exchange_strong(
              expected, callback->next_,
              std::memory_order_release,
              std::memory_order_relaxed)) {
        // Successfully removed from head
        if (callback->next_) {
          callback->next_->prev_ = nullptr;
        }
        return;
      }
    }

    // Not head, or CAS failed - do manual removal
    if (callback->prev_) {
      callback->prev_->next_ = callback->next_;
    }
    if (callback->next_) {
      callback->next_->prev_ = callback->prev_;
    }
  }
};

// Type alias for consistency with P2300
using InplaceStopToken = InplaceStopSource::Token;

// InplaceStopCallback - RAII callback registration
//
// Registers a callback with a stop source that will be invoked when stop is
// requested. The callback is automatically unregistered when the InplaceStopCallback
// is destroyed.
//
// Thread-safe: multiple callbacks can be registered concurrently, and request_stop()
// can be called from any thread.
//
// IMPORTANT: The callback may be invoked:
// 1. In the thread calling request_stop() (if registered before stop)
// 2. In the constructor thread (if stop already requested when constructed)
// 3. Never (if NeverStopToken or callback destroyed before stop)
//
// Example:
//   InplaceStopSource source;
//   auto token = source.get_token();
//
//   {
//     InplaceStopCallback callback{token, [] {
//       std::println("Stop requested!");
//     }};
//
//     source.request_stop();  // Callback invoked here
//   }  // Callback unregistered (won't be called again)
//
template <typename Callback>
class InplaceStopCallback : private InplaceStopCallbackBase {
 public:
  // Construct and register callback with stop token
  template <typename C>
    requires std::constructible_from<Callback, C>
  explicit InplaceStopCallback(InplaceStopToken token, C&& callback) noexcept(
      std::is_nothrow_constructible_v<Callback, C>)
      : callback_(std::forward<C>(callback)) {
    auto* source = token.get_source();

    if (!source) {
      // NeverStopToken - callback will never be invoked
      return;
    }

    source_ = source;

    // Try to register with source
    if (!source->register_callback(this)) {
      // Stop already requested (or race with request_stop)
      // Check if callback was already invoked by request_stop during registration race
      if (!callback_completed_.load(std::memory_order_acquire)) {
        // Not yet invoked - invoke it now in this thread
        execute();
        callback_completed_.store(true, std::memory_order_release);
      }
    }
  }

  // Non-copyable, non-movable (stored inline in operation state)
  InplaceStopCallback(const InplaceStopCallback&) = delete;
  auto operator=(const InplaceStopCallback&) -> InplaceStopCallback& = delete;
  InplaceStopCallback(InplaceStopCallback&&) = delete;
  auto operator=(InplaceStopCallback&&) -> InplaceStopCallback& = delete;

  ~InplaceStopCallback() {
    if (source_) {
      source_->unregister_callback(this);
    }
  }

 private:
  void execute() noexcept override {
    // Invoke the callback
    callback_();
  }

  Callback callback_;
  InplaceStopSource* source_{nullptr};
};

// Deduction guide
template <typename Callback>
InplaceStopCallback(InplaceStopToken, Callback)
    -> InplaceStopCallback<Callback>;

static_assert(StopToken<NeverStopToken>);
static_assert(StopToken<InplaceStopToken>);

}  // namespace tempura
