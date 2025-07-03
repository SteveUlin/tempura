#include "synchronization/threadpool.h"
#include <functional>
#include <mutex>

namespace tempura {

ThreadPool::ThreadPool(size_t num_threads) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this] {
      while (true) {
        std::move_only_function<void()> task;

        {
          std::unique_lock<std::mutex> lock(mutex_);
          condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

          if (stop_ && tasks_.empty()) {
            return;
          }

          task = std::move(tasks_.front());
          tasks_.pop();
          lock.unlock();
        }

        task();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  if (!stop_) {
    stop();
  }
}

void ThreadPool::mainThreadExecute() {
  std::move_only_function<void()> task;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    // Run on the main thread until there are no tasks left.
    if (tasks_.empty()) {
      return;
    }

    task = std::move(tasks_.front());
    tasks_.pop();
  }

  task();
}

void ThreadPool::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
  }
  condition_.notify_all();

  for (std::thread& worker : workers_) {
    worker.join();
  }
}


}  // namespace tempura
