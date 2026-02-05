// Define Aggregate Types with Reflection
//
// Advanced reflection example: programmatically construct new types
// at compile time by manipulating member lists.
//
// This uses P2996's "define_aggregate" to create new struct types
// from a list of member specifications.

#include "meta.h"

#include <print>
#include <string_view>
#include <utility>
#include <vector>

namespace meta = std::meta;

// ============================================================================
// Create a struct that mirrors another but with all members const
// ============================================================================

template <typename T>
consteval auto makeConstMembersType() -> meta::info {
  std::vector<meta::info> const_members;

  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    // Create a const version of this member's type
    auto member_type = meta::type_of(member);
    auto const_type = meta::add_const(member_type);

    // Create member specification with same name but const type
    const_members.push_back(
        meta::data_member_spec(const_type, {.name = meta::name_of(member)}));
  }

  return meta::define_aggregate(^^T, const_members);
}

// ============================================================================
// Create a "builder" type with setter methods
// ============================================================================

// Concept: aggregate with nonstatic data members
template <typename T>
concept Aggregate =
    std::is_aggregate_v<T> && (meta::nonstatic_data_members_of(^^T).size() > 0);

// Generate a builder pattern for any aggregate
template <Aggregate T>
class Builder {
 public:
  // Setters for each field using reflection
  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    constexpr auto MemberType = meta::type_of(member);

    auto [:meta::identifier(std::string("with_") + std::string(meta::name_of(member))):]
        ([:MemberType:] value) -> Builder& {
      value_.[:member:] = std::move(value);
      return *this;
    }
  }

  auto build() const -> T { return value_; }

 private:
  T value_{};
};

// ============================================================================
// Copy struct but add a prefix to all member names
// ============================================================================

template <meta::info Struct, meta::info Prefix>
consteval auto prefixedStruct() -> meta::info {
  std::vector<meta::info> members;

  template for (constexpr auto m : meta::nonstatic_data_members_of(Struct)) {
    std::string new_name =
        std::string([:Prefix:]) + "_" + std::string(meta::name_of(m));
    members.push_back(
        meta::data_member_spec(meta::type_of(m), {.name = new_name}));
  }

  return meta::define_aggregate(Struct, members);
}

// ============================================================================
// Struct with only numeric members (filter members by type)
// ============================================================================

template <typename T>
consteval auto numericMembersOnly() -> meta::info {
  std::vector<meta::info> numeric_members;

  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    auto member_type = meta::type_of(member);
    // Check if the member type is arithmetic
    if constexpr (std::is_arithmetic_v<[:member_type:]>) {
      numeric_members.push_back(member);
    }
  }

  return meta::define_aggregate(^^T, numeric_members);
}

// ============================================================================
// Example types
// ============================================================================

struct Point {
  double x;
  double y;
  double z;
};

struct Person {
  std::string_view name;
  int age;
  double height;
};

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("=== Define Aggregate Types ===\n");

  // Basic struct
  Point p{.x = 1.0, .y = 2.0, .z = 3.0};
  std::println("Original Point: ({}, {}, {})", p.x, p.y, p.z);

  // Using the Builder pattern
  std::println("\nBuilder pattern:");
  auto built_point =
      Builder<Point>{}.with_x(10.0).with_y(20.0).with_z(30.0).build();
  std::println("Built Point: ({}, {}, {})", built_point.x, built_point.y,
               built_point.z);

  // Person builder
  auto person =
      Builder<Person>{}.with_name("Bob").with_age(25).with_height(1.85).build();
  std::println("Built Person: {} (age {}, height {})", person.name, person.age,
               person.height);

  std::println("\n✓ All examples completed successfully!");

  return 0;
}
