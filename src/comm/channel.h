// In-process topic — the "Live" delivery rung, and the only one that exists yet.
//
// A Channel<T> hands a shared_ptr<const T> from a publisher to a subscriber.
// Immutable + shared ⇒ many readers, zero copy, no lifetime question: publisher
// and every subscriber co-own the same bytes. That is what "Live" means, and
// why it is the one rung that needs no serializer.
//
// receive() yields a Sender (the src/task substrate) that completes with the
// next message, parking if the queue is empty. Delivery rides senders, not
// callbacks, so cancellation and composition arrive for free as this grows.
//
// INCOMPLETE BY DESIGN — each edge grows only when something forces it:
//   - one parked receiver at a time (single subscriber); a 2nd forces fan-out
//   - unbounded queue; a bound forces a real overflow error
//   - no close / cancellation; stop_token forces it

#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <utility>

#include "task/concepts.h"

namespace tempura {

template <typename T>
class Channel {
 public:
  using Item = std::shared_ptr<const T>;

  // Enqueue a message, or hand it straight to a parked receiver. Never blocks.
  void publish(Item msg) {
    Waiter* parked = nullptr;
    {
      std::lock_guard lock{mutex_};
      if (waiter_ != nullptr) {
        parked = std::exchange(waiter_, nullptr);
      } else {
        queue_.push_back(std::move(msg));
        return;
      }
    }
    // Complete OUTSIDE the lock: setValue runs subscriber code, and a receiver
    // that re-publishes would deadlock if we still held mutex_.
    parked->complete(std::move(msg));
  }

  auto receive() { return ReceiveSender{this}; }

 private:
  // Type-erased handle to a parked receive op-state, so Channel<T> need not know
  // the receiver type. The op-state derives from this and lives on the
  // subscriber's stack until completed — hence it must be non-movable.
  struct Waiter {
    virtual void complete(Item msg) noexcept = 0;

   protected:
    ~Waiter() = default;
  };

  template <typename R>
  class ReceiveOperationState : public Waiter {
   public:
    ReceiveOperationState(Channel* channel, R receiver)
        : channel_{channel}, receiver_{std::move(receiver)} {}

    ReceiveOperationState(const ReceiveOperationState&) = delete;
    auto operator=(const ReceiveOperationState&) -> ReceiveOperationState& = delete;
    ReceiveOperationState(ReceiveOperationState&&) = delete;
    auto operator=(ReceiveOperationState&&) -> ReceiveOperationState& = delete;

    void start() noexcept {
      Item msg;
      {
        std::lock_guard lock{channel_->mutex_};
        if (channel_->queue_.empty()) {
          channel_->waiter_ = this;  // park; publish() will call complete()
          return;
        }
        msg = std::move(channel_->queue_.front());
        channel_->queue_.pop_front();
      }
      std::move(receiver_).setValue(std::move(msg));  // deliver outside the lock
    }

    // Called by publish() on the producer's thread for the parked path. After
    // setValue's final count_down this op may be destroyed concurrently, so we
    // touch nothing afterward — the src/task syncWait teardown contract.
    void complete(Item msg) noexcept override {
      std::move(receiver_).setValue(std::move(msg));
    }

   private:
    Channel* channel_;
    R receiver_;
  };

  class ReceiveSender {
   public:
    // Only the value channel — the sender never stops or errors yet. A stopped
    // signature appears the day close()/cancellation does, not before.
    template <typename Env = EmptyEnv>
    using CompletionSignatures =
        tempura::CompletionSignatures<SetValueTag(Item)>;

    explicit ReceiveSender(Channel* channel) : channel_{channel} {}

    template <typename R>
    auto connect(R&& receiver) && {
      return ReceiveOperationState<std::remove_cvref_t<R>>{
          channel_, std::forward<R>(receiver)};
    }

   private:
    Channel* channel_;
  };

  std::mutex mutex_;
  std::deque<Item> queue_;
  Waiter* waiter_ = nullptr;  // at most one outstanding receive (step 1)
};

}  // namespace tempura
