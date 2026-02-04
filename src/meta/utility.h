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

// --- TypeIdentity ---
template <typename T>
struct TypeIdentity {
  using Type = T;
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

// --- Type Traits (using compiler intrinsics) ---

template <typename T>
inline constexpr bool isConst = false;

template <typename T>
inline constexpr bool isConst<const T> = true;

template <typename T>
inline constexpr bool isReference = false;

template <typename T>
inline constexpr bool isReference<T&> = true;

template <typename T>
inline constexpr bool isReference<T&&> = true;

template <typename T>
inline constexpr bool isEmpty = __is_empty(T);

template <typename T>
inline constexpr bool isFinal = __is_final(T);

template <typename T>
inline constexpr bool isTriviallyDefaultConstructible =
    __is_trivially_constructible(T);

template <typename T>
inline constexpr bool isTriviallyCopyConstructible =
    __is_trivially_constructible(T, const T&);

template <typename T>
inline constexpr bool isTriviallyMoveConstructible =
    __is_trivially_constructible(T, T&&);

template <typename T>
inline constexpr bool isTriviallyDestructible = __has_trivial_destructor(T);

// --- RemoveConst ---

template <typename T>
struct RemoveConst {
  using Type = T;
};

template <typename T>
struct RemoveConst<const T> {
  using Type = T;
};

template <typename T>
using RemoveConstT = typename RemoveConst<T>::Type;

// --- RemoveVolatile ---

template <typename T>
struct RemoveVolatile {
  using Type = T;
};

template <typename T>
struct RemoveVolatile<volatile T> {
  using Type = T;
};

template <typename T>
using RemoveVolatileT = typename RemoveVolatile<T>::Type;

// --- RemoveCV ---

template <typename T>
using RemoveCvT = RemoveConstT<RemoveVolatileT<T>>;

// --- RemoveReference ---

template <typename T>
struct RemoveReference {
  using Type = T;
};

template <typename T>
struct RemoveReference<T&> {
  using Type = T;
};

template <typename T>
struct RemoveReference<T&&> {
  using Type = T;
};

template <typename T>
using RemoveReferenceT = typename RemoveReference<T>::Type;

// --- RemoveCvRef ---

template <typename T>
using RemoveCvRefT = RemoveCvT<RemoveReferenceT<T>>;

// --- Decay ---
// Simplified decay: removes const, volatile, and references.
// Does NOT handle array-to-pointer or function-to-pointer decay.
// For full std::decay behavior, use <type_traits>.

template <typename T>
using DecayT = RemoveCvRefT<T>;

// --- Type Concepts ---

// Canonical type form: not const, not reference
// Required for type equality to work correctly in type-level computation
template <typename T>
concept CanonicalType = !isConst<T> && !isReference<T>;

// Stateless tag type suitable for compile-time computation
// - Empty: pure type-level tag with no data members
// - Final: prevents inheritance issues with type identity
// - Trivially constructible/destructible: works freely in consteval contexts
template <typename T>
concept TagType =
    CanonicalType<T> && isEmpty<T> && isFinal<T> &&
    isTriviallyDefaultConstructible<T> && isTriviallyCopyConstructible<T> &&
    isTriviallyMoveConstructible<T> && isTriviallyDestructible<T>;

}  // namespace tempura
