#pragma once
#include <experimental/meta>
#include <array>
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <vector>

namespace tempura {

// ─── Compile-time string ────────────────────────────────────────────────────
//
// Structural type usable as an NTTP. Stores N chars including the null
// terminator — FixedString{"x"} has N=2. Implicit conversion to string_view
// strips the null.

template <std::size_t N>
struct FixedString {
  char data[N]{};
  constexpr FixedString() = default;
  constexpr FixedString(const char (&s)[N]) {
    for (std::size_t i = 0; i < N; ++i) data[i] = s[i];
  }
  constexpr operator std::string_view() const { return {data, N - 1}; }
  constexpr std::size_t size() const { return N - 1; }
  friend constexpr bool operator==(const FixedString&, const FixedString&) = default;
  friend constexpr auto operator<=>(const FixedString&, const FixedString&) = default;
};

template <std::size_t N>
FixedString(const char (&)[N]) -> FixedString<N>;

// ─── Operator notation descriptors ──────────────────────────────────────────
//
// Ops declare their rendering style via a static `notation` member.
// Concepts below dispatch toString generically based on the notation type.
//
// Precedence lives inside the notation rather than in an external table because
// it's intrinsic to how the operator renders — two operators at the same level
// share parenthesization behavior, and the notation already captures the symbol
// and associativity. External precedence would force a parallel mapping that
// drifts out of sync. Domains define named constants for their levels (e.g.
// kAdditivePrec, kMultiplicativePrec in math.h).

struct InfixNotation {
  std::string_view symbol;   // " + ", " * ", etc.
  int precedence;
  bool right_wraps = false;  // right operand wraps at equal precedence
};

struct PrefixNotation {
  std::string_view symbol;   // "-"
  int precedence;
};

struct FunctionNotation {
  std::string_view name;     // "sin", "exp" — always atom precedence
};

template <typename Op>
concept InfixOp = std::same_as<std::remove_cvref_t<decltype(Op::notation)>, InfixNotation>;

template <typename Op>
concept PrefixOp = std::same_as<std::remove_cvref_t<decltype(Op::notation)>, PrefixNotation>;

template <typename Op>
concept FunctionOp = std::same_as<std::remove_cvref_t<decltype(Op::notation)>, FunctionNotation>;

struct ConstantNotation {
  std::string_view symbol;   // "π", "e", "τ", etc.
};

template <typename Op>
concept ConstantOp = std::same_as<std::remove_cvref_t<decltype(Op::notation)>, ConstantNotation>;

// ─── Annotation storage (type-encoded, reflected to info) ────────────────────

template <typename... Entries>
struct AnnotationList {};

using EmptyAnnotations = AnnotationList<>;

consteval std::meta::info mergeAnnotations(std::meta::info a, std::meta::info b) {
  if (a == ^^EmptyAnnotations) return b;
  if (b == ^^EmptyAnnotations) return a;
  auto a_entries = std::meta::template_arguments_of(a);
  auto b_entries = std::meta::template_arguments_of(b);
  // Collect unique entries — dedup by reflection identity
  std::vector<std::meta::info> merged;
  for (auto e : a_entries) merged.push_back(e);
  for (auto e : b_entries) {
    bool dup = false;
    for (auto m : merged) if (m == e) { dup = true; break; }
    if (!dup) merged.push_back(e);
  }
  // Sort for canonical ordering (display_string_of gives stable lexicographic key)
  std::sort(merged.begin(), merged.end(), [](auto x, auto y) {
    return std::meta::display_string_of(x) < std::meta::display_string_of(y);
  });
  return std::meta::substitute(^^AnnotationList, merged);
}

// ─── The expression ──────────────────────────────────────────────────────────
//
// One type. Every symbolic expression is an Expr. The tree structure and
// annotations live as std::meta::info values — consteval-only, zero runtime cost.

struct Expr {
  std::meta::info tree;
  std::meta::info meta = ^^EmptyAnnotations;
};

// ─── Type-level AST (exists only as info, instantiated at splice boundary) ───

template <typename Op, typename... Children>
struct App {};

// ─── Leaf types ──────────────────────────────────────────────────────────────
//
// Symbol identity comes from the Tag type — a unique lambda per declaration
// site. Symbols are anonymous by default; Named<Symbol<Tag>, "x"> wraps one
// with a display name for toString.

template <typename Tag = decltype([] {})>
struct Symbol {
  template <typename V>
  constexpr auto operator=(V value) const;
};

template <auto V>
struct Constant {
  static constexpr auto value = V;
};

template <auto V>
inline constexpr Constant<V> lit{};

// Structural placeholder for pattern-matching rewrite rules.
// Only appears in pattern/replacement info values — never in user-facing expressions.
template <typename Tag>
struct PatternVar {};

// ─── Named wrapper ──────────────────────────────────────────────────────────
//
// Named<Sym, Name> annotates a symbol with a display name. It's a pure
// annotation — the tree always contains bare Symbol<Tag> nodes. The name
// mapping lives in Expr.meta as Named<Symbol<Tag>, Name> entries, which
// toString reads via injectNames. Eval and diff never see Named.
//
// Created via pipe:   Symbol{} | name<"x">
// Or factory:         sym<"x">()

template <typename Sym, FixedString Name>
struct Named {
  template <typename V>
  constexpr auto operator=(V value) const { return Sym{} = value; }
};

// Pipe tag — name<"x"> is a zero-size value used with operator|
template <FixedString N>
struct NameAnnotation {};

template <FixedString N>
inline constexpr NameAnnotation<N> name{};

template <typename Tag, FixedString N>
consteval Named<Symbol<Tag>, N> operator|(Symbol<Tag>, NameAnnotation<N>) {
  return {};
}

// ─── Binder / BinderPack ─────────────────────────────────────────────────────
//
// BinderPack aggregates typed binders via multiple inheritance. Overload
// resolution on operator[] dispatches to the correct binder at compile time —
// zero-cost lookup, heterogeneous return types, no runtime map.

template <typename Tag, typename V>
struct Binder {
  V value;
  constexpr auto operator[](Symbol<Tag>) const { return value; }
  // Accept Named<Symbol<Tag>, N> — transparently unwraps to the inner symbol
  template <FixedString N>
  constexpr auto operator[](Named<Symbol<Tag>, N>) const { return value; }
};

template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... b) : Binders{b}... {}
  using Binders::operator[]...;
};

template <typename... Binders>
BinderPack(Binders...) -> BinderPack<Binders...>;

template <typename Tag>
template <typename V>
constexpr auto Symbol<Tag>::operator=(V value) const {
  return Binder<Tag, V>{value};
}

// ─── toInfo / metaOf ─────────────────────────────────────────────────────────
//
// toInfo: converts any symbolic value to its tree info. ADL-extensible —
// domains add overloads for their leaf types without touching core.
//
// metaOf: extracts annotations. Named symbols inject HasSymbol<Name>.

template <typename Tag>
consteval std::meta::info toInfo(Symbol<Tag>) { return ^^Symbol<Tag>; }

template <auto V>
consteval std::meta::info toInfo(Constant<V>) { return ^^Constant<V>; }

consteval std::meta::info toInfo(Expr e) { return e.tree; }

// Named → bare symbol in tree, name goes to meta only
template <typename Sym, FixedString N>
consteval std::meta::info toInfo(Named<Sym, N>) { return toInfo(Sym{}); }

template <typename T>
consteval std::meta::info metaOf(T) { return ^^EmptyAnnotations; }

consteval std::meta::info metaOf(Expr e) { return e.meta; }

// Named symbols carry the symbol-name mapping as annotation
template <typename Sym, FixedString N>
consteval std::meta::info metaOf(Named<Sym, N>) {
  return ^^AnnotationList<Named<Sym, N>>;
}

// ─── Symbolic concept ────────────────────────────────────────────────────────
//
// Anything with a toInfo overload participates in expressions. Domains extend
// this by providing toInfo for their leaf types — no concept modification needed.

template <typename T>
concept Symbolic = requires(T t) {
  { toInfo(t) } -> std::same_as<std::meta::info>;
};

// ─── Info-domain query helpers ───────────────────────────────────────────────

namespace detail {

// For type-parameter templates (App, Symbol)
template <typename T, template <typename...> typename Template>
consteval bool isSpecOfType() {
  constexpr auto info = std::meta::remove_cvref(^^T);
  if constexpr (!std::meta::has_template_arguments(info)) return false;
  else return std::meta::template_of(info) == ^^Template;
}

// For mixed/NTTP templates (Named<Sym, Name>, Constant<V>)
template <typename T>
consteval bool isSpecOfNttp(std::meta::info tmpl) {
  constexpr auto info = std::meta::remove_cvref(^^T);
  if constexpr (!std::meta::has_template_arguments(info)) return false;
  else return std::meta::template_of(info) == tmpl;
}

}  // namespace detail

template <typename T>
constexpr bool is_app_v = detail::isSpecOfType<T, App>();

template <typename T>
constexpr bool is_symbol_v = detail::isSpecOfType<T, Symbol>();

template <typename T>
constexpr bool is_constant_v = detail::isSpecOfNttp<T>(^^Constant);

template <typename T>
constexpr bool is_named_v = detail::isSpecOfNttp<T>(^^Named);

// ─── Factory helpers ─────────────────────────────────────────────────────────
//
// std::meta::substitute(^^App, {^^Op, lhs_info, rhs_info}) creates the info
// for App<Op, Lhs, Rhs> WITHOUT instantiating the type. Only the final splice
// in eval triggers instantiation.

consteval Expr makeBinary(std::meta::info op,
                          std::meta::info lhs, std::meta::info lhs_meta,
                          std::meta::info rhs, std::meta::info rhs_meta) {
  std::array args{op, lhs, rhs};
  return Expr{
    .tree = std::meta::substitute(^^App, args),
    .meta = mergeAnnotations(lhs_meta, rhs_meta)
  };
}

consteval Expr makeUnary(std::meta::info op,
                         std::meta::info operand, std::meta::info operand_meta) {
  std::array args{op, operand};
  return Expr{
    .tree = std::meta::substitute(^^App, args),
    .meta = operand_meta
  };
}

consteval Expr makeNullary(std::meta::info op) {
  std::array args{op};
  return Expr{
    .tree = std::meta::substitute(^^App, args),
    .meta = ^^EmptyAnnotations
  };
}

// ─── Symbol factory ─────────────────────────────────────────────────────────
//
// sym<"x">() creates a named symbol — unique per call site via lambda tag,
// with a display name for toString. Uses the pipe: Symbol<Tag>{} | name<Name>.
// Anonymous symbols use Symbol<> directly.

template <FixedString Name, typename Tag = decltype([] {})>
consteval auto sym() { return Symbol<Tag>{} | name<Name>; }

}  // namespace tempura
