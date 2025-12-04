#pragma once

// Speed up compile times by not using the standard library and instead writing
// our own utility functions and types.

namespace tempura {

// --- SizeT ---

using SizeT = decltype(sizeof(int));

// --- IntegerSequence ---

template <class T, T...>
struct IntegerSequence {};
template <SizeT... Ns>
using IndexSequence = IntegerSequence<SizeT, Ns...>;
template <SizeT N>
using MakeIndexSequence =
#if defined(__clang__) || defined(_MSC_VER)
    __make_integer_seq<IntegerSequence, SizeT, N>;
#else
    IndexSequence<__integer_pack(N)...>;
#endif

// --- kDeclVal ---

template <typename T>
auto kDeclVal() -> T&&;

// --- IsSame ---

template <typename T, typename U>
inline constexpr bool isSame = false;

template <typename T>
inline constexpr bool isSame<T, T> = true;

template <typename T, typename U>
concept SameAs = isSame<T, U>;

// --- Swap ---
template <typename T>
constexpr void swap(T& a, T& b) noexcept {
  T temp = static_cast<T&&>(a);
  a = static_cast<T&&>(b);
  b = static_cast<T&&>(temp);
}

// --- Type Traits ---

struct TrueType {
  static constexpr bool value = true;
};
struct FalseType {
  static constexpr bool value = false;
};

// --- Conditional ---

template <bool Cond, typename T, typename F>
struct Conditional {
  using Type = T;
};

template <typename T, typename F>
struct Conditional<false, T, F> {
  using Type = F;
};

// --- Concepts ---

template <typename RangeT>
concept Range = requires(RangeT range) {
  range.begin();
  range.end();
};

template <typename Func, typename... Args>
concept Invocable = requires(Func func, Args... args) {
  { func(args...) };
};

template <typename B>
auto testPointerConversion(const volatile B*) -> TrueType;
template <typename>
auto testPointerConversion(const volatile void*) -> FalseType;
template <typename B, typename D>
auto testIsBaseOf(int)
    -> decltype(testPointerConversion<B>(static_cast<D*>(nullptr)));
template <typename, typename>
auto testIsBaseOf(...) -> FalseType;

template <typename D, typename B>
concept DerivedFrom = decltype(testIsBaseOf<B, D>(0))::value;

template <typename T, typename U>
concept ConstructibleFrom = requires {
  { T{kDeclVal<U>()} };
};

}  // namespace tempura
