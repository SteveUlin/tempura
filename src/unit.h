#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <ranges>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

#include "comparison.h"

namespace tempura {

template <typename T>
concept Ostreamable = requires(T t) { std::cout << t; };

// ============================================================================
// TestContext - Thread-local test execution context
// ============================================================================

class TestContext {
 public:
  TestContext() = default;

  void recordFailure(std::string message) {
    failures_.push_back(std::move(message));
    has_failure_ = true;
  }

  [[nodiscard]] auto hasFailures() const -> bool {
    return has_failure_;
  }

  [[nodiscard]] auto failures() const -> const std::vector<std::string>& {
    return failures_;
  }

  void reset() {
    failures_.clear();
    has_failure_ = false;
  }

  // RAII guard to set/unset thread-local context
  class Guard {
   public:
    explicit Guard(TestContext& ctx) : previous_(current_) {
      current_ = &ctx;
    }

    ~Guard() { current_ = previous_; }

    Guard(const Guard&) = delete;
    Guard(Guard&&) = delete;
    auto operator=(const Guard&) -> Guard& = delete;
    auto operator=(Guard&&) -> Guard& = delete;

   private:
    TestContext* previous_;
  };

  // Get the current thread-local context
  static auto current() -> TestContext* { return current_; }

 private:
  std::vector<std::string> failures_;
  bool has_failure_ = false;

  static inline thread_local TestContext* current_ = nullptr;
};

class Test;

// TestRegistry stores global data about the currently running test and
// metadata about the entire test suite.
//
// Unit test binaries should indicate success or failure by returning
// TestRegistry::result().
class TestRegistry {
 public:
  TestRegistry(const TestRegistry&) = delete;
  TestRegistry(TestRegistry&&) = delete;
  auto operator=(const TestRegistry&) -> TestRegistry& = delete;
  auto operator=(TestRegistry&&) -> TestRegistry& = delete;

  static void setCurrent(Test& test) {
    getInstance().current_ = &test;
    getInstance().current_success_ = true;
  }

  static void setFailure() {
    if (getInstance().current_success_) {
      ++getInstance().total_failures_;
    }
    getInstance().current_success_ = false;
  }

  static auto result() -> int {
    return static_cast<int>(getInstance().total_failures_);
  }

 private:
  TestRegistry() = default;

  static auto getInstance() -> TestRegistry& {
    static TestRegistry instance;
    return instance;
  }

  Test* current_ = nullptr;
  bool current_success_ = false;

  std::size_t total_failures_ = 0;
};

class Test {
 public:
  void operator=(const std::function<void()>& func) {
    TestRegistry::setCurrent(*this);
    std::clog << "Running... " << name_ << '\n';

    // Set up TestContext for this test
    TestContext ctx;
    TestContext::Guard guard(ctx);

    try {
      func();
    } catch (const std::exception& e) {
      // Unexpected exception
      std::cerr << "Unexpected exception: " << e.what() << std::endl;
      ctx.recordFailure(
          std::string("Unexpected exception: ") + e.what());
    } catch (...) {
      // Unknown exception
      std::cerr << "Unknown exception thrown" << std::endl;
      ctx.recordFailure("Unknown exception thrown");
    }

    // If there were any failures, mark the test as failed
    if (ctx.hasFailures()) {
      TestRegistry::setFailure();
    }
  }

 private:
  constexpr Test(std::string_view name) : name_(name) {}

  friend constexpr auto operator""_test(const char* /*name*/,
                                        std::size_t /*size*/) -> Test;

  std::string_view name_;
};

[[nodiscard]] constexpr auto operator""_test(const char* name,
                                             std::size_t size) -> Test {
  return Test{{name, size}};
}

auto expectEq(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs == rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected Equal: " << lhs << " got: " << rhs << std::endl;
  }

  // Record in TestContext for better reporting
  if (auto* ctx = TestContext::current()) {
    std::string msg = std::string("expectEq failed at ") + location.file_name() +
                      ":" + std::to_string(location.line());
    ctx->recordFailure(msg);
  }

  TestRegistry::setFailure();
  return false;
}

auto expectNeq(const auto& lhs, const auto& rhs,
               const std::source_location location =
                   std::source_location::current()) -> bool {
  if (lhs != rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected Not Equal: " << lhs << " got: " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectTrue(const auto& arg, const std::source_location location =
                                     std::source_location::current()) -> bool {
  if (arg) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(arg)>) {
    std::cerr << "Expected true: " << arg << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectFalse(const auto& arg, const std::source_location location =
                                      std::source_location::current()) -> bool {
  if (!arg) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(arg)>) {
    std::cerr << "Expected false: " << arg << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectLT(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs < rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected: " << lhs << " < " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectLE(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs <= rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected: " << lhs << " <= " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectGT(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs > rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected: " << lhs << " > " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectGE(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs >= rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected: " << lhs << " >= " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

// Generic delta parameter (no template instantiation per delta value)
constexpr auto expectNear(const auto& lhs, const auto& rhs,
                          const auto& delta,
                          const std::source_location location =
                              std::source_location::current()) -> bool {
  if (std::abs(lhs - rhs) < delta) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected Near ±(" << delta << "): " << lhs << " got: " << rhs
              << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

// Overload with default delta value
constexpr auto expectNear(const auto& lhs, const auto& rhs,
                          const std::source_location location =
                              std::source_location::current()) -> bool {
  return expectNear(lhs, rhs, 0.0001, location);
}

// Generic delta parameter (no template instantiation per delta value)
constexpr auto expectRangeNear(std::ranges::input_range auto&& lhs,
                               std::ranges::input_range auto&& rhs,
                               const auto& delta,
                               const std::source_location location =
                                   std::source_location::current()) -> bool {
  // Check size mismatch first if both ranges are sized
  if constexpr (std::ranges::sized_range<decltype(lhs)> &&
                std::ranges::sized_range<decltype(rhs)>) {
    auto size_lhs = std::ranges::size(lhs);
    auto size_rhs = std::ranges::size(rhs);
    if (size_lhs != size_rhs) {
      std::print(std::cerr, "Error at {}:{}\n", location.file_name(),
                 location.line());
      std::print(std::cerr, "\tRange size mismatch: {} vs {}\n", size_lhs,
                 size_rhs);
      TestRegistry::setFailure();
      return false;
    }
  }

  for (auto [l, r, idx] : std::views::zip(lhs, rhs, std::views::iota(0))) {
    if (std::abs(l - r) > delta) {
      std::print(std::cerr, "Error at {}:{}\n", location.file_name(),
                 location.line());
      std::print(std::cerr, "\tError at index {} of range\n", idx);
      if constexpr (Ostreamable<decltype(l)> and Ostreamable<decltype(r)>) {
        std::print(std::cerr, "\tExpected Near ±({}): {} got: {}\n", delta, l,
                   r);
      }
      TestRegistry::setFailure();
      return false;
    }
  }
  return true;
}

// Overload with default delta value
constexpr auto expectRangeNear(std::ranges::input_range auto&& lhs,
                               std::ranges::input_range auto&& rhs,
                               const std::source_location location =
                                   std::source_location::current()) -> bool {
  return expectRangeNear(std::forward<decltype(lhs)>(lhs),
                         std::forward<decltype(rhs)>(rhs),
                         0.0001, location);
}
constexpr auto expectRangeEq(auto&& lhs, auto&& rhs,
                   const std::source_location location =
                       std::source_location::current()) -> bool {
  // Check size mismatch first if both ranges are sized
  if constexpr (std::ranges::sized_range<decltype(lhs)> &&
                std::ranges::sized_range<decltype(rhs)>) {
    auto size_lhs = std::ranges::size(lhs);
    auto size_rhs = std::ranges::size(rhs);
    if (size_lhs != size_rhs) {
      std::print(std::cerr, "Error at {}:{}\n", location.file_name(),
                 location.line());
      std::print(std::cerr, "\tRange size mismatch: {} vs {}\n", size_lhs,
                 size_rhs);
      TestRegistry::setFailure();
      return false;
    }
  }

  for (auto [l, r, idx] : std::views::zip(lhs, rhs, std::views::iota(0))) {
    if (l != r) {
      std::print(std::cerr, "Error at {}:{}\n", location.file_name(),
                 location.line());
      std::print(std::cerr, "\tError at index {} of range\n", idx);
      if constexpr (Ostreamable<decltype(l)> and Ostreamable<decltype(r)>) {
        std::print(std::cerr, "\tExpected Equal: {} got: {}\n", l, r);
      }
      TestRegistry::setFailure();
      return false;
    }
  }
  return true;
}


}  // namespace tempura
