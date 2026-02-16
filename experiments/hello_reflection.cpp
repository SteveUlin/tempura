#include <experimental/meta>
#include <print>

// Expand helper — converts a vector<info> into a pack expansion.
// [:expand(range):] >> [&]<auto elem> { ... } iterates at compile time
// without the heap-allocation issues of template for.
template <auto... Vals>
struct Replicator {
  template <typename F>
  constexpr void operator>>(F body) const {
    (body.template operator()<Vals>(), ...);
  }
};

template <auto... Vals>
constexpr Replicator<Vals...> replicator = {};

template <typename R>
consteval auto expand(R range) {
  std::vector<std::meta::info> args;
  for (auto r : range) {
    args.push_back(std::meta::reflect_constant(r));
  }
  return std::meta::substitute(^^replicator, args);
}

struct Point {
  double x;
  double y;
  double z;
};

int main() {
  [:expand(std::meta::nonstatic_data_members_of(^^Point,
      std::meta::access_context::unchecked())):] >> [&]<auto member> {
    std::println("{}", std::meta::identifier_of(member));
  };
}
