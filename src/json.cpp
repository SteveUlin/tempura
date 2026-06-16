#include "json.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <variant>

namespace tempura {
namespace {

template <class... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

}  // namespace

auto escapeJson(const std::string& s) -> std::string {
  std::string output;
  output.reserve(s.size());
  for (unsigned char c : s) {
    switch (c) {
      case '"': output += R"(\")"; break;
      case '\\': output += R"(\\)"; break;
      case '\b': output += R"(\b)"; break;
      case '\f': output += R"(\f)"; break;
      case '\n': output += R"(\n)"; break;
      case '\r': output += R"(\r)"; break;
      case '\t': output += R"(\t)"; break;
      default:
        // JSON requires control characters (U+0000–U+001F) to be escaped.
        if (c < 0x20) {
          output += std::format(R"(\u{:04x})", c);
        } else {
          output += static_cast<char>(c);
        }
    }
  }
  return output;
}

auto toString(const JsonMap& value, size_t indent_level) -> std::string {
  if (value.empty()) {
    return "{}";
  }
  const std::string indent(indent_level * 2, ' ');
  std::string output = "{\n";
  bool first = true;
  for (const auto& [key, val] : value) {
    if (!first) {
      output += ",\n";
    }
    first = false;
    output += std::format("  {}\"{}\" : {}", indent, escapeJson(key),
                          toString(val, indent_level + 1));
  }
  output += std::format("\n{}}}", indent);
  return output;
}

auto toString(const JsonArray& value, size_t indent_level) -> std::string {
  if (value.empty()) {
    return "[]";
  }
  const std::string indent(indent_level * 2, ' ');
  std::string output = "[\n";
  bool first = true;
  for (const auto& element : value) {
    if (!first) {
      output += ",\n";
    }
    first = false;
    output += std::format("  {}{}", indent, toString(element, indent_level + 1));
  }
  output += std::format("\n{}]", indent);
  return output;
}

auto toString(const JsonValue& value, size_t indent_level) -> std::string {
  return std::visit(
      Overloaded{
          [](std::monostate) -> std::string { return "null"; },
          [](bool curr) -> std::string { return curr ? "true" : "false"; },
          [](int curr) -> std::string { return std::to_string(curr); },
          [](int64_t curr) -> std::string { return std::to_string(curr); },
          [](double curr) -> std::string { return std::format("{:g}", curr); },
          [](const std::string& curr) -> std::string {
            return std::format("\"{}\"", escapeJson(curr));
          },
          [&](const JsonArray& curr) -> std::string {
            return toString(curr, indent_level);
          },
          [&](const JsonMap& curr) -> std::string {
            return toString(curr, indent_level);
          }},
      value);
}

}  // namespace tempura
