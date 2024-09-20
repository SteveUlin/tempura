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

  [[nodiscard]] static auto result() -> int {
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

template <class TLhs, class TRhs>
class expectEq final {
 public:
  constexpr expectEq(
      const TLhs& lhs, const TRhs& rhs,
      std::source_location location = std::source_location::current())
      : lhs_(lhs), rhs_(rhs), location_(location) {}

  ~expectEq() {
    if (not *this) {
      std::cerr << "Error at" << location_.file_name() << ":"
                << location_.line() << std::endl;
      if constexpr (Ostreamable<TLhs> and Ostreamable<TRhs>) {
        std::cerr << "Expected: " << lhs_ << " got: " << rhs_ << std::endl;
      }
      TestRegistry::setFailure();
    }
  }

  [[nodiscard]] constexpr operator bool() const { return lhs_ == rhs_; }

 private:
  const TLhs& lhs_;
  const TRhs& rhs_;
  std::source_location location_;
};

template <class TLhs, class TRhs>
class expectNeq final {
 public:
  constexpr expectNeq(
      const TLhs& lhs, const TRhs& rhs,
      std::source_location location = std::source_location::current())
      : lhs_(lhs), rhs_(rhs), location_(location) {}

  ~expectNeq() {
    if (not *this) {
      std::cerr << location_.file_name() << ":" << location_.line();
      TestRegistry::setFailure();
    }
  }

  [[nodiscard]] constexpr operator bool() const { return lhs_ != rhs_; }

 private:
  const TLhs& lhs_;
  const TRhs& rhs_;
  std::source_location location_;
};

class expectTrue final {
 public:
  constexpr expectTrue(bool value) : value_(value) {}

  ~expectTrue() {
    if (not *this) {
      std::cerr << location_.file_name() << ":" << location_.line();
      TestRegistry::setFailure();
    }
  }

  [[nodiscard]] constexpr operator bool() const { return value_; }

 private:
  bool value_;
  std::source_location location_;
};

class expectFalse final {
 public:
  constexpr expectFalse(bool value) : value_(value) {}

  ~expectFalse() {
    if (not *this) {
      std::cerr << location_.file_name() << ":" << location_.line();
      TestRegistry::setFailure();
    }
  }

  [[nodiscard]] constexpr operator bool() const { return value_; }

 private:
  bool value_;
  std::source_location location_;
};

template <class TLhs, class TRhs>
class expectApproxEq final {
 public:
  constexpr expectApproxEq(
      const TLhs& lhs, const TRhs& rhs,
      std::source_location location = std::source_location::current())
      : lhs_(lhs), rhs_(rhs), location_(location) {}

  ~expectApproxEq() {
    if (not *this) {
      std::cerr << location_.file_name() << ":" << location_.line();
      TestRegistry::setFailure();
    }
  }

  [[nodiscard]] constexpr operator bool() const { return lhs_ - rhs_ < 0.0001; }

 private:
  const TLhs& lhs_;
  const TRhs& rhs_;
  std::source_location location_;
};

}  // namespace tempura
