#pragma once

#include <tuple>
#include <type_traits>

#include "meta/tags.h"
#include "meta/utility.h"
#include "symbolic4/compressed.h"

// ============================================================================
// core.h - The fundamental building blocks of symbolic expressions
// ============================================================================
//
// Philosophy: "Types ARE the representation"
//
// In most symbolic math libraries, expressions are runtime data structures -
// trees of pointers, nodes, and type tags. This design takes a different
// approach: the expression IS the type. The AST lives in the type system.
//
// Benefits:
//   1. Zero runtime overhead - the compiler does all the work
//   2. Type-safe operations - invalid expressions are compile errors
//   3. Optimal code generation - compilers can inline and optimize everything
//   4. Constexpr evaluation - expressions can be computed at compile time
//
// Core Abstraction:
//   Everything in an expression tree is either:
//     - Atom<Id, Effect>: A leaf node (symbol, random variable, deterministic)
//     - Constant<V>, Fraction<N,D>: Compile-time constant leaves
//     - Expression<Op, Args...>: An internal node (addition, multiplication, etc.)
//
//   The "Effect" determines how an Atom behaves during evaluation:
//     - Free: Variable - look up value in bindings (Symbol<X>)
//     - (deleted: Embedded<T> — replaced by Constant<V> + binders)
//     - Sample<D>: Random variable - sample from distribution D
//     - Compute<E>: Deterministic function - compute from expression E
//
// Example:
//   Symbol<struct X> x;           // x is a free variable
//   auto expr = x * x + one;      // expr's type encodes the entire tree:
//                                 // Expression<AddOp,
//                                 //   Expression<MulOp, Atom<X,Free>, Atom<X,Free>>,
//                                 //   Constant<1>>
//
// ============================================================================

namespace tempura::symbolic4 {

// Base tag for all symbolic types
struct SymbolicTag {};

// Forward declaration for binding support
template <typename SymbolType, typename ValueType>
class TypeValueBinder;

// ============================================================================
// Binding Effects — how atoms produce values during evaluation
// ============================================================================
//
// An "Effect" determines how an Atom gets its value during evaluation.
// Think of it as the "behavior" of a leaf node. This is the key to unifying
// symbols, literals, random variables, and computed values under one type.
//
// Why effects instead of separate types?
//   - Uniform handling in generic code (traversals, interpreters)
//   - Extensible: add new behaviors without changing the core Atom type
//   - Effect can carry data (distributions, expressions) without overhead
//
// The core effects (domain-independent):
//
//   Free       - A free variable. Value must be provided at evaluation time.
//                Example: Symbol<struct X> x; evaluate(x + 1, x = 5.0);
//
//   Compute    - Deterministic function of other variables.
//                Example: DeterministicVar<struct Y, E> y; // y = f(x)
//
// Domain-specific effects (e.g., Sample<D>, IndexedSample<D, Dims>) are defined
// in their respective domain headers (see distributions/effects.h).
//
// Compile-time constants use Constant<V> (no effect, just NTTP).
// Runtime data uses Symbol + binding at eval time.

// Free variable - look up value in bindings at evaluation time
struct Free {};

// Compute from expression E (deterministic transformation)
template <typename E>
struct Compute {
  [[no_unique_address]] E expr_;
  constexpr Compute() = default;
  constexpr Compute(E e) : expr_{e} {}
};

// ============================================================================
// The Unified Atom Type
// ============================================================================
//
// Atom<Id, Effect> is the universal leaf node in the expression tree.
//
// Type parameters:
//   Id     - A unique type that identifies this atom. Used for:
//            - Type-safe bindings (x = 5 only binds to x, not y)
//            - DAG representation (detecting shared subexpressions)
//            Id can be void for anonymous atoms.
//
//   Effect - How this atom gets its value (Free, Compute, or domain-specific)
//
// The combination of Id + Effect gives us all leaf types:
//   Symbol<X>        = Atom<X, Free>           - Variable, needs binding
//   DeterministicVar = Atom<X, Compute<E>>     - Computed from expression
//   Atom<X, Sample<D>>                         - Random variable ~ D (domain effect)
//
// Memory layout:
//   [[no_unique_address]] ensures empty effects (like Free) take no space.
//   Atom<X, Free> has sizeof == 1 (minimum for addressability).

template <typename Id, typename Effect>
struct Atom : SymbolicTag {
  using id_type = Id;
  using effect_type = Effect;

  [[no_unique_address]] Effect effect_;

  constexpr Atom() = default;
  constexpr Atom(Effect e) : effect_{e} {}

  // For atoms with identity, get a Symbol referencing this atom's Id.
  // This enables DAG patterns: let<X>(expr) creates an atom, x.sym() references it.
  static constexpr auto sym()
    requires(!std::is_void_v<Id>)
  {
    return Atom<Id, Free>{};
  }

  // Enable binding syntax for evaluation: x = value
  // This operator is only valid for Free effect (symbols).
  // Usage: evaluate(expr, x = 5.0, y = 3.0)
  constexpr auto operator=(auto value) const
    requires std::is_same_v<Effect, Free>
  {
    return TypeValueBinder{Atom<Id, Free>{}, value};
  }
};

// ============================================================================
// Atom Aliases — the user-facing vocabulary
// ============================================================================
//
// These type aliases provide the intuitive interface for building expressions.
// Under the hood, they're all just Atom<Id, Effect> with different configs.

// Symbol - A named variable that must be bound at evaluation time.
//
// Usage:
//   Symbol<struct X> x;        // Named: X is the unique identity type
//   Symbol x;                  // Anonymous: uses lambda for unique type
//   evaluate(x + 1, x = 5.0);  // Bind x to 5.0 during evaluation
//
// The Id type serves two purposes:
//   1. Type safety: x = 5 cannot accidentally bind to y
//   2. Identity: allows detecting when the same symbol appears multiple times
//
// The lambda trick (decltype([] {})) generates a unique type at each usage.
template <typename Id = decltype([] {})>
using Symbol = Atom<Id, Free>;

// Constant - A compile-time constant value encoded in the type itself.
//
// The value is a non-type template parameter, so "5" and "6" are different
// types. This enables compile-time optimization of constant expressions.
//
// Usage:
//   Constant<5> five;
//   auto expr = x + five;  // or just: x + Constant<5>{}
//   static_assert(Constant<5>::value == 5);
template <auto V>
struct Constant : SymbolicTag {
  static constexpr auto value = V;
};

// Fraction - A compile-time rational number (avoids floating-point in types).
//
// Represents N/D exactly, automatically reduced to lowest terms.
// Useful when you need exact rational arithmetic before final evaluation.
//
// Usage:
//   Fraction<1, 2> half;   // 1/2, stored as numerator=1, denominator=2
//   Fraction<-3, 6> neg;   // Auto-reduces to -1/2
//   static_assert(Fraction<1,2>::value == 0.5);
template <long long N, long long D = 1>
struct Fraction : SymbolicTag {
  static_assert(D != 0, "Denominator cannot be zero");

  // GCD for reduction to lowest terms
  static constexpr auto gcdImpl(long long a, long long b) -> long long {
    return b == 0 ? a : gcdImpl(b, a % b);
  }
  static constexpr auto absVal(long long x) -> long long { return x < 0 ? -x : x; }

  static constexpr auto g = gcdImpl(absVal(N), absVal(D));
  static constexpr auto sign = ((N < 0) != (D < 0)) ? -1 : 1;

  static constexpr auto numerator = sign * absVal(N) / g;
  static constexpr auto denominator = absVal(D) / g;

  static constexpr double value =
      static_cast<double>(numerator) / static_cast<double>(denominator);
};

// DeterministicVar - A computed value (deterministic function of other vars).
//
// Represents a named intermediate computation. Useful for:
//   - Caching shared subexpressions
//   - Naming parts of a model for inspection
//
// Usage:
//   DeterministicVar<struct Y, decltype(x * x)> y_squared;
template <typename Id, typename ExprT>
using DeterministicVar = Atom<Id, Compute<ExprT>>;

// ============================================================================
// Compound Expressions (internal nodes)
// ============================================================================
//
// Expression<Op, Args...> represents an internal node in the expression tree.
// It combines an operator (Op) with its arguments (Args...).
//
// Type parameters:
//   Op     - The operation type (AddOp, MulOp, SinOp, etc.)
//            Op types are empty tags that also act as callable functors.
//   Args   - The child expressions (must satisfy the Symbolic concept)
//
// Arity (number of arguments) determines expression structure:
//   0 args: Nullary constant (pi, e)
//   1 arg:  Unary operation (sin, cos, neg)
//   2 args: Binary operation (+, -, *, /, pow)
//
// Memory layout:
//   Children are stored in a CompressedTuple, which uses [[no_unique_address]]
//   to eliminate storage for empty children. Most symbolic expressions have
//   empty children (just type information), so this is crucial for efficiency.
//
// Example:
//   Expression<AddOp, Symbol<X>, Constant<1>> is "x + 1"
//   Expression<SinOp, Symbol<X>> is "sin(x)"
//   Expression<PiOp> is "π" (nullary - no children)

// Forward declare Symbolic concept
template <typename T>
concept Symbolic = DerivedFrom<T, SymbolicTag>;

template <typename Op, Symbolic... Args>
struct Expression : SymbolicTag {
  using op_type = Op;
  static constexpr SizeT arity = sizeof...(Args);

  [[no_unique_address]] CompressedTuple<Args...> args_;

  constexpr Expression() = default;
  constexpr Expression(Op, Args... a) : args_{a...} {}
  constexpr explicit Expression(Args... a)
    requires(sizeof...(Args) > 0)
      : args_{a...} {}

  // Access child at index I (0-based, left to right)
  template <SizeT I>
  constexpr auto arg() const {
    return get<I>(args_);
  }
};

// ============================================================================
// Type Traits — derive "kind" from structure
// ============================================================================
//
// These traits allow generic code to inspect and dispatch on expression types.
// Since the AST is encoded in the type system, we need type-level introspection
// rather than runtime checks.
//
// Key traits:
//   is_atom_v<T>       - Is T a leaf node (Atom)?
//   is_expression_v<T> - Is T an internal node (Expression)?
//   is_terminal_v<T>   - Is T a leaf? (same as !is_expression_v<T>)
//   is_constant_v<T>   - Is T a compile-time constant?
//   is_fraction_v<T>   - Is T a compile-time rational?
//   has_identity_v<T>  - Does T have a non-void Id? (named vs anonymous)
//
// Accessor traits:
//   get_id_t<T>        - Extract the Id type from an Atom
//   get_effect_t<T>    - Extract the Effect type from an Atom
//   get_op_t<T>        - Extract the Op type from an Expression
//   get_args_t<T>      - Extract child types as TypeList from an Expression

#include <experimental/meta>

namespace core_traits_detail {

// Check if T (after stripping cv/ref) is a specialization of a given template.
// This overload works for templates with only type parameters.
//
// IMPORTANT: We strip cv in the REFLECTION domain (meta::remove_cvref) rather than
// the TYPE domain (using U = remove_cv_t<T>; ^^U) because clang-p2996 reflects
// local type aliases as opaque names — ^^U doesn't see through to the underlying
// template specialization. Working in the info domain avoids this.
template <typename T, template <typename...> typename Template>
consteval bool isSpecOf() {
  constexpr auto info = std::meta::remove_cvref(^^T);
  if constexpr (!std::meta::has_template_arguments(info)) return false;
  else return std::meta::template_of(info) == ^^Template;
}

// Check if T is a specialization of any template via its reflected info.
// Works for NTTP templates (Constant<auto V>, Fraction<N,D>, SimplexTransform<K>, etc.)
// where the template can't be passed as template<typename...> typename.
template <typename T>
consteval bool isSpecOf(std::meta::info tmpl) {
  constexpr auto info = std::meta::remove_cvref(^^T);
  if constexpr (!std::meta::has_template_arguments(info)) return false;
  else return std::meta::template_of(info) == tmpl;
}

}  // namespace core_traits_detail

// Is this type an Atom?
template <typename T>
constexpr bool is_atom_v = core_traits_detail::isSpecOf<T, Atom>();

// Is this type an Expression? (strips cv before checking)
template <typename T>
constexpr bool is_expression_v = core_traits_detail::isSpecOf<T, Expression>();

// Is this type a Constant? (NTTP template — info-based isSpecOf)
template <typename T>
constexpr bool is_constant_v = core_traits_detail::isSpecOf<T>(^^Constant);

// Is this type a Fraction? (NTTP template — info-based isSpecOf)
template <typename T>
constexpr bool is_fraction_v = core_traits_detail::isSpecOf<T>(^^Fraction);

// Does this atom have a non-void Id?
template <typename T>
consteval bool hasIdentityImpl() {
  if constexpr (!is_atom_v<T>) return false;
  else return std::meta::template_arguments_of(^^T)[0] != ^^void;
}

template <typename T>
constexpr bool has_identity_v = hasIdentityImpl<T>();

// Is this a terminal (leaf) node?
template <typename T>
constexpr bool is_terminal_v = !is_expression_v<T>;

// Get the Id type from an Atom (first template argument)
template <typename T>
  requires is_atom_v<T>
using get_id_t = [:std::meta::template_arguments_of(^^T)[0]:];

// Get the Effect type from an Atom (second template argument)
template <typename T>
  requires is_atom_v<T>
using get_effect_t = [:std::meta::template_arguments_of(^^T)[1]:];

// Do two atoms share the same Id (regardless of effect)?
template <typename A, typename B>
consteval bool sameAtomIdImpl() {
  if constexpr (!is_atom_v<A> || !is_atom_v<B>) return false;
  else return std::meta::template_arguments_of(^^A)[0]
           == std::meta::template_arguments_of(^^B)[0];
}

template <typename A, typename B>
constexpr bool same_atom_id_v = sameAtomIdImpl<A, B>();

// Get the Op type from an Expression (first template argument)
// Use meta::remove_cvref in the info domain — ^^std::remove_cv_t<T> has the local-alias bug.
template <typename T>
  requires is_expression_v<std::remove_cv_t<T>>
using get_op_t = [:std::meta::template_arguments_of(std::meta::remove_cvref(^^T))[0]:];

// Get arguments as TypeList from an Expression.
// Deprecated: symbolic3 only. New code should use template_arguments_of directly.
template <typename T>
  requires is_expression_v<T>
struct GetArgs {
 private:
  static consteval auto compute() -> std::meta::info {
    auto args = std::meta::template_arguments_of(^^T);
    // Skip first arg (Op), rest are the expression arguments
    std::vector<std::meta::info> expr_args(args.begin() + 1, args.end());
    return std::meta::substitute(^^TypeList, expr_args);
  }
 public:
  using type = [:compute():];
};

template <typename T>
using get_args_t = typename GetArgs<T>::type;

// ============================================================================
// Evaluation Support — TypeValueBinder and BinderPack
// ============================================================================
//
// These types enable the binding syntax: evaluate(expr, x = 5.0, y = 3.0)
//
// How it works:
//   1. x = 5.0 calls Symbol's operator=, which returns TypeValueBinder<X, double>
//   2. Multiple binders are packed into BinderPack via inheritance
//   3. BinderPack uses "using Binders::operator[]..." to inherit all lookups
//   4. bindings[x] calls the correct TypeValueBinder's operator[] via overload resolution
//
// Type-based dispatch:
//   The key insight is that each Symbol<X> has a unique type X. When we look up
//   bindings[x], the compiler selects the overload that matches x's type.
//   This is resolved at compile time - no runtime map lookup needed.
//
// Example:
//   Symbol<struct X> x;
//   Symbol<struct Y> y;
//   auto bindings = BinderPack{x = 5.0, y = 3.0};
//   bindings[x];  // returns 5.0 (calls TypeValueBinder<X,double>::operator[])
//   bindings[y];  // returns 3.0 (calls TypeValueBinder<Y,double>::operator[])

// Maps a unique Symbol type to a runtime value
template <typename SymbolType, typename ValueType>
class TypeValueBinder {
 public:
  using symbol_type = SymbolType;
  using value_type = ValueType;

  constexpr TypeValueBinder(SymbolType, ValueType value) : value_{value} {}

  // Lookup: only matches the exact symbol type
  constexpr auto operator[](SymbolType) const -> ValueType { return value_; }

 private:
  ValueType value_;
};

// Multiple bindings using parameter pack inheritance for type-based dispatch
template <typename... Binders>
struct BinderPack : Binders... {
  constexpr BinderPack(Binders... binders) : Binders{binders}... {}
  using Binders::operator[]...;  // Bring all operator[] overloads into scope
};

// Deduction guide: infer binder types from constructor arguments
template <typename... Symbols, typename... Values>
BinderPack(TypeValueBinder<Symbols, Values>...)
    -> BinderPack<TypeValueBinder<Symbols, Values>...>;

// General deduction guide for mixed binder types (TypeValueBinder + IndexedBinding)
template <typename... Binders>
BinderPack(Binders...) -> BinderPack<Binders...>;

// Convert std::tuple<Binders...> → BinderPack<Binders...>
template <typename... Ts>
auto tupleToBinderPack(const std::tuple<Ts...>& t) {
  return std::apply([](const auto&... bs) { return BinderPack{bs...}; }, t);
}
inline auto tupleToBinderPack(const std::tuple<>&) { return BinderPack<>{}; }

}  // namespace tempura::symbolic4

// Transitional: include probabilistic effects so existing code that includes
// core.h still finds Sample, IndexedSample, and their traits.
// This will be removed once all consumers include effects.h directly.
#include "symbolic4/distributions/effects.h"
