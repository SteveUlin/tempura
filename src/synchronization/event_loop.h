#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace tempura {

// EventLoop provides a single-threaded task execution queue
//
// Multiple threads can concurrently post tasks to the EventLoop, which are
// then executed serially by a single worker thread. The EventLoop does not
// own the worker thread; instead, it provides a run() method that the worker
// thread calls to process tasks.
//
// Shutdown semantics:
// - stop() prevents new tasks from being posted
// - run() drains all pending tasks before returning
// - Tasks posted before stop() completes are guaranteed to execute
//
// Example usage:
//   EventLoop loop;
//   std::thread worker([&loop] { loop.run(); });
//   loop.post([] { std::cout << "Task 1\n"; });
//   loop.post([] { std::cout << "Task 2\n"; });
//   loop.stop();  // Drains Task 1 and Task 2 before returning
//   worker.join();
//
class EventLoop {
 public:
  using Task = std::move_only_function<void()>;

  EventLoop() = default;

  // Disallow copy and move (event loops are synchronization points)
  EventLoop(const EventLoop&) = delete;
  EventLoop(EventLoop&&) = delete;
  auto operator=(const EventLoop&) -> EventLoop& = delete;
  auto operator=(EventLoop&&) -> EventLoop& = delete;

  // Post a task to the event loop
  // Returns false if the event loop has been stopped (rejects the task)
  auto post(Task task) -> bool {
    std::scoped_lock lock{mutex_};
    if (stopped_) {
      return false;  // Reject new tasks after stop
    }
    queue_.push(std::move(task));
    cond_var_.notify_one();
    return true;
  }

  // Run the event loop (blocking call for worker thread)
  // Processes all tasks from the queue until stop() is called
  // Drains all pending tasks before returning (FIFO order guaranteed)
  // PRECONDITION:
  //   - Must not be called concurrently from multiple threads
  void run() {
    while (true) {
      Task task;
      {
        std::unique_lock lock{mutex_};
        cond_var_.wait(lock, [this] { return stopped_ || !queue_.empty(); });

        // Exit only when stopped AND queue is fully drained
        if (stopped_ && queue_.empty()) {
          break;
        }

        task = std::move(queue_.front());
        queue_.pop();
      }
      task();
    }
  }

  // Signal the event loop to stop
  // Prevents new tasks from being posted and signals the worker to exit
  // run() will drain all previously-posted tasks before returning
  void stop() {
    std::scoped_lock lock{mutex_};
    stopped_ = true;
    cond_var_.notify_one();
  }

 private:
  std::queue<Task> queue_;
  std::mutex mutex_;
  std::condition_variable cond_var_;
  bool stopped_{false};
};

}  // namespace tempura
