// Tests for the in-process Live channel.

#include "comm/channel.h"
#include "task/task.h"

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <tuple>

#include "unit.h"

using namespace tempura;

// Minimal receiver for the deterministic parked-path test — captures the
// delivered Item without going through syncWait's blocking machinery.
template <typename T>
struct CaptureReceiver {
  std::optional<std::shared_ptr<const T>>* out;
  void setValue(std::shared_ptr<const T> msg) noexcept { *out = std::move(msg); }
  void setStopped() noexcept {}
};

auto main() -> int {
  "publish-then-receive delivers the queued message, shared not copied"_test = [] {
    Channel<int> ch;
    auto msg = std::make_shared<const int>(42);
    ch.publish(msg);

    auto r = syncWait(ch.receive());
    if (!expectTrue(r.has_value())) return;
    auto got = std::get<0>(*r);
    expectTrue(got.get() == msg.get());  // pointer-equal ⇒ borrowed, never copied
    expectEq(*got, 42);
  };

  "receive parks when empty, completes on a later publish"_test = [] {
    // Single-threaded and deterministic: start() finds the queue empty and
    // parks; the subsequent publish() hands off directly to the parked op.
    Channel<int> ch;
    std::optional<std::shared_ptr<const int>> got;
    auto op = ch.receive().connect(CaptureReceiver<int>{&got});
    op.start();
    expectFalse(got.has_value());  // parked, nothing delivered yet

    auto msg = std::make_shared<const int>(7);
    ch.publish(msg);
    if (!expectTrue(got.has_value())) return;
    expectTrue(got->get() == msg.get());
  };

  "cross-thread: a producer thread publishes, the consumer receives"_test = [] {
    // Races between the queued and parked paths run to run; both deliver, so the
    // assertion holds either way — this exercises the real thread handoff.
    Channel<std::string> ch;
    auto msg = std::make_shared<const std::string>("hello");
    std::thread producer{[&] { ch.publish(msg); }};

    auto r = syncWait(ch.receive());
    producer.join();
    if (!expectTrue(r.has_value())) return;
    expectEq(*std::get<0>(*r), std::string{"hello"});
  };

  return TestRegistry::result();
}
