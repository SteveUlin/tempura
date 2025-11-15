#pragma once

#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace tempura {

class ThreadPool {
 public:
  ThreadPool(size_t num_threads);

  // Will automatically join all threads when the ThreadPool is destroyed.
  //
  // This will block until all tasks are completed.
  ~ThreadPool();

  // Add a task to the thread pool
  template <typename F, typename... Args>
  auto enqueue(F&& f, Args&&... args) {
    using ReturnType = std::result_of_t<F(Args...)>;

    assert(!stop_ && "Cannot enqueue tasks while the threadpool is stopped");

    auto task = std::packaged_task<ReturnType()>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<ReturnType> res = task.get_future();

    {
      std::scoped_lock lock{mutex_};
      tasks_.emplace([task = std::move(task)] mutable { task(); });
    }
    condition_.notify_one();

    return res;
  }

  void mainThreadExecute();

  void stop();

 private:
  std::vector<std::thread> workers_;
  std::queue<std::move_only_function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stop_ = false;
};

}  // namespace tempura
