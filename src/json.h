#pragma once

#include <cstddef>
#include <cstdint>
#include <format>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace tempura {

struct JsonArray;
struct JsonMap;

using JsonValue = std::variant<std::monostate, bool, int, int64_t, double,
                               std::string, JsonArray, JsonMap>;

struct JsonArray : std::vector<JsonValue> {
  using std::vector<JsonValue>::vector;
  JsonArray() = default;
  JsonArray(std::initializer_list<JsonValue> init) : std::vector<JsonValue>(init) {}
};

struct JsonMap : std::map<std::string, JsonValue> {
  using std::map<std::string, JsonValue>::map;
  JsonMap() = default;
  JsonMap(std::initializer_list<std::pair<const std::string, JsonValue>> init)
      : std::map<std::string, JsonValue>(init) {}
};

// Serialization is non-templated, so it lives in json.cpp and is compiled once
// into the static library — not emitted into every translation unit that
// includes this header (which would be an ODR violation for escapeJson).
auto escapeJson(const std::string& s) -> std::string;
auto toString(const JsonValue& value, size_t indent_level = 0) -> std::string;
auto toString(const JsonArray& value, size_t indent_level = 0) -> std::string;
auto toString(const JsonMap& value, size_t indent_level = 0) -> std::string;

}  // namespace tempura

template <>
struct std::formatter<tempura::JsonValue> : std::formatter<std::string> {
  auto format(const tempura::JsonValue& value, std::format_context& ctx) const {
    return std::formatter<std::string>::format(tempura::toString(value), ctx);
  }
};

template <>
struct std::formatter<tempura::JsonArray> : std::formatter<std::string> {
  auto format(const tempura::JsonArray& value, std::format_context& ctx) const {
    return std::formatter<std::string>::format(tempura::toString(value), ctx);
  }
};

template <>
struct std::formatter<tempura::JsonMap> : std::formatter<std::string> {
  auto format(const tempura::JsonMap& value, std::format_context& ctx) const {
    return std::formatter<std::string>::format(tempura::toString(value), ctx);
  }
};
