#include "task/stop_token.h"
#include "unit.h"

#include <thread>

using namespace tempura;

auto main() -> int {
  // ═══════════════════════════════════════════════════════════════════════════
  // NeverStopToken tests
  // ═══════════════════════════════════════════════════════════════════════════

  "NeverStopToken - never stops"_test = [] {
  NeverStopToken token;
  expectFalse(token.stop_requested());
  expectFalse(token.stop_possible());
};

"NeverStopToken - is constexpr"_test = [] {
  constexpr NeverStopToken token;
  static_assert(!token.stop_requested());
  static_assert(!token.stop_possible());
};

"NeverStopToken - equality"_test = [] {
  NeverStopToken token1;
  NeverStopToken token2;
  expectTrue(token1 == token2);
};

"NeverStopToken - satisfies StopToken concept"_test = [] {
  static_assert(StopToken<NeverStopToken>);
};

// ═══════════════════════════════════════════════════════════════════════════
// InplaceStopSource tests
// ═══════════════════════════════════════════════════════════════════════════

"InplaceStopSource - initial state not stopped"_test = [] {
  InplaceStopSource source;
  expectFalse(source.stop_requested());

  auto token = source.get_token();
  expectFalse(token.stop_requested());
  expectTrue(token.stop_possible());
};

"InplaceStopSource - request_stop changes state"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  expectFalse(token.stop_requested());

  bool first_request = source.request_stop();
  expectTrue(first_request);  // First call returns true
  expectTrue(source.stop_requested());
  expectTrue(token.stop_requested());
};

"InplaceStopSource - request_stop idempotent"_test = [] {
  InplaceStopSource source;

  bool first = source.request_stop();
  expectTrue(first);

  bool second = source.request_stop();
  expectFalse(second);  // Already stopped

  expectTrue(source.stop_requested());
};

"InplaceStopSource - multiple tokens share state"_test = [] {
  InplaceStopSource source;
  auto token1 = source.get_token();
  auto token2 = source.get_token();

  expectFalse(token1.stop_requested());
  expectFalse(token2.stop_requested());

  source.request_stop();

  expectTrue(token1.stop_requested());
  expectTrue(token2.stop_requested());
};

"InplaceStopSource - default token not stoppable"_test = [] {
  InplaceStopToken token;  // Default constructed
  expectFalse(token.stop_possible());
  expectFalse(token.stop_requested());
};

"InplaceStopToken - equality"_test = [] {
  InplaceStopSource source1;
  InplaceStopSource source2;

  auto token1a = source1.get_token();
  auto token1b = source1.get_token();
  auto token2 = source2.get_token();

  expectTrue(token1a == token1b);  // Same source
  expectFalse(token1a == token2);  // Different source
};

"InplaceStopToken - satisfies StopToken concept"_test = [] {
  static_assert(StopToken<InplaceStopToken>);
};

// ═══════════════════════════════════════════════════════════════════════════
// Thread safety tests
// ═══════════════════════════════════════════════════════════════════════════

"InplaceStopSource - thread safe request_stop"_test = [] {
  InplaceStopSource source;

  std::atomic<int> first_count{0};
  constexpr int kThreadCount = 10;

  std::thread threads[kThreadCount];
  for (int i = 0; i < kThreadCount; ++i) {
    threads[i] = std::thread([&source, &first_count] {
      if (source.request_stop()) {
        first_count.fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // Exactly one thread should have been first
  expectEq(first_count.load(), 1);
  expectTrue(source.stop_requested());
};

"InplaceStopSource - thread safe token observation"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  std::atomic<bool> stop_observed{false};

  // Thread 1: observes token
  std::thread observer([&token, &stop_observed] {
    while (!token.stop_requested()) {
      std::this_thread::yield();
    }
    stop_observed.store(true, std::memory_order_release);
  });

  // Thread 2: requests stop after small delay
  std::thread stopper([&source] {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    source.request_stop();
  });

  observer.join();
  stopper.join();

  expectTrue(stop_observed.load(std::memory_order_acquire));
};

// ═══════════════════════════════════════════════════════════════════════════
// Compile-time tests
// ═══════════════════════════════════════════════════════════════════════════

"compile time - InplaceStopSource is not copyable"_test = [] {
  static_assert(!std::is_copy_constructible_v<InplaceStopSource>);
  static_assert(!std::is_copy_assignable_v<InplaceStopSource>);
};

"compile time - InplaceStopSource is not movable"_test = [] {
  static_assert(!std::is_move_constructible_v<InplaceStopSource>);
  static_assert(!std::is_move_assignable_v<InplaceStopSource>);
};

"compile time - InplaceStopToken is copyable"_test = [] {
  static_assert(std::is_copy_constructible_v<InplaceStopToken>);
  static_assert(std::is_copy_assignable_v<InplaceStopToken>);
};

"compile time - InplaceStopToken is movable"_test = [] {
  static_assert(std::is_move_constructible_v<InplaceStopToken>);
  static_assert(std::is_move_assignable_v<InplaceStopToken>);
};

"compile time - NeverStopToken is trivial"_test = [] {
  static_assert(std::is_trivially_copyable_v<NeverStopToken>);
  static_assert(std::is_trivially_destructible_v<NeverStopToken>);
};

  return TestRegistry::result();
}
