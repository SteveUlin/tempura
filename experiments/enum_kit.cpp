#include <experimental/meta>
#include <print>
#include <string_view>

enum class Color : std::uint8_t {
  kRed,
  kBlue,
  kGreen,
};

auto main() -> int {
  template for (constexpr auto e :
      std::define_static_array(
        std::meta::enumerators_of(^^Color))) {
    constexpr std::string_view name = std::meta::identifier_of(e);
    Color value = [:e:];
    auto int_value = static_cast<int>(value);

    std::println("Enumerator: {} = {}", name, int_value);
  }
  return 0;
}
