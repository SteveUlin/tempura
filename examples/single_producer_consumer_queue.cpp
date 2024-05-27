#include "profiler.h"

#include <mutex>
#include <thread>

// Single producer, single consumer queue using a mutex.
void runNaive(size_t N = 1'000'000) {
  std::queue<size_t> queue;
  std::mutex mut;
  std::atomic<bool> done = false;
  TEMPURA_TRACE("runNaieve");
  std::thread producer{[&] {
    for (size_t i = 0; i < N; i++) {
      std::lock_guard lock{mut};
      queue.push(i);
    }
    done = true;
  }};
  std::thread consumer{[&] {
    while (true) {
      std::lock_guard lock{mut};
      if (!queue.empty()) {
        queue.pop();
      } else if (done) {
        return;
      }
    }
  }};
  producer.join();
  consumer.join();
}

template <size_t N>
auto repeat(std::invocable auto func) -> void {
  for (size_t i = 0; i < N; i++) {
    func();
  }
}

auto main() -> int {
  tempura::Profiler::beginTracing();
  repeat<10>([] { runNaive(); });
  tempura::Profiler::endAndPrintStats();
}
