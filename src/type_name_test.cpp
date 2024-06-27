#include "type_name.h"

#include "unit.h"

using namespace tempura;
using namespace std::string_view_literals;

auto main() -> int {
  "Basic typenames"_test = [] {
    expectEq(typeName<int>(), "int"sv);
    expectEq(typeName<char>(), "char"sv);

    expectEq(typeName(int{}), "int"sv);
    expectEq(typeName(char{}), "char"sv);
  };

  "Constexpr typenames"_test = [] {
    static_assert(typeName<int>() == "int"sv);
    static_assert(typeName<char>() == "char"sv);

    static_assert(typeName(int{}) == "int"sv);
    static_assert(typeName(char{}) == "char"sv);
  };

  "Lambdas are different"_test = [] {
    expectNeq(typeName<decltype([]{})>(), typeName<decltype([]{})>());
    expectNeq(typeName([]{}), typeName([]{}));
  };

  return TestRegistry::result();
}
