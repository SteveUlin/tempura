#pragma once

#include <format>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace tempura {

using JsonValue = std::variant<std::monostate, bool, int64_t, double,
                               std::string, struct JsonArray, struct JsonMap>;
struct JsonArray : std::vector<JsonValue> {};
struct JsonMap : std::map<std::string, JsonValue> {};

auto escapeJson(const std::string& s) -> std::string {
  std::string output;
  for (auto c : s) {
    switch (c) {
      case '"':
        output += R"(\")";
        break;
      case '\\':
        output += R"(\\)";
        break;
      case '/':
        output += R"(\/)";
        break;
      case '\b':
        output += R"(\b)";
        break;
      case '\f':
        output += R"(\f)";
        break;
      case '\n':
        output += R"(\n)";
        break;
      case '\r':
        output += R"(\r)";
        break;
      case '\t':
        output += R"(\t)";
        break;
      default:
        output += c;
    }
  }
  return output;
}

namespace internal {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

constexpr auto toString(const JsonValue& obj, size_t indentLevel = 0)
    -> std::string;
constexpr auto toString(const JsonArray& obj, size_t indentLevel = 0)
    -> std::string;

constexpr auto toString(const JsonMap& obj, size_t indentLevel = 0)
    -> std::string {
  if (obj.empty()) {
    return "{}";
  }
  const std::string indent = std::string(indentLevel * 2, ' ');

  std::string output = "{\n";
  bool first = true;
  for (const auto& [key, value] : obj) {
    if (!first) {
      output += ",\n";
    }
    first = false;
    output += std::format("  {}\"{}\" : {}", indent, escapeJson(key),
                          toString(value, indentLevel + 1));
  }
  output += std::format("\n{}}}", indent);
  return output;
}

constexpr auto toString(const JsonArray& obj, size_t indentLevel)
    -> std::string {
  if (obj.empty()) {
    return "[]";
  }
  const std::string indent = std::string(indentLevel * 2, ' ');

  std::string output = "[\n";
  bool first = true;
  for (const auto& element : obj) {
    if (!first) {
      output += ",\n";
    }
    first = false;
    output += std::format("  {}{}", indent, toString(element, indentLevel + 1));
  }
  output += std::format("\n{}]", indent);
  return output;
}

constexpr auto toString(const JsonValue& obj, size_t indentLevel)
    -> std::string {
  return std::visit(
      Overloaded{
          [](std::monostate) -> std::string { return "null"; },
          [](bool curr) -> std::string { return curr ? "true" : "false"; },
          [](int64_t curr) -> std::string { return std::to_string(curr); },
          [](double curr) -> std::string { return std::format("{:g}", curr); },
          [](const std::string& curr) -> std::string {
            return std::format("\"{}\"", escapeJson(curr));
          },
          [&](const JsonArray& curr) -> std::string {
            return toString(curr, indentLevel);
          },
          [&](const JsonMap& curr) -> std::string {
            return toString(curr, indentLevel);
          }},
      obj);
}

}  // namespace internal

}  // namespace tempura

template <>
struct std::formatter<tempura::JsonValue> : std::formatter<std::string> {
  auto format(const tempura::JsonValue& value, std::format_context& ctx) const {
    return std::formatter<std::string>::format(tempura::internal::toString(value), ctx);
  }
};

template <>
struct std::formatter<tempura::JsonArray> : std::formatter<std::string> {
  auto format(const tempura::JsonArray& value, std::format_context& ctx) const {
    return std::formatter<std::string>::format(tempura::internal::toString(value), ctx);
  }
};

template <>
struct std::formatter<tempura::JsonMap> : std::formatter<std::string> {
  auto format(const tempura::JsonMap& value, std::format_context& ctx) const {
    return std::formatter<std::string>::format(tempura::internal::toString(value), ctx);
  }
};

