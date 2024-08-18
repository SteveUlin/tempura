#include "fifo_queue.h"

#include "unit.h"
#include "benchmark.h"
#include "profiler.h"

#include <thread>

using namespace tempura;

auto main() -> int {
  "Capacity is N"_test = [] {
    expectEq(16ULL, tempura::FIFOQueue<int, 16>::capacity());
  };

  "Basic Push Pop"_test = [] {
    auto queue = tempura::FIFOQueue<int, 16>{};
    expectTrue(queue.push(5));
    expectEq(5, *queue.pop());
  };

  "Push Push Push Pop Pop Pop"_test = [] {
    auto queue = tempura::FIFOQueue<int, 16>{};
    expectTrue(queue.push(1));
    expectTrue(queue.push(2));
    expectTrue(queue.push(3));


    expectEq(1, *queue.pop());
    expectEq(2, *queue.pop());
    expectEq(3, *queue.pop());
  };

  "Push Pop Push Pop Push Pop"_test = [] {
    auto queue = tempura::FIFOQueue<int, 16>{};

    expectTrue(queue.push(1));
    expectEq(1, *queue.pop());
    expectTrue{queue.push(2)};
    expectEq{2, *queue.pop()};
    expectTrue{queue.push(3)};
    expectEq(3, *queue.pop());
  };

  "Threads 1M"_test = [] {
    TEMPURA_TRACE();
    auto queue = tempura::FIFOQueue<int, 1024>{};
    auto t1 = std::thread([&] {
      for (int i = 0; i < 1'000'000; ++i) {
        while (!queue.push(i)) {}
      }
    });

    auto t2 = std::thread([&] {
      for (int i = 0; i < 1'000'000; ++i) {
        std::optional<int> val;
        val = queue.pop();
        while (!val.has_value()) {
          val = queue.pop();
        }
        expectEq(i, *val);
      }
    });
    t1.join();
    t2.join();
  };

  constexpr std::size_t N = 1'000'000;
  "Threads"_bench.ops(N) = [] {
    auto queue = tempura::FIFOQueue<int, 1024>{};
    auto t1 = std::thread([&] {
      for (std::size_t i = 0; i < N; ++i) {
        while (!queue.push(i)) {}
      }
    });

    auto t2 = std::thread([&] {
      for (std::size_t i = 0; i < N; ++i) {
        std::optional<int> val;
        val = queue.pop();
        while (!val.has_value()) {
          val = queue.pop();
        }
        expectEq(i, *val);
      }
    });
    t1.join();
    t2.join();
  };
}
