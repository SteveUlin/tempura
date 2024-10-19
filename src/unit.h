#pragma once

#include <functional>
#include <iostream>
#include <source_location>
#include <string_view>

namespace tempura {

template <typename T>
concept Ostreamable = requires(T t) { std::cout << t; };

class Test;

// TestRegistry stores global data about the currently running test and
// metadata about the entire test suite.
//
// Unit test binaries should indicate success or failure by returning
// TestRegistry::result().
class TestRegistry final {
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

  Test* current_;
  bool current_success_;

  std::size_t total_failures_ = 0;
};

class Test {
 public:
  auto operator=(const std::function<void()>& func) -> void {
    TestRegistry::setCurrent(*this);
    std::clog << "Running... " << name_ << '\n';
    func();
  }

 private:
  constexpr Test(std::string_view name) : name_(name) {}

  friend constexpr auto operator""_test(const char*, std::size_t) -> Test;

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

template <double delta = 0.0001>
auto expectNear(const auto& lhs, const auto& rhs,
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

auto expectLessThan(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs <= rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected: " << lhs << " less than: " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

auto expectGreaterThan(const auto& lhs, const auto& rhs,
              const std::source_location location =
                  std::source_location::current()) -> bool {
  if (lhs >= rhs) {
    return true;
  }
  std::cerr << "Error at" << location.file_name() << ":" << location.line()
            << std::endl;
  if constexpr (Ostreamable<decltype(lhs)> and Ostreamable<decltype(rhs)>) {
    std::cerr << "Expected: " << lhs << " greater than: " << rhs << std::endl;
  }
  TestRegistry::setFailure();
  return false;
}

}  // namespace tempura
