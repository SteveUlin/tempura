// Automatic Enum-to-String with C++26 Reflection
//
// One of the most requested features: automatic conversion between
// enums and their string representations without macros!
//
// Key primitives:
// - meta::enumerators_of - get all enumerator values
// - meta::name_of - get enumerator name
// - meta::value_of - get underlying value

#include "meta.h"

#include <optional>
#include <print>
#include <string_view>

namespace meta = std::meta;

// ============================================================================
// Core enum reflection utilities
// ============================================================================

// Convert enum value to its name
template <typename E>
  requires std::is_enum_v<E>
constexpr auto enumToString(E value) -> std::string_view {
  template for (constexpr auto e : meta::enumerators_of(^^E)) {
    if (value == [:e:]) {
      return meta::name_of(e);
    }
  }
  return "<unknown>";
}

// Convert string to enum value (returns optional)
template <typename E>
  requires std::is_enum_v<E>
constexpr auto stringToEnum(std::string_view name) -> std::optional<E> {
  template for (constexpr auto e : meta::enumerators_of(^^E)) {
    if (name == meta::name_of(e)) {
      return [:e:];
    }
  }
  return std::nullopt;
}

// Get count of enumerators
template <typename E>
  requires std::is_enum_v<E>
consteval auto enumCount() -> std::size_t {
  return meta::enumerators_of(^^E).size();
}

// Check if value is a valid enumerator
template <typename E>
  requires std::is_enum_v<E>
constexpr auto isValidEnum(E value) -> bool {
  template for (constexpr auto e : meta::enumerators_of(^^E)) {
    if (value == [:e:]) {
      return true;
    }
  }
  return false;
}

// ============================================================================
// Print all enumerators with their values
// ============================================================================

template <typename E>
  requires std::is_enum_v<E>
void printEnumInfo() {
  std::println("Enum {} ({} values):", meta::name_of(^^E), enumCount<E>());

  template for (constexpr auto e : meta::enumerators_of(^^E)) {
    // meta::value_of gives the underlying integer value
    std::println("  {} = {}", meta::name_of(e), static_cast<int>([:e:]));
  }
}

// ============================================================================
// Example enums
// ============================================================================

enum class Color { Red, Green, Blue, Yellow, Cyan, Magenta };

enum class HttpStatus {
  OK = 200,
  Created = 201,
  BadRequest = 400,
  NotFound = 404,
  InternalError = 500
};

// Flags enum (powers of 2)
enum class Permission : unsigned {
  None = 0,
  Read = 1,
  Write = 2,
  Execute = 4,
  All = 7
};

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("=== Automatic Enum-to-String ===\n");

  // Print enum info
  printEnumInfo<Color>();
  std::println("");
  printEnumInfo<HttpStatus>();

  // enum -> string
  std::println("\nEnum to string:");
  std::println("  Color::Blue     -> \"{}\"", enumToString(Color::Blue));
  std::println("  HttpStatus::OK  -> \"{}\"", enumToString(HttpStatus::OK));
  std::println("  HttpStatus::404 -> \"{}\"", enumToString(HttpStatus::NotFound));

  // string -> enum
  std::println("\nString to enum:");
  if (auto color = stringToEnum<Color>("Green")) {
    std::println("  \"Green\" -> Color::{}", enumToString(*color));
  }
  if (auto status = stringToEnum<HttpStatus>("NotFound")) {
    std::println("  \"NotFound\" -> HttpStatus::{}", enumToString(*status));
  }
  if (auto invalid = stringToEnum<Color>("Purple"); !invalid) {
    std::println("  \"Purple\" -> (not found)");
  }

  // Validation
  std::println("\nValidation:");
  std::println("  isValidEnum(Color::Red): {}", isValidEnum(Color::Red));
  std::println("  isValidEnum(Color(42)):  {}", isValidEnum(Color{42}));

  // Compile-time usage
  static_assert(enumToString(Color::Red) == "Red");
  static_assert(stringToEnum<Color>("Blue") == Color::Blue);
  static_assert(enumCount<Color>() == 6);
  std::println("\nAll static_asserts passed!");

  return 0;
}
