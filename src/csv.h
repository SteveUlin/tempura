#pragma once

// CSV parsing library with row-by-row iteration
//
// Usage:
//   std::istringstream input{"name,age\nAlice,30\nBob,25"};
//   CsvReader reader{input, ','};
//   reader.skipRows(1);  // skip header
//   while (reader.nextRow()) {
//     std::string name = reader.get<std::string>(0);
//     int64_t age = reader.get<int64_t>(1);
//   }

#include <cassert>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace tempura {

// Parses CSV data row-by-row from any std::istream
class CsvReader {
 public:
  explicit CsvReader(std::istream& input, char delimiter = ',')
      : input_{&input}, delimiter_{delimiter} {}

  // Skip the next n rows (useful for headers)
  void skipRows(int n) {
    for (int i = 0; i < n && input_->good(); ++i) {
      std::string line;
      std::getline(*input_, line);
    }
  }

  // Advance to the next row. Returns false if no more rows.
  auto nextRow() -> bool {
    if (!std::getline(*input_, current_line_)) {
      fields_.clear();
      return false;
    }
    parseCurrentLine();
    return true;
  }

  // Number of fields in the current row
  [[nodiscard]] auto numFields() const -> std::size_t { return fields_.size(); }

  // Get field at index as string (no conversion)
  [[nodiscard]] auto getString(std::size_t index) const -> std::string_view {
    assert(index < fields_.size() && "field index out of bounds");
    return fields_[index];
  }

  // Get field at index, converted to type T
  // Supported types: std::string, std::string_view, int64_t, int, double
  template <typename T>
  [[nodiscard]] auto get(std::size_t index) const -> T {
    std::string_view field = getString(index);

    if constexpr (std::is_same_v<T, std::string>) {
      return stripQuotes(field);
    } else if constexpr (std::is_same_v<T, std::string_view>) {
      return field;
    } else if constexpr (std::is_integral_v<T>) {
      return parseIntegral<T>(field);
    } else if constexpr (std::is_floating_point_v<T>) {
      return parseFloatingPoint<T>(field);
    } else {
      static_assert(sizeof(T) == 0, "Unsupported type for CSV parsing");
    }
  }

  // Try to get field, returns nullopt if index out of bounds or parse fails
  template <typename T>
  [[nodiscard]] auto tryGet(std::size_t index) const -> std::optional<T> {
    if (index >= fields_.size()) {
      return std::nullopt;
    }
    std::string_view field = fields_[index];

    if constexpr (std::is_same_v<T, std::string>) {
      return std::string{stripQuotes(field)};
    } else if constexpr (std::is_same_v<T, std::string_view>) {
      return field;
    } else if constexpr (std::is_integral_v<T>) {
      return tryParseIntegral<T>(field);
    } else if constexpr (std::is_floating_point_v<T>) {
      return tryParseFloatingPoint<T>(field);
    } else {
      static_assert(sizeof(T) == 0, "Unsupported type for CSV parsing");
    }
  }

  // Check if the input stream has more data
  [[nodiscard]] auto good() const -> bool { return input_->good(); }

  // Check if end of file reached
  [[nodiscard]] auto eof() const -> bool { return input_->eof(); }

 private:
  void parseCurrentLine() {
    fields_.clear();
    if (current_line_.empty()) {
      return;
    }

    std::size_t start = 0;
    bool in_quotes = false;

    for (std::size_t i = 0; i < current_line_.size(); ++i) {
      char c = current_line_[i];

      if (c == '"') {
        in_quotes = !in_quotes;
      } else if (c == delimiter_ && !in_quotes) {
        fields_.emplace_back(current_line_.data() + start, i - start);
        start = i + 1;
      }
    }

    // Add the last field
    fields_.emplace_back(current_line_.data() + start,
                         current_line_.size() - start);
  }

  // Remove surrounding quotes if present
  static auto stripQuotes(std::string_view s) -> std::string {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
      s = s.substr(1, s.size() - 2);
    }
    // Handle escaped quotes ""
    std::string result;
    result.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '"' && i + 1 < s.size() && s[i + 1] == '"') {
        result += '"';
        ++i;  // Skip the second quote
      } else {
        result += s[i];
      }
    }
    return result;
  }

  template <typename T>
  static auto parseIntegral(std::string_view s) -> T {
    // Skip leading/trailing whitespace
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
      s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
      s.remove_suffix(1);
    }

    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    assert(ec == std::errc{} && "failed to parse integer");
    return value;
  }

  template <typename T>
  static auto tryParseIntegral(std::string_view s) -> std::optional<T> {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
      s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
      s.remove_suffix(1);
    }

    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{}) {
      return std::nullopt;
    }
    return value;
  }

  template <typename T>
  static auto parseFloatingPoint(std::string_view s) -> T {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
      s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
      s.remove_suffix(1);
    }

    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    assert(ec == std::errc{} && "failed to parse floating-point");
    return value;
  }

  template <typename T>
  static auto tryParseFloatingPoint(std::string_view s) -> std::optional<T> {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
      s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
      s.remove_suffix(1);
    }

    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{}) {
      return std::nullopt;
    }
    return value;
  }

  std::istream* input_;
  char delimiter_;
  std::string current_line_;
  std::vector<std::string_view> fields_;
};

}  // namespace tempura
