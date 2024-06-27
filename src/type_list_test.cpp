#include "type_list.h"

#include "type_name.h"
#include "unit.h"

using namespace tempura;
using namespace std::string_view_literals;

struct FilterFn {
  template <typename T>
  static constexpr auto operator()() {
    return typeName<T>() <= "float"sv;
  }
};

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

    static_assert(std::is_same_v<decltype(list.filter<FilterFn>()), TypeList<double, float, char>>);
  };

  "TypeList Sort"_test = [] {
    constexpr auto list = TypeList<int, double, float, char>{};

    static_assert(std::is_same_v<decltype(sort<internal::TypeNameCmp>(list)), TypeList<char, double, float, int>>);
  };

  return 0;
}
