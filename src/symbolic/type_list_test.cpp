#include "symbolic/type_list.h"

#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic;
using namespace std::string_view_literals;

// template <typename T>
// struct LessEqualFloat
//     : std::conditional_t<typeName<T>() <= "float"sv, std::true_type,
//     std::false_type> {};

auto main() -> int {
  "TypeList Constructor"_test = [] {
    {
      TypeList<int, double, float, char> list{};
      static_assert(
          std::is_same_v<decltype(list), TypeList<int, double, float, char>>);
    }

    {
      TypeList list{int{}, double{}, float{}, char{}};
      static_assert(
          std::is_same_v<decltype(list), TypeList<int, double, float, char>>);
    }
  };

  "TypeList Concat"_test = [] {
    static_assert(
        std::is_same_v<TypeList<int, double, float, char>,
                       Concat<TypeList<int, double>, TypeList<float, char>>>);

    static_assert(std::is_same_v<TypeList<int, double>,
                                 Concat<TypeList<int, double>, TypeList<>>>);

    static_assert(std::is_same_v<TypeList<float, char>,
                                 Concat<TypeList<>, TypeList<float, char>>>);
  };

  "TypeList Head"_test = [] {
    using List = TypeList<int, double, float, char>;
    static_assert(std::is_same_v<int, List::HeadType>);
    // Not defined
    // TypeList<>::HeadType;
  };

  "TypeList Tail"_test = [] {
    using List = TypeList<int, double, float, char>;
    static_assert(
        std::is_same_v<TypeList<double, float, char>, List::TailType>);
    // Not defined
    // TypeList<>::TailType;
  };

  "TypeList Size"_test = [] {
    using List = TypeList<int, double, float, char>;
    static_assert(4ULL == List::size);
    static_assert(0ULL == TypeList<>::size);
  };

  "TypeList Empty"_test = [] {
    static_assert(TypeList<>::empty);

    using List = TypeList<int, double, float, char>;
    static_assert(not List::empty);
  };

  "TypeList =="_test = [] {
    static_assert(TypeList<int, double, float, char>{} ==
                  TypeList<int, double, float, char>{});
    static_assert(not(TypeList<int, double, float, char>{} ==
                      TypeList<int, double, float>{}));

    static_assert(TypeList<int, double, float, char>{} !=
                  TypeList<int, double, float>{});
    static_assert(not(TypeList<int, double, float, char>{} !=
                      TypeList<int, double, float, char>{}));
  };

  //   "TypeList Get"_test = [] {
  //     constexpr auto list = TypeList<int, double, float, char>();
  //     static_assert(std::is_same_v<int, decltype(list.get<0>())>);
  //     static_assert(std::is_same_v<char, decltype(list.get<3>())>);
  //     // Not defined
  //     // TypeList{}.tail();
  //   };
  //
  //
  //
  //   "TypeList Filter"_test = [] {
  //     constexpr auto list = TypeList<int, double, float, char, int>{};
  //
  //     constexpr auto left = list.filter<LessEqualFloat>();
  //     constexpr auto right = list.invFilter<LessEqualFloat>();
  //     // This compiles but clang doesn't think so
  //     static_assert(left == TypeList<double, float, char>{});
  //     static_assert(right == TypeList<int, int>{});
  //   };
  //
  //   "TypeList sort"_test = [] {
  //     constexpr auto list = TypeList<int, double, float, char>{};
  //
  //     static_assert(std::is_same_v<decltype(sort<TypeNameCmp>(list)),
  //     TypeList<char, double, float, int>>);
  //   };
  //
  //   "TypeList groupBy"_test = [] {
  //     constexpr auto list = TypeList<double, double, float, char>{};
  //
  //     std::cout << typeName(groupBy<TypeNameEq>(list)) << std::endl;
  //   };

  return 0;
}
