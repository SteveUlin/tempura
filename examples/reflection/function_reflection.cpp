// Function Reflection with C++26
//
// Demonstrates reflection on functions and their parameters:
// - meta::parameters_of - get function parameters
// - meta::return_type_of - get return type
// - meta::is_noexcept - check noexcept specification
// - Generating wrapper functions automatically

#include "meta.h"

#include <print>
#include <string_view>
#include <tuple>

namespace meta = std::meta;

// ============================================================================
// Basic function introspection
// ============================================================================

template <auto Fn>
consteval auto functionName() -> std::string_view {
  return meta::name_of(^^Fn);
}

template <auto Fn>
consteval auto parameterCount() -> std::size_t {
  return meta::parameters_of(^^Fn).size();
}

template <auto Fn>
void printFunctionSignature() {
  constexpr auto fn_reflection = ^^Fn;

  std::print("{}(", meta::name_of(fn_reflection));

  bool first = true;
  template for (constexpr auto param : meta::parameters_of(fn_reflection)) {
    if (!first) std::print(", ");
    first = false;
    std::print("{}", meta::name_of(meta::type_of(param)));
  }

  std::print(") -> {}", meta::name_of(meta::return_type_of(fn_reflection)));

  if (meta::is_noexcept(fn_reflection)) {
    std::print(" noexcept");
  }
  std::println("");
}

// ============================================================================
// Print parameter names and types
// ============================================================================

template <auto Fn>
void describeFunction() {
  constexpr auto fn_reflection = ^^Fn;

  std::println("Function: {}", meta::name_of(fn_reflection));
  std::println("  Return type: {}",
               meta::name_of(meta::return_type_of(fn_reflection)));
  std::println("  Parameters:");

  int idx = 0;
  template for (constexpr auto param : meta::parameters_of(fn_reflection)) {
    // Note: parameter names may be empty if not in source
    auto param_name = meta::name_of(param);
    if (param_name.empty()) {
      std::println("    [{}]: {}", idx, meta::name_of(meta::type_of(param)));
    } else {
      std::println("    [{}] {} : {}", idx, param_name,
                   meta::name_of(meta::type_of(param)));
    }
    ++idx;
  }
}

// ============================================================================
// Create a logging wrapper for any function
// ============================================================================

template <auto Fn>
struct LoggingWrapper {
  template <typename... Args>
  static auto call(Args&&... args) {
    std::print("[LOG] Calling {}(", meta::name_of(^^Fn));

    // Print arguments (simplified - just count them)
    std::print("{} args", sizeof...(Args));
    std::println(")");

    if constexpr (std::is_void_v<decltype(Fn(std::forward<Args>(args)...))>) {
      Fn(std::forward<Args>(args)...);
      std::println("[LOG] {} returned void", meta::name_of(^^Fn));
    } else {
      auto result = Fn(std::forward<Args>(args)...);
      std::println("[LOG] {} returned", meta::name_of(^^Fn));
      return result;
    }
  }
};

// ============================================================================
// Example functions to reflect upon
// ============================================================================

int add(int a, int b) { return a + b; }

double divide(double numerator, double denominator) {
  return numerator / denominator;
}

void greet(std::string_view name) { std::println("Hello, {}!", name); }

int noThrowFunction(int x) noexcept { return x * 2; }

// ============================================================================
// Class member functions
// ============================================================================

class Calculator {
 public:
  int multiply(int a, int b) const { return a * b; }
  void reset() { value_ = 0; }
  int getValue() const noexcept { return value_; }

 private:
  int value_ = 0;
};

template <typename Class>
void printMemberFunctions() {
  std::println("Member functions of {}:", meta::name_of(^^Class));

  template for (constexpr auto member : meta::members_of(^^Class)) {
    if (meta::is_function(member) && meta::is_public(member)) {
      std::print("  {}(", meta::name_of(member));

      bool first = true;
      template for (constexpr auto param : meta::parameters_of(member)) {
        if (!first) std::print(", ");
        first = false;
        std::print("{}", meta::name_of(meta::type_of(param)));
      }

      std::print(") -> {}", meta::name_of(meta::return_type_of(member)));

      if (meta::is_const(member)) std::print(" const");
      if (meta::is_noexcept(member)) std::print(" noexcept");
      std::println("");
    }
  }
}

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("=== Function Reflection ===\n");

  // Basic introspection
  std::println("Function signatures:");
  printFunctionSignature<add>();
  printFunctionSignature<divide>();
  printFunctionSignature<greet>();
  printFunctionSignature<noThrowFunction>();

  // Detailed description
  std::println("\nDetailed function info:");
  describeFunction<add>();
  std::println("");
  describeFunction<divide>();

  // Logging wrapper
  std::println("\nLogging wrapper demo:");
  auto result = LoggingWrapper<add>::call(3, 4);
  std::println("Result: {}", result);

  LoggingWrapper<greet>::call("World");

  // Class member functions
  std::println("\nClass member reflection:");
  printMemberFunctions<Calculator>();

  // Compile-time checks
  static_assert(parameterCount<add>() == 2);
  static_assert(parameterCount<greet>() == 1);
  std::println("\nAll static_asserts passed!");

  return 0;
}
