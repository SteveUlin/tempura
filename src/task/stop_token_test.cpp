#include "task/stop_token.h"
#include "unit.h"

#include <latch>
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

// ═══════════════════════════════════════════════════════════════════════════
// InplaceStopCallback tests
// ═══════════════════════════════════════════════════════════════════════════

"InplaceStopCallback - basic invocation"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  bool callback_invoked = false;

  {
    InplaceStopCallback callback{token, [&] {
      callback_invoked = true;
    }};

    expectFalse(callback_invoked);

    source.request_stop();

    expectTrue(callback_invoked);
  }
};

"InplaceStopCallback - invoked immediately if already stopped"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  source.request_stop();  // Stop first

  bool callback_invoked = false;

  {
    InplaceStopCallback callback{token, [&] {
      callback_invoked = true;
    }};

    // Should be invoked immediately in constructor
    expectTrue(callback_invoked);
  }
};

"InplaceStopCallback - multiple callbacks"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  int invocation_count = 0;

  {
    InplaceStopCallback callback1{token, [&] { invocation_count++; }};
    InplaceStopCallback callback2{token, [&] { invocation_count++; }};
    InplaceStopCallback callback3{token, [&] { invocation_count++; }};

    source.request_stop();

    expectEq(invocation_count, 3);
  }
};

"InplaceStopCallback - not invoked if destroyed before stop"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  bool callback_invoked = false;

  {
    InplaceStopCallback callback{token, [&] {
      callback_invoked = true;
    }};
  }  // Callback destroyed here

  source.request_stop();

  expectFalse(callback_invoked);  // Should not be invoked
};

"InplaceStopCallback - only invoked once"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  int invocation_count = 0;

  {
    InplaceStopCallback callback{token, [&] { invocation_count++; }};

    source.request_stop();
    source.request_stop();  // Second call
    source.request_stop();  // Third call

    expectEq(invocation_count, 1);  // Only invoked once
  }
};

"InplaceStopCallback - works with NeverStopToken"_test = [] {
  NeverStopToken token;

  bool callback_invoked = false;

  // Should compile but never invoke callback
  InplaceStopCallback callback{InplaceStopToken{}, [&] {
    callback_invoked = true;
  }};

  expectFalse(callback_invoked);
};

"InplaceStopCallback - thread safe registration"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  std::atomic<int> callback_count{0};
  std::latch registered{2};
  std::latch stop_done{1};

  // Thread 1: Register callback, signal registration, wait for stop
  std::thread registrar([&] {
    InplaceStopCallback callback{token, [&] { callback_count++; }};
    registered.count_down();
    stop_done.wait();  // Wait for stop to complete
  });

  // Thread 2: Register callback, signal registration, wait for stop
  std::thread registrar2([&] {
    InplaceStopCallback callback{token, [&] { callback_count++; }};
    registered.count_down();
    stop_done.wait();  // Wait for stop to complete
  });

  // Wait for both callbacks to be registered
  registered.wait();

  // Now request stop while both callbacks are still registered
  source.request_stop();

  // Signal threads they can exit
  stop_done.count_down();

  registrar.join();
  registrar2.join();

  // Both callbacks should have been invoked
  expectEq(callback_count.load(), 2);
};

"InplaceStopCallback - race between registration and stop"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  std::atomic<int> callback_count{0};
  std::atomic<bool> start{false};

  // Thread 1: Register callback when start flag is set
  std::thread registrar([&] {
    while (!start.load(std::memory_order_acquire)) {
      // Wait for start signal
    }
    InplaceStopCallback callback{token, [&] { callback_count++; }};
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  });

  // Thread 2: Request stop when start flag is set
  std::thread stopper([&] {
    while (!start.load(std::memory_order_acquire)) {
      // Wait for start signal
    }
    source.request_stop();
  });

  // Start both threads simultaneously
  start.store(true, std::memory_order_release);

  registrar.join();
  stopper.join();

  // Callback should be invoked exactly once (either during registration or during stop)
  expectEq(callback_count.load(), 1);
};

"InplaceStopCallback - captures by value"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  int value = 42;
  bool callback_invoked = false;
  int captured_value = 0;

  {
    InplaceStopCallback callback{token, [value, &callback_invoked, &captured_value] {
      callback_invoked = true;
      captured_value = value;
    }};

    value = 99;  // Change original

    source.request_stop();

    expectTrue(callback_invoked);
    expectEq(captured_value, 42);  // Should have captured original value
  }
};

"InplaceStopCallback - can modify captured references"_test = [] {
  InplaceStopSource source;
  auto token = source.get_token();

  int counter = 0;

  {
    InplaceStopCallback callback{token, [&counter] {
      counter++;
    }};

    source.request_stop();

    expectEq(counter, 1);
  }
};

"compile time - InplaceStopCallback is not copyable"_test = [] {
  using CallbackType = InplaceStopCallback<decltype([]{}


)>;
  static_assert(!std::is_copy_constructible_v<CallbackType>);
  static_assert(!std::is_copy_assignable_v<CallbackType>);
};

"compile time - InplaceStopCallback is not movable"_test = [] {
  using CallbackType = InplaceStopCallback<decltype([]{}
)>;
  static_assert(!std::is_move_constructible_v<CallbackType>);
  static_assert(!std::is_move_assignable_v<CallbackType>);
};

  return TestRegistry::result();
}
