#include "type_list.h"

#include "type_name.h"
#include "unit.h"

using namespace tempura;
using namespace std::string_view_literals;

template <typename T>
struct LessEqualFloat
    : std::conditional_t<typeName<T>() <= "float"sv, std::true_type, std::false_type> {};

auto main() -> int {
  "TypeList Constructor"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>{};
    static_assert(std::is_same_v<decltype(list), const TypeList<int, double, float, char>>);

    constexpr auto list2 = TypeList{int{}, double{}, float{}, char{}};
    static_assert(std::is_same_v<decltype(list2), const TypeList<int, double, float, char>>);
  };

  "TypeList Concat"_test = [] {
    constexpr auto lhs = TypeList<int, double>{};
    constexpr auto rhs = TypeList<float, char>{};

    static_assert(std::is_same_v<decltype(concat(lhs, rhs)), TypeList<int, double, float, char>>);
    static_assert(std::is_same_v<decltype(concat(lhs, rhs, rhs)), TypeList<int, double, float, char, float, char>>);
    static_assert(std::is_same_v<decltype(concat(lhs, TypeList<>{}, rhs)), TypeList<int, double, float, char>>);
  };

  "TypeList Head"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>{};
    static_assert(std::is_same_v<decltype(list.head()), int>);
    // Not defined
    // TypeList{}.head();
  };

  "TypeList Tail"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>();
    static_assert(std::is_same_v<TypeList<double, float, char>, decltype(list.tail())>);
    // Not defined
    // TypeList{}.tail();
  };

  "TypeList Get"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>();
    static_assert(std::is_same_v<int, decltype(list.get<0>())>);
    static_assert(std::is_same_v<char, decltype(list.get<3>())>);
    // Not defined
    // TypeList{}.tail();
  };

  "TypeList Size"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>{};
    static_assert(4ULL == list.size());
  };

  "TypeList Empty"_test = [] {
    constexpr auto empty = TypeList{};
    static_assert(empty.empty());

    constexpr auto list = TypeList<int, double, float, char>{};
    static_assert(!list.empty());
  };

  "TypeList Filter"_test = [] {
    constexpr auto list = TypeList<int, double, float, char, int>{};

    constexpr auto left = list.filter<LessEqualFloat>();
    constexpr auto right = list.invFilter<LessEqualFloat>();
    // This compiles but clang doesn't think so
    static_assert(left == TypeList<double, float, char>{});
    static_assert(right == TypeList<int, int>{});
  };

  "TypeList sort"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>{};

    static_assert(std::is_same_v<decltype(sort<TypeNameCmp>(list)), TypeList<char, double, float, int>>);
  };

  "TypeList groupBy"_test = [] {
    constexpr auto list = TypeList<double, double, float, char>{};

    std::cout << typeName(groupBy<TypeNameEq>(list)) << std::endl;
  };

  return 0;
}
