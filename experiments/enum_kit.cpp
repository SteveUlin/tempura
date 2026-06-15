#include <cstdint>
#include <meta>
#include <print>
#include <string_view>

enum class Color : uint8_t {
  kRed,
  kBlue,
  kGreen,
};

template <typename E>
  requires std::is_enum_v<E>
constexpr auto toString(E value) -> std::string_view {
  template for (constexpr auto e :
    std::define_static_array(
      std::meta::enumerators_of(^^E))) {
    if (value == [:e:]) {
      return std::meta::identifier_of(e);
    }
  }

  return "<UNKNOWN ENUM VALUE>";
}

template <typename E>
  requires std::is_enum_v<E>
consteval auto numEntries() {
  return std::meta::enumerators_of(^^E).size();
}

auto main() -> int {

  template for (constexpr auto e :
      std::define_static_array(
        std::meta::enumerators_of(^^Color))) {
    constexpr std::string_view name = std::meta::identifier_of(e);
    Color value = [:e:];
    auto int_value = static_cast<int>(value);

    std::println("Enumerator: {} = {}", name, int_value);
  }

  std::println("Number of Entries: {}", numEntries<Color>());
  std::println("A value: {}", toString(Color::kRed));

  return 0;
}
