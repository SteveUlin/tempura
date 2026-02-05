// Basic C++26 Reflection Examples
//
// Demonstrates the fundamental reflection primitives:
// - ^^T (reflect operator) - creates a std::meta::info from a type/entity
// - [:r:] (splice operator) - reifies a reflection back into code
// - std::meta::name_of - extracts the name as a string_view
// - std::meta::type_of - gets the type of a reflected entity

#include "meta.h"

#include <print>
#include <string_view>

namespace meta = std::meta;

// ============================================================================
// Example 1: Getting type names at compile time
// ============================================================================

template <typename T>
consteval auto typeName() -> std::string_view {
  return meta::name_of(^^T);
}

// ============================================================================
// Example 2: Check if two types are the same using reflection
// ============================================================================

template <typename T, typename U>
consteval auto sameTypeReflected() -> bool {
  return ^^T == ^^U;
}

// ============================================================================
// Example 3: Get qualified name (includes namespaces)
// ============================================================================

namespace my_namespace {
struct MyStruct {};
}  // namespace my_namespace

template <typename T>
consteval auto qualifiedName() -> std::string_view {
  return meta::qualified_name_of(^^T);
}

// ============================================================================
// Example 4: Reflect on fundamental properties
// ============================================================================

template <typename T>
consteval auto describeType() {
  constexpr auto r = ^^T;

  struct TypeInfo {
    std::string_view name;
    bool is_class;
    bool is_enum;
    bool is_union;
    bool is_fundamental;
  };

  return TypeInfo{
      .name = meta::name_of(r),
      .is_class = meta::is_class_type(r),
      .is_enum = meta::is_enum_type(r),
      .is_union = meta::is_union_type(r),
      .is_fundamental =
          !meta::is_class_type(r) && !meta::is_enum_type(r) && !meta::is_union_type(r),
  };
}

// ============================================================================
// Example 5: Splice to create types dynamically
// ============================================================================

// Given a reflection of a type, create an instance of that type
template <meta::info R>
consteval auto makeDefaultValue() {
  return [:R:]{};  // Splice the reflection and value-initialize
}

// ============================================================================
// Main - demonstrate all features
// ============================================================================

struct Point {
  double x;
  double y;
};

enum class Color { Red, Green, Blue };

auto main() -> int {
  std::println("=== Basic C++26 Reflection ===\n");

  // Type names
  std::println("Type names:");
  std::println("  int       -> \"{}\"", typeName<int>());
  std::println("  double    -> \"{}\"", typeName<double>());
  std::println("  Point     -> \"{}\"", typeName<Point>());
  std::println("  Color     -> \"{}\"", typeName<Color>());

  // Qualified names
  std::println("\nQualified names:");
  std::println("  MyStruct  -> \"{}\"", qualifiedName<my_namespace::MyStruct>());

  // Type comparison
  std::println("\nType comparison via reflection:");
  std::println("  int == int:    {}", sameTypeReflected<int, int>());
  std::println("  int == double: {}", sameTypeReflected<int, double>());

  // Type properties
  std::println("\nType properties:");
  {
    constexpr auto info = describeType<Point>();
    std::println("  Point: is_class={}, is_enum={}", info.is_class, info.is_enum);
  }
  {
    constexpr auto info = describeType<Color>();
    std::println("  Color: is_class={}, is_enum={}", info.is_class, info.is_enum);
  }
  {
    constexpr auto info = describeType<int>();
    std::println("  int:   is_fundamental={}", info.is_fundamental);
  }

  // Splicing to create values
  std::println("\nSplicing:");
  constexpr int val = makeDefaultValue<^^int>();
  std::println("  makeDefaultValue<^^int>() = {}", val);

  return 0;
}
