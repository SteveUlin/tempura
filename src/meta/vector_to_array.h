#include <vector>
#include <array>

namespace tempura {

consteval auto vectorToArray(auto generator) {
  using T = decltype(generator())::value_type;
  constexpr size_t N = generator().size();
  std::array<T, N> arr{};
  std::ranges::copy(generator(), arr.begin());
  return arr;
}

} // namespace tempura
