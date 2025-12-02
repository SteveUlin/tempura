#include "meta/fixed_string.h"

#include "unit.h"

using namespace tempura;

auto main() -> int {
  "construction from string literal"_test = [] {
    constexpr FixedString str = "hello";
    static_assert(str.size() == 5);
    static_assert(!str.empty());
    expectEq(str.size(), 5u);
    expectFalse(str.empty());
  };

  "construction from empty string literal"_test = [] {
    constexpr FixedString str = "";
    static_assert(str.size() == 0);
    static_assert(str.empty());
    expectEq(str.size(), 0u);
    expectTrue(str.empty());
  };

  "construction from variadic chars"_test = [] {
    constexpr FixedString<char, 3> str('a', 'b', 'c');
    static_assert(str.size() == 3);
    expectEq(str[0], 'a');
    expectEq(str[1], 'b');
    expectEq(str[2], 'c');
  };

  "construction from pointer range"_test = [] {
    const char* source = "hello world";
    FixedString<char, 5> str(source, source + 5);
    expectEq(str[0], 'h');
    expectEq(str[4], 'o');
  };

  "default construction"_test = [] {
    constexpr FixedString<char, 5> str;
    static_assert(str.size() == 5);
    static_assert(str[0] == '\0');
    expectEq(str[0], '\0');
  };

  "c_str returns null-terminated string"_test = [] {
    constexpr FixedString str = "test";
    static_assert(str.c_str()[4] == '\0');
    expectEq(str.c_str()[4], '\0');
  };

  "data returns pointer to underlying array"_test = [] {
    constexpr FixedString str = "abc";
    static_assert(str.data()[0] == 'a');
    static_assert(str.data()[1] == 'b');
    static_assert(str.data()[2] == 'c');
  };

  "begin and end iterators"_test = [] {
    constexpr FixedString str = "xyz";

    SizeT count = 0;
    for (auto it = str.begin(); it != str.end(); ++it) {
      ++count;
    }
    expectEq(count, 3u);

    expectEq(*str.begin(), 'x');
    expectEq(*(str.end() - 1), 'z');
  };

  "cbegin and cend iterators"_test = [] {
    constexpr FixedString str = "abc";
    expectEq(*str.cbegin(), 'a');
    expectEq(*(str.cend() - 1), 'c');
  };

  "range-based for loop"_test = [] {
    constexpr FixedString str = "hello";
    char result[6] = {};
    SizeT i = 0;
    for (char c : str) {
      result[i++] = c;
    }
    expectEq(result[0], 'h');
    expectEq(result[1], 'e');
    expectEq(result[2], 'l');
    expectEq(result[3], 'l');
    expectEq(result[4], 'o');
  };

  "mutable iteration"_test = [] {
    FixedString str = "hello";
    for (char& c : str) {
      c = c - 32;  // to uppercase
    }
    expectEq(str, FixedString("HELLO"));
  };

  "concatenation of two strings"_test = [] {
    constexpr FixedString a = "hello";
    constexpr FixedString b = " world";
    constexpr auto result = a + b;

    static_assert(result.size() == 11);
    static_assert(result == FixedString("hello world"));
    expectEq(result.size(), 11u);
    expectEq(result[0], 'h');
    expectEq(result[5], ' ');
    expectEq(result[6], 'w');
  };

  "concatenation with empty string"_test = [] {
    constexpr FixedString a = "test";
    constexpr FixedString b = "";
    constexpr auto result = a + b;

    static_assert(result.size() == 4);
    static_assert(result == a);
    expectEq(result[0], 't');
    expectEq(result[3], 't');
  };

  "equality comparison"_test = [] {
    constexpr FixedString a = "abc";
    constexpr FixedString b = "abc";
    constexpr FixedString c = "xyz";
    constexpr FixedString d = "ab";

    static_assert(a == b);
    static_assert(!(a == c));
    static_assert(!(a == d));  // different sizes

    expectTrue(a == b);
    expectFalse(a == c);
    expectFalse(a == d);
  };

  "inequality comparison"_test = [] {
    constexpr FixedString a = "abc";
    constexpr FixedString b = "xyz";
    static_assert(a != b);
    expectTrue(a != b);
  };

  "less than comparison"_test = [] {
    constexpr FixedString a = "abc";
    constexpr FixedString b = "abd";
    constexpr FixedString c = "ab";
    constexpr FixedString d = "abcd";

    static_assert(a < b);      // lexicographic
    static_assert(c < a);      // prefix is less
    static_assert(a < d);      // prefix is less
    static_assert(!(b < a));

    expectTrue(a < b);
    expectTrue(c < a);
    expectTrue(a < d);
  };

  "greater than comparison"_test = [] {
    constexpr FixedString a = "xyz";
    constexpr FixedString b = "abc";
    static_assert(a > b);
    expectTrue(a > b);
  };

  "less than or equal comparison"_test = [] {
    constexpr FixedString a = "abc";
    constexpr FixedString b = "abc";
    constexpr FixedString c = "abd";
    static_assert(a <= b);
    static_assert(a <= c);
    expectTrue(a <= b);
    expectTrue(a <= c);
  };

  "greater than or equal comparison"_test = [] {
    constexpr FixedString a = "xyz";
    constexpr FixedString b = "xyz";
    constexpr FixedString c = "abc";
    static_assert(a >= b);
    static_assert(a >= c);
    expectTrue(a >= b);
    expectTrue(a >= c);
  };

  "use as NTTP"_test = [] {
    // The primary use case: FixedString as non-type template parameter
    []<FixedString Str>() {
      static_assert(Str.size() == 4);
      expectEq(Str.data()[0], 't');
    }.template operator()<"test">();
  };

  "wide character support"_test = [] {
    constexpr FixedString<wchar_t, 5> wstr(L"hello");
    static_assert(wstr.size() == 5);
    expectEq(wstr.data()[0], L'h');
    expectEq(wstr.data()[4], L'o');
  };

  "char8_t support"_test = [] {
    constexpr FixedString<char8_t, 3> u8str(u8"abc");
    static_assert(u8str.size() == 3);
    expectEq(u8str.data()[0], u8'a');
  };

  "char16_t support"_test = [] {
    constexpr FixedString<char16_t, 3> u16str(u"abc");
    static_assert(u16str.size() == 3);
    expectEq(u16str.data()[0], u'a');
  };

  "char32_t support"_test = [] {
    constexpr FixedString<char32_t, 3> u32str(U"abc");
    static_assert(u32str.size() == 3);
    expectEq(u32str.data()[0], U'a');
  };

  "multiple concatenations"_test = [] {
    constexpr FixedString a = "a";
    constexpr FixedString b = "b";
    constexpr FixedString c = "c";
    constexpr auto result = a + b + c;

    static_assert(result.size() == 3);
    static_assert(result == FixedString("abc"));
    expectEq(result[0], 'a');
    expectEq(result[1], 'b');
    expectEq(result[2], 'c');
  };

  "chained literal concatenation"_test = [] {
    constexpr FixedString a = "hello";
    constexpr auto result = "(" + a + ")";

    static_assert(result.size() == 7);
    static_assert(result == FixedString("(hello)"));
  };

  "concat with string literal on right"_test = [] {
    constexpr FixedString a = "hello";
    constexpr auto result = a + " world";

    static_assert(result.size() == 11);
    static_assert(result == FixedString("hello world"));
    expectEq(result[5], ' ');
    expectEq(result[6], 'w');
  };

  "concat with string literal on left"_test = [] {
    constexpr FixedString a = "world";
    constexpr auto result = "hello " + a;

    static_assert(result.size() == 11);
    static_assert(result == FixedString("hello world"));
    expectEq(result[0], 'h');
    expectEq(result[6], 'w');
  };

  "subscript operator"_test = [] {
    constexpr FixedString str = "hello";
    static_assert(str[0] == 'h');
    static_assert(str[4] == 'o');

    expectEq(str[0], 'h');
    expectEq(str[1], 'e');
    expectEq(str[4], 'o');
  };

  "mutable subscript operator"_test = [] {
    FixedString str = "hello";
    str[0] = 'H';
    expectEq(str[0], 'H');
    expectEq(str.data()[0], 'H');
  };

  "length alias for size"_test = [] {
    constexpr FixedString str = "test";
    static_assert(str.length() == str.size());
    static_assert(str.length() == 4);
    expectEq(str.length(), 4u);
  };

  "consteval construction ensures compile-time evaluation"_test = [] {
    // This test verifies the consteval constructor works
    constexpr FixedString str = "compile time";
    static_assert(str.size() == 12);
    expectEq(str.size(), 12u);
  };

  return TestRegistry::result();
}
