#pragma once

#include <type_traits>

#include "meta/tags.h"
#include "meta/type_list.h"
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
//     - Atom<Id, Strategy>: A leaf node (symbol, constant, literal)
//     - Expression<Op, Args...>: An internal node (addition, multiplication, etc.)
//
//   The "Strategy" determines how an Atom behaves during evaluation:
//     - Lookup: Variable - look up value in bindings (Symbol<X>)
//     - Intrinsic<T>: Embedded value - the atom carries its value (Literal<double>)
//     - Sample<D>: Random variable - sample from distribution D
//     - Compute<E>: Deterministic function - compute from expression E
//
// Example:
//   Symbol<struct X> x;           // x is a lookup variable
//   auto expr = x * x + one;      // expr's type encodes the entire tree:
//                                 // Expression<AddOp,
//                                 //   Expression<MulOp, Atom<X,Lookup>, Atom<X,Lookup>>,
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
// Binding Strategies — the "effects" that atoms can have
// ============================================================================
//
// A "Strategy" determines how an Atom gets its value during evaluation.
// Think of it as the "behavior" of a leaf node. This is the key to unifying
// symbols, literals, random variables, and computed values under one type.
//
// Why strategies instead of separate types?
//   - Uniform handling in generic code (traversals, interpreters)
//   - Extensible: add new behaviors without changing the core Atom type
//   - Strategy can carry data (distributions, expressions) without overhead
//
// The four built-in strategies:
//
//   Lookup     - A free variable. Value must be provided at evaluation time.
//                Example: Symbol<struct X> x; evaluate(x + 1, x = 5.0);
//
//   Intrinsic  - Value is baked into the atom. No lookup needed.
//                Example: Literal<double> lit{3.14}; // carries 3.14
//
//   Sample     - Random variable. Draws from a probability distribution.
//                Example: RandomVar<struct W, Normal> w; // w ~ Normal()
//
//   Compute    - Deterministic function of other variables.
//                Example: DeterministicVar<struct Y, E> y; // y = f(x)

// Look up value in bindings at evaluation time
struct Lookup {};

// Value embedded directly in the atom (no lookup needed)
template <typename T>
struct Intrinsic {
  T value;
  constexpr Intrinsic() = default;
  constexpr Intrinsic(T v) : value{v} {}
};

// Sample from distribution D (for Bayesian modeling)
template <typename D>
struct Sample {
  [[no_unique_address]] D dist_;
  constexpr Sample() = default;
  constexpr Sample(D d) : dist_{d} {}
};

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
// Atom<Id, Strategy> is the universal leaf node in the expression tree.
//
// Type parameters:
//   Id       - A unique type that identifies this atom. Used for:
//              - Type-safe bindings (x = 5 only binds to x, not y)
//              - DAG representation (detecting shared subexpressions)
//              For anonymous atoms (like Literal), Id can be void.
//
//   Strategy - How this atom gets its value (Lookup, Intrinsic, Sample, Compute)
//
// The combination of Id + Strategy gives us all leaf types:
//   Symbol<X>        = Atom<X, Lookup>           - Variable, needs binding
//   Literal<T>       = Atom<void, Intrinsic<T>>  - Embedded runtime value
//   RandomVar<X, D>  = Atom<X, Sample<D>>        - Random variable ~ D
//   DeterministicVar = Atom<X, Compute<E>>       - Computed from expression
//
// Memory layout:
//   [[no_unique_address]] ensures empty strategies (like Lookup) take no space.
//   Atom<X, Lookup> has sizeof == 1 (minimum for addressability).

template <typename Id, typename Strategy>
struct Atom : SymbolicTag {
  using id_type = Id;
  using strategy_type = Strategy;

  [[no_unique_address]] Strategy strategy_;

  constexpr Atom() = default;
  constexpr Atom(Strategy s) : strategy_{s} {}

  // For atoms with identity, get a Symbol referencing this atom's Id.
  // This enables DAG patterns: let<X>(expr) creates an atom, x.sym() references it.
  static constexpr auto sym()
    requires(!std::is_void_v<Id>)
  {
    return Atom<Id, Lookup>{};
  }

  // Enable binding syntax for evaluation: x = value
  // This operator is only valid for Lookup strategy (symbols).
  // Usage: evaluate(expr, x = 5.0, y = 3.0)
  constexpr auto operator=(auto value) const
    requires std::is_same_v<Strategy, Lookup>
  {
    return TypeValueBinder{Atom<Id, Lookup>{}, value};
  }
};

// ============================================================================
// Atom Aliases — the user-facing vocabulary
// ============================================================================
//
// These type aliases provide the intuitive interface for building expressions.
// Under the hood, they're all just Atom<Id, Strategy> with different configs.

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
using Symbol = Atom<Id, Lookup>;

// Literal - A runtime value embedded directly in the expression.
//
// Unlike Constant, the value isn't known at compile time - it's stored in
// the Intrinsic strategy. Has no Id (void) since it's anonymous.
//
// Usage:
//   Literal<double> lit{Intrinsic{3.14}};
//   auto expr = lit + x;
template <typename T>
using Literal = Atom<void, Intrinsic<T>>;

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

// RandomVar - A random variable that samples from distribution D.
//
// For Bayesian modeling: represents a quantity with uncertainty.
// The Id identifies which random variable this is (for tracking in inference).
//
// Usage:
//   RandomVar<struct Weight, Normal> w;  // w ~ Normal()
template <typename Id, typename DistT>
using RandomVar = Atom<Id, Sample<DistT>>;

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
//   is_literal_v<T>    - Is T a runtime literal value?
//   has_identity_v<T>  - Does T have a non-void Id? (named vs anonymous)
//
// Accessor traits:
//   get_id_t<T>        - Extract the Id type from an Atom
//   get_strategy_t<T>  - Extract the Strategy type from an Atom
//   get_op_t<T>        - Extract the Op type from an Expression
//   get_args_t<T>      - Extract child types as TypeList from an Expression

// Is this type an Atom?
template <typename T>
struct IsAtom : std::false_type {};

template <typename Id, typename S>
struct IsAtom<Atom<Id, S>> : std::true_type {};

template <typename T>
constexpr bool is_atom_v = IsAtom<T>::value;

// Is this type an Expression?
template <typename T>
struct IsExpression : std::false_type {};

template <typename Op, typename... Args>
struct IsExpression<Expression<Op, Args...>> : std::true_type {};

// Handle cv-qualifications
template <typename T>
struct IsExpression<const T> : IsExpression<T> {};

template <typename T>
struct IsExpression<volatile T> : IsExpression<T> {};

template <typename T>
struct IsExpression<const volatile T> : IsExpression<T> {};

template <typename T>
constexpr bool is_expression_v = IsExpression<T>::value;

// Is this type a Constant?
template <typename T>
struct IsConstant : std::false_type {};

template <auto V>
struct IsConstant<Constant<V>> : std::true_type {};

template <typename T>
constexpr bool is_constant_v = IsConstant<T>::value;

// Is this type a Fraction?
template <typename T>
struct IsFraction : std::false_type {};

template <long long N, long long D>
struct IsFraction<Fraction<N, D>> : std::true_type {};

template <typename T>
constexpr bool is_fraction_v = IsFraction<T>::value;

// Is this type a Literal?
template <typename T>
struct IsLiteral : std::false_type {};

template <typename U>
struct IsLiteral<Atom<void, Intrinsic<U>>> : std::true_type {};

template <typename T>
constexpr bool is_literal_v = IsLiteral<T>::value;

// Does this atom have an identity (non-void Id)?
template <typename T>
struct HasIdentity : std::false_type {};

template <typename Id, typename S>
struct HasIdentity<Atom<Id, S>> : std::bool_constant<!std::is_void_v<Id>> {};

template <typename T>
constexpr bool has_identity_v = HasIdentity<T>::value;

// Is this a terminal (leaf) node?
// Terminals are anything that's not an Expression
template <typename T>
constexpr bool is_terminal_v = !is_expression_v<T>;

// Get the Id type from an Atom
template <typename T>
struct GetId;

template <typename Id, typename S>
struct GetId<Atom<Id, S>> {
  using type = Id;
};

template <typename T>
using get_id_t = typename GetId<T>::type;

// Get the Strategy type from an Atom
template <typename T>
struct GetStrategy;

template <typename Id, typename S>
struct GetStrategy<Atom<Id, S>> {
  using type = S;
};

template <typename T>
using get_strategy_t = typename GetStrategy<T>::type;

// Get the Op type from an Expression
template <typename T>
struct GetOp;

template <typename Op, typename... Args>
struct GetOp<Expression<Op, Args...>> {
  using type = Op;
};

template <typename T>
struct GetOp<const T> : GetOp<T> {};

template <typename T>
using get_op_t = typename GetOp<T>::type;

// Get arguments as TypeList from an Expression
template <typename T>
struct GetArgs;

template <typename Op, typename... Args>
struct GetArgs<Expression<Op, Args...>> {
  using type = TypeList<Args...>;
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

}  // namespace tempura::symbolic4
