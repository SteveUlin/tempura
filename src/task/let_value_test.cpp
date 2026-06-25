// Tests for letValue / letError — async chaining on the value and error channels.

#include "task/task.h"
#include "task/test_helpers.h"

#include <atomic>
#include <system_error>
#include <tuple>

#include "unit.h"

using namespace tempura;

inline constexpr int kPoison = -999999;

// A source whose completion value lives in its op-state and is OVERWRITTEN on
// destruction. If letValue/letError reads the forwarded arg after tearing down
// the source op-state, it observes kPoison — making the dangling-reference bug
// deterministic at any optimization level, not just under ASan/-O1.
class PoisonValueSender {
 public:
  explicit PoisonValueSender(int v) : value_{v} {}
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(int), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r, int v) : receiver_{std::move(r)}, value_{v} {}
      ~OpState() { value_ = kPoison; }
      void start() noexcept { receiver_.setValue(value_); }  // ref into op-state storage
     private:
      R receiver_;
      int value_;
    };
    return OpState{std::forward<R>(receiver), value_};
  }

 private:
  int value_;
};

// Error-channel twin of PoisonValueSender: the error lives in op-state and is
// poisoned on destruction. Guards the letError invoke-before-destruct fix.
class PoisonErrorSender {
 public:
  explicit PoisonErrorSender(int e) : error_{e} {}
  template <typename Env = EmptyEnv>
  using CompletionSignatures =
      tempura::CompletionSignatures<SetValueTag(int), SetErrorTag(int), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r, int e) : receiver_{std::move(r)}, error_{e} {}
      ~OpState() { error_ = kPoison; }
      void start() noexcept { receiver_.setError(error_); }  // ref into op-state storage
     private:
      R receiver_;
      int error_;
    };
    return OpState{std::forward<R>(receiver), error_};
  }

 private:
  int error_;
};

// Completes with a VALUE but declares an error channel, so letError applies to
// it yet the value bypasses the recovery function.
class ValueWithErrorChannelSender {
 public:
  template <typename Env = EmptyEnv>
  using CompletionSignatures = tempura::CompletionSignatures<
      SetValueTag(int), SetErrorTag(std::error_code), SetStoppedTag()>;

  template <typename R>
  auto connect(R&& receiver) && {
    class OpState {
     public:
      OpState(R r) : receiver_{std::move(r)} {}
      void start() noexcept { receiver_.setValue(123); }
     private:
      R receiver_;
    };
    return OpState{std::forward<R>(receiver)};
  }
};

auto main() -> int {
  "letValue - forwards the value and chains the inner sender"_test = [] {
    int seen = 0;
    auto s = letValue(just(21), [&](int x) {
      seen = x;
      return just(x * 2);
    });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(seen, 21);
    expectEq(std::get<0>(*r), 42);
  };

  "letValue - reads the value before the source op-state is destroyed"_test = [] {
    int seen = kPoison;
    auto s = letValue(PoisonValueSender{77}, [&](int x) {
      seen = x;
      return just(0);
    });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(seen, 77);  // kPoison ⇒ the value dangled past source teardown
  };

  "letValue - chains across an async scheduler boundary"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};
    std::atomic<int> seen{0};
    auto s = on(scheduler, just(9)) | letValue([&](int x) {
               seen.store(x);
               return just(x + 1);
             });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(seen.load(), 9);
    expectEq(std::get<0>(*r), 10);
  };

  "letValue - an error in the source bypasses the function"_test = [] {
    bool called = false;
    auto s = letValue(ErrorSenderTest{}, [&](int x) {
      called = true;
      return just(x);
    });
    auto r = syncWait(std::move(s));
    expectFalse(r.has_value());  // error channel → empty optional
    expectFalse(called);         // the value function never ran
  };

  "letError - recovers an error into a value"_test = [] {
    int seen = 0;
    auto s = letError(ErrorSenderTest{}, [&](std::error_code ec) {
      seen = ec.value();
      return just(99);
    });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(std::get<0>(*r), 99);
    expectTrue(seen != 0);  // saw the real error code
  };

  "letError - reads the error before the source op-state is destroyed"_test = [] {
    int seen = kPoison;
    auto s = letError(PoisonErrorSender{404}, [&](int e) {
      seen = e;
      return just(0);
    });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(seen, 404);  // kPoison ⇒ the error dangled past source teardown
  };

  "letError - recovers across an async scheduler boundary"_test = [] {
    ThreadPool pool{2};
    ThreadPoolScheduler scheduler{pool};
    std::atomic<int> seen{0};
    auto s = on(scheduler, ErrorSenderTest{}) | letError([&](std::error_code ec) {
               seen.store(ec.value());
               return just(7);
             });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(std::get<0>(*r), 7);
    expectTrue(seen.load() != 0);
  };

  "letError - a value in the source bypasses the function"_test = [] {
    bool called = false;
    auto s = letError(ValueWithErrorChannelSender{}, [&](std::error_code) {
      called = true;
      return just(-1);
    });
    auto r = syncWait(std::move(s));
    if (!expectTrue(r.has_value())) return;
    expectEq(std::get<0>(*r), 123);  // value passed straight through
    expectFalse(called);
  };

  return TestRegistry::result();
}
