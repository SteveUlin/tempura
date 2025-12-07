// Tests for sender/receiver concepts

#include "task/task.h"
#include "task/test_helpers.h"

#include <string>
#include <tuple>
#include <type_traits>

#include "unit.h"

using namespace tempura;

// Types without ValueTypes should not satisfy Sender
struct NotASender {
  // Missing: using ValueTypes = ...;
};

auto main() -> int {
  // ==========================================================================
  // Sender Concept Validation
  // ==========================================================================

  "Sender concept - JustSender"_test = [] {
    static_assert(Sender<JustSender<int>>);
    static_assert(Sender<decltype(just(42))>);
  };

  "Sender concept - composed senders"_test = [] {
    static_assert(Sender<decltype(just(42) | then([](int x) { return x * 2; }))>);
    static_assert(Sender<decltype(just(42) | letValue([](int x) { return just(x * 2); }))>);
    static_assert(Sender<decltype(CustomErrorSender1{} | letError([](std::tuple<std::string, int> err) { return just(0); }))>);
  };

  "SenderTo concept - basic validation"_test = [] {
    static_assert(SenderTo<JustSender<int>, ValueReceiver<int>>);
    static_assert(SenderTo<decltype(just(42)), PrintReceiver<int>>);
  };

  "Sender concept - invalid types"_test = [] {
    static_assert(!Sender<NotASender>);
  };

  // ==========================================================================
  // Variadic Error Types - P2300 Symmetry
  // ==========================================================================

  "ErrorTypes - custom error senders satisfy Sender concept"_test = [] {
    static_assert(Sender<CustomErrorSender1>);
    static_assert(Sender<CustomErrorSender2>);
    static_assert(Sender<MultiErrorSender>);
  };

  "CompletionSignatures - multiple error signatures"_test = [] {
    // Custom error sender with multiple error signatures
    using MultiSigs = MultiErrorSender::CompletionSignatures<>;
    using ErrorSigs = ErrorSignaturesT<MultiSigs>;

    // Verify we have 3 error signatures
    static_assert(Size_v<ErrorSigs> == 3);
  };

  return 0;
}
