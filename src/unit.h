#pragma once

#include <functional>
#include <iostream>
#include <source_location>
#include <string_view>

namespace tempura {

struct Test;

// TestRegistry stores global data about the currently running test and
// metadata about the entire test suite.
//
// Unit test binaries should indicate success or failure by returning 
// TestRegistry::result().
class TestRegistry final {
public:
  static void setCurrent(Test& test) {
    instance.current_ = &test;
    instance.current_success_ = true;
  }

  static void setFailure() {
    if (instance.current_success_) {
      ++instance.total_failures_;
    }
    instance.current_success_ = false;
  }

  [[nodiscard]] static bool result() {
    return instance.total_failures_ > 0;
  }

private:
  static TestRegistry& instance;

  Test* current_;
  bool current_success_;

  std::size_t total_failures_ = 0;
};

struct Test {
  std::string_view name = {};

  auto operator=(std::function<void()>& func) -> void {
    std::clog << "Running... " << name << '\n';
    func();
  }
};

[[nodiscard]] constexpr Test operator""_test(
  const char* name, std::size_t size) {
  return Test{ .name = {name, size} };
}


template <class TLhs, class TRhs>
class expectEq final {
public:
  constexpr expectEq(
      const TLhs& lhs, const TRhs& rhs,
      std::source_location location = std::source_location::current())
      : lhs_(lhs), rhs_(rhs), location_(location) {}

  ~expectEq() {
    if (not *this) {
      TestRegistry::setFailure();
      std::cerr << location_.file_name() << ":"
            << location_.line() << ":"
            << "error: "
            << "Expected " << lhs_ << " == " << rhs_ << '\n';
    }
  }

  [[nodiscard]] constexpr operator bool() const {
    return lhs_ == rhs_;
  }

private:
  const TLhs& lhs_;
  const TRhs& rhs_;
  std::source_location location_;
};

}
