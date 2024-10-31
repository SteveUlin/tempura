#include "json.h"

#include <print>

using namespace tempura;

auto main() -> int {
  JsonValue value = JsonMap{{
      {"key1", "value1"},
      {"key2", 42},
      {"key3", 3.14},
      {"key4", JsonArray{{1, 2, 3}}},
      {"key5", JsonMap{{{"key5", true}}}},
  }};

  std::println("{}", value);
  return 0;
}
