#include "json.h"

#include <format>
#include <string>
#include <variant>

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "escapeJson handles the JSON special characters"_test = [] {
    expectEq(escapeJson("plain"), "plain");
    expectEq(escapeJson(R"(")"), R"(\")");  // a quote
    expectEq(escapeJson(R"(\)"), R"(\\)");  // a backslash
    expectEq(escapeJson("\t"), R"(\t)");    // a tab
    expectEq(escapeJson("\n"), R"(\n)");    // a newline
    // Control chars without a short escape use the \uXXXX form.
    expectTrue(escapeJson("\x01").ends_with("u0001"));
  };

  "scalars serialize to their JSON literals"_test = [] {
    expectEq(toString(JsonValue{std::monostate{}}), "null");
    expectEq(toString(JsonValue{true}), "true");
    expectEq(toString(JsonValue{false}), "false");
    expectEq(toString(JsonValue{42}), "42");
    expectEq(toString(JsonValue{std::string{"hi"}}), R"("hi")");
  };

  "empty containers are compact"_test = [] {
    expectEq(toString(JsonValue{JsonArray{}}), "[]");
    expectEq(toString(JsonValue{JsonMap{}}), "{}");
  };

  "nested structure formats through std::formatter"_test = [] {
    const JsonValue v = JsonMap{{"n", 1}, {"arr", JsonArray{2, 3}}};
    const std::string s = std::format("{}", v);
    // Pretty output is whitespace-sensitive, so assert structure, not a golden.
    expectTrue(s.starts_with("{"));
    expectTrue(s.contains(R"("arr")"));
    expectTrue(s.contains("3"));
  };

  return TestRegistry::result();
}
