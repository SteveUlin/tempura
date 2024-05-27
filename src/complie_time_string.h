#include <algorithm>
#include <cstddef>

namespace tempura {

template <size_t N>
struct CompileTimeString {
  constexpr CompileTimeString(const char (&str)[N]) { std::copy_n(str, N, value); }
  char value[N]{};
};

template <CompileTimeString cts>
constexpr auto operator""_cts() {
  return cts;
}

template <size_t N, size_t M>
constexpr auto operator<=>(CompileTimeString<N> lhs, CompileTimeString<M> rhs) -> bool {
  return std::ranges::lexicographical_compare(lhs.value, rhs.value);
}

template <size_t N, size_t M>
constexpr auto operator!=(CompileTimeString<N> lhs, CompileTimeString<M> rhs) -> bool {
  return !(lhs == rhs);
}

template <size_t N, size_t M>
constexpr auto operator+(CompileTimeString<N> lhs,
                         CompileTimeString<M> rhs) -> CompileTimeString<N + M> {
  CompileTimeString<N + M> result{};
  std::copy_n(lhs.value, N, result.value);
  std::copy_n(rhs.value, M, result.value + N);
  return result;
}

}  // namespace tempura

