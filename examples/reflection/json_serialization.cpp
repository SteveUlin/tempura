// Automatic JSON Serialization with C++26 Reflection
//
// Demonstrates a practical use case: generating JSON from any struct
// without writing boilerplate serialization code.
//
// This approach eliminates the need for:
// - Macro-based serialization (NLOHMANN_DEFINE_TYPE_INTRUSIVE, etc.)
// - Hand-written to_json/from_json functions
// - External code generators

#include "meta.h"

#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace meta = std::meta;

// ============================================================================
// JSON Serialization
// ============================================================================

// Forward declaration for recursive calls
template <typename T>
auto toJson(T const& value) -> std::string;

// Primitives
auto toJson(int value) -> std::string { return std::to_string(value); }
auto toJson(double value) -> std::string { return std::to_string(value); }
auto toJson(bool value) -> std::string { return value ? "true" : "false"; }

auto toJson(std::string_view value) -> std::string {
  std::string result = "\"";
  for (char c : value) {
    if (c == '"')
      result += "\\\"";
    else if (c == '\\')
      result += "\\\\";
    else if (c == '\n')
      result += "\\n";
    else
      result += c;
  }
  result += '"';
  return result;
}

auto toJson(std::string const& value) -> std::string {
  return toJson(std::string_view{value});
}

auto toJson(char const* value) -> std::string {
  return toJson(std::string_view{value});
}

// Vector serialization
template <typename T>
auto toJson(std::vector<T> const& vec) -> std::string {
  std::string result = "[";
  bool first = true;
  for (auto const& elem : vec) {
    if (!first) result += ", ";
    first = false;
    result += toJson(elem);
  }
  result += "]";
  return result;
}

// Struct serialization using reflection
template <typename T>
  requires std::is_class_v<T>
auto toJson(T const& obj) -> std::string {
  std::string result = "{\n";
  bool first = true;

  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    if (!first) result += ",\n";
    first = false;

    result += "  \"";
    result += meta::name_of(member);
    result += "\": ";
    result += toJson(obj.[:member:]);
  }

  result += "\n}";
  return result;
}

// ============================================================================
// Pretty printer with indentation
// ============================================================================

template <typename T>
auto toJsonPretty(T const& obj, int indent = 0) -> std::string;

// Implementation for structs
template <typename T>
  requires std::is_class_v<T> && (!requires { typename T::value_type; })
auto toJsonPretty(T const& obj, int indent) -> std::string {
  std::string pad(indent, ' ');
  std::string inner_pad(indent + 2, ' ');
  std::string result = "{\n";
  bool first = true;

  template for (constexpr auto member : meta::nonstatic_data_members_of(^^T)) {
    if (!first) result += ",\n";
    first = false;

    result += inner_pad;
    result += '"';
    result += meta::name_of(member);
    result += "\": ";
    result += toJsonPretty(obj.[:member:], indent + 2);
  }

  result += "\n" + pad + "}";
  return result;
}

// Overloads for primitives (ignore indent)
auto toJsonPretty(int v, int) -> std::string { return toJson(v); }
auto toJsonPretty(double v, int) -> std::string { return toJson(v); }
auto toJsonPretty(bool v, int) -> std::string { return toJson(v); }
auto toJsonPretty(std::string_view v, int) -> std::string { return toJson(v); }
auto toJsonPretty(std::string const& v, int) -> std::string { return toJson(v); }

template <typename T>
auto toJsonPretty(std::vector<T> const& vec, int indent) -> std::string {
  if (vec.empty()) return "[]";

  std::string pad(indent, ' ');
  std::string inner_pad(indent + 2, ' ');
  std::string result = "[\n";

  for (std::size_t i = 0; i < vec.size(); ++i) {
    result += inner_pad + toJsonPretty(vec[i], indent + 2);
    if (i + 1 < vec.size()) result += ",";
    result += "\n";
  }

  result += pad + "]";
  return result;
}

// ============================================================================
// Example structs
// ============================================================================

struct Address {
  std::string street;
  std::string city;
  int zip_code;
};

struct Person {
  std::string name;
  int age;
  bool employed;
  Address address;
};

struct Team {
  std::string team_name;
  std::vector<std::string> members;
  Person lead;
};

// ============================================================================
// Main
// ============================================================================

auto main() -> int {
  std::println("=== Automatic JSON Serialization ===\n");

  // Simple struct
  Address addr{
      .street = "123 Main St",
      .city = "Springfield",
      .zip_code = 12345,
  };

  std::println("Address JSON:");
  std::println("{}\n", toJsonPretty(addr));

  // Nested struct
  Person person{
      .name = "Alice",
      .age = 30,
      .employed = true,
      .address = addr,
  };

  std::println("Person JSON:");
  std::println("{}\n", toJsonPretty(person));

  // With vectors
  Team team{
      .team_name = "Engineering",
      .members = {"Alice", "Bob", "Charlie"},
      .lead = person,
  };

  std::println("Team JSON:");
  std::println("{}", toJsonPretty(team));

  return 0;
}
