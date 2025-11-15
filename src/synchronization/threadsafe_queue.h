#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <utility>

namespace tempura {

// ThreadSafeQueue is a simple thread-safe queue implementation using condition
// variables. It's a dumb container with no lifecycle management - users are
// responsible for coordinating shutdown via sentinel values or external flags.
template <typename T>
class ThreadSafeQueue {
 public:
  ThreadSafeQueue() = default;

  // Disallow copy and move (synchronization points shouldn't move)
  ThreadSafeQueue(const ThreadSafeQueue&) = delete;
  ThreadSafeQueue(ThreadSafeQueue&&) = delete;
  auto operator=(const ThreadSafeQueue&) -> ThreadSafeQueue& = delete;
  auto operator=(ThreadSafeQueue&&) -> ThreadSafeQueue& = delete;

  // Push a value onto the queue
  // Accepts both lvalues (copied) and rvalues (moved)
  template <typename U>
  requires std::convertible_to<U, T>
  void push(U&& value) {
    std::scoped_lock lock{mutex_};
    queue_.push(std::forward<U>(value));
    cond_var_.notify_one();
  }

  // Wait for and pop a value from the queue (blocking)
  // Blocks until an item is available
  void waitAndPop(T& value) {
    std::unique_lock lock{mutex_};
    cond_var_.wait(lock, [this] { return !queue_.empty(); });
    value = std::move(queue_.front());
    queue_.pop();
  }

  // Try to pop a value without blocking
  // Returns false if the queue is empty
  auto tryPop(T& value) -> bool {
    std::scoped_lock lock{mutex_};
    if (queue_.empty()) {
      return false;
    }
    value = std::move(queue_.front());
    queue_.pop();
    return true;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable cond_var_;
  std::queue<T> queue_;
};
} // namespace tempura
