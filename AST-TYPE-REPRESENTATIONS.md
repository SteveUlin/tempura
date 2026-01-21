# AST Type Representations in Modern C++

Reference document for symbolic3 rework - exploring type-level AST representations.

---

## Table of Contents

1. [Boost.YAP: Modern Expression Templates](#1-boostyap-modern-expression-templates)
2. [Mathematical & CS Foundations](#2-mathematical--cs-foundations)
3. [Other Libraries & Approaches](#3-other-libraries--approaches)
4. [Modern C++ Techniques](#4-modern-c-techniques)
5. [Synthesis & Design Implications](#5-synthesis--design-implications)

---

## 1. Boost.YAP: Modern Expression Templates

YAP (Yet Another Proto) is a C++14+ expression template library - the modern successor to Boost.Proto. It provides a cleaner, more intuitive approach to building DSELs.

### 1.1 Core Philosophy

YAP structures expression processing as a compiler pipeline:

```cpp
auto expr_0 = /* capture expression */;
auto expr_1 = boost::yap::transform(expr_0, optimize_pass_1);
auto expr_2 = boost::yap::transform(expr_1, optimize_pass_2);
auto result = boost::yap::evaluate(expr_n);
```

This mirrors: **parse → optimize → codegen**

**Key design decisions:**
- Expression semantics match C++ built-in expressions
- Values stored by decay (not reference) - mirrors C++ value semantics
- Transforms are simple callables with overloads
- No built-in grammar DSL (validate in transforms instead)

### 1.2 Expression Representation

#### The Minimal Expression Template

The simplest type that models `Expression`:

```cpp
template <boost::yap::expr_kind Kind, typename Tuple>
struct minimal_expr {
    static const boost::yap::expr_kind kind = Kind;
    Tuple elements;
};
```

Two requirements:
1. `static const expr_kind kind` - the operation type
2. `Tuple elements` - operands stored in `hana::tuple`

#### The `expr_kind` Enumeration

46 kinds covering all C++ operators:

```cpp
enum class expr_kind {
    expr_ref = 0,      // Reference to another expression
    terminal = 1,      // Leaf node (holds a value)

    // Unary (2-11): unary_plus, negate, dereference, complement,
    //               address_of, logical_not, pre_inc, pre_dec, post_inc, post_dec

    // Binary (12-42): shift_left, shift_right, multiplies, divides, modulus,
    //                 plus, minus, less, greater, less_equal, greater_equal,
    //                 equal_to, not_equal_to, logical_or, logical_and,
    //                 bitwise_and, bitwise_or, bitwise_xor, comma, mem_ptr,
    //                 assign, *_assign variants, subscript

    if_else = 44,      // ?: (ternary)
    call = 45          // ()
};
```

#### Tuple Arity by Kind

| `expr_kind` | `elements` Contents |
|-------------|---------------------|
| `terminal` | 1 element: the actual value |
| `expr_ref` | 1 element: reference to Expression |
| Unary ops | 1 element: child Expression |
| Binary ops | 2 elements: child Expressions |
| `if_else` | 3 elements: condition, then, else |
| `call` | 1+ elements: callable + arguments |

#### Full Type Example

The expression `std::sqrt(3.0) + 8.0f` produces:

```cpp
boost::yap::expression<
    boost::yap::expr_kind::plus,
    boost::hana::tuple<
        boost::yap::expression<
            boost::yap::expr_kind::call,
            boost::hana::tuple<
                boost::yap::expression<
                    boost::yap::expr_kind::terminal,
                    boost::hana::tuple<double (*)(double)>  // sqrt
                >,
                boost::yap::expression<
                    boost::yap::expr_kind::terminal,
                    boost::hana::tuple<double>              // 3.0
                >
            >
        >,
        boost::yap::expression<
            boost::yap::expr_kind::terminal,
            boost::hana::tuple<float>                       // 8.0f
        >
    >
>
```

#### Creating Expressions

```cpp
// Make terminals
auto left = boost::yap::make_terminal<minimal_expr>(1);
auto right = boost::yap::make_terminal<minimal_expr>(41);

// Make compound expression
auto expr = boost::yap::make_expression<
    minimal_expr,
    boost::yap::expr_kind::plus
>(left, right);

// Debug print
boost::yap::print(std::cout, expr);
// Output:
// expr<+>
//     term<int>[=1]
//     term<int>[=41]
```

### 1.3 The Expression Concept

For type `E` to model `Expression`:

```cpp
E::kind      // Must be boost::yap::expr_kind (compile-time constant)
e.elements   // Must be boost::hana::tuple<...>
E e{t};      // Constructible from tuple
```

The `ExpressionTemplate` concept: a class template that produces an Expression when instantiated with `(expr_kind, hana::tuple<...>)`.

### 1.4 Transform System

A **Transform** is a callable with overloads that match expressions. `transform(expr, xform)`:
1. If `xform` matches `expr` directly → return `xform(expr)`
2. Otherwise → recursively transform children, rebuild expression

#### Two Flavors of Overloads

**TagTransform** - tag as first parameter, unpacked operand values:

```cpp
struct my_transform {
    // Match any negation of something convertible to double
    auto operator()(boost::yap::negate_tag, double x) {
        return -x;
    }

    // Match any terminal
    template<typename T>
    auto operator()(boost::yap::expr_tag<boost::yap::expr_kind::terminal>, T&& val) {
        return std::forward<T>(val);
    }
};
```

**ExpressionTransform** - entire expression as parameter:

```cpp
struct my_transform {
    template <typename T, typename U>
    auto operator()(
        my_expr<boost::yap::expr_kind::plus, boost::hana::tuple<T, U>> const& expr
    ) {
        // Access full expression structure
        return boost::yap::transform(boost::yap::left(expr), *this);
    }
};
```

TagTransform checked first; ExpressionTransform only if TagTransform doesn't match.

#### Complete Transform Example: `get_arity`

```cpp
struct get_arity {
    // Placeholder terminals → return their index
    template <long long I>
    boost::hana::llong<I> operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
        boost::yap::placeholder<I>
    ) {
        return boost::hana::llong_c<I>;
    }

    // Other terminals → arity 0
    template <typename T>
    auto operator()(boost::yap::expr_tag<boost::yap::expr_kind::terminal>, T&&) {
        return boost::hana::llong_c<0>;
    }

    // Compound expressions → max of children
    template <boost::yap::expr_kind Kind, typename... Args>
    auto operator()(boost::yap::expr_tag<Kind>, Args&&... args) {
        return boost::hana::maximum(boost::hana::make_tuple(
            boost::yap::transform(
                boost::yap::as_expr(std::forward<Args>(args)),
                get_arity{}
            )...
        ));
    }
};

// Usage
auto expr = 1_p * 2_p;
auto arity = boost::yap::transform(expr, get_arity{});
static_assert(arity.value == 2);
```

#### Stateful Transform: `take_nth`

```cpp
struct take_nth {
    template <typename T>
    auto operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
        std::vector<T> const& vec
    ) {
        return boost::yap::make_terminal(vec[n]);
    }

    std::size_t n;  // State
};
```

### 1.5 Operator Macros

```cpp
// Binary operators
BOOST_YAP_USER_BINARY_OPERATOR(plus, my_expr, my_expr)
BOOST_YAP_USER_BINARY_OPERATOR(multiplies, my_expr, my_expr)

// Unary operators
BOOST_YAP_USER_UNARY_OPERATOR(negate, my_expr, my_expr)

// Special operators
BOOST_YAP_USER_ASSIGN_OPERATOR(my_expr, ::my_expr)
BOOST_YAP_USER_SUBSCRIPT_OPERATOR(::my_expr)
BOOST_YAP_USER_CALL_OPERATOR(::my_expr)

// Placeholder literals: 1_p, 2_p, etc.
BOOST_YAP_USER_LITERAL_PLACEHOLDER_OPERATOR(my_expr)
```

### 1.6 Evaluation

`evaluate()` strips YAP machinery and evaluates as normal C++:

```cpp
using namespace boost::yap::literals;

evaluate(1_p + 2.0, 3.0);              // 5.0  (3.0 + 2.0)
evaluate(1_p * 2_p, 3.0, 2.0);         // 6.0  (3.0 * 2.0)
evaluate((1_p - 2_p) / 2_p, 3.0, 2.0); // 0.5  ((3.0 - 2.0) / 2.0)
```

Related functions:
- `replacements(args...)` - transform that replaces placeholders
- `replace_placeholders(expr, args...)` - replace then return expression
- `evaluation(args...)` - transform for evaluation

### 1.7 Complete Example: Lazy Vector

```cpp
template <boost::yap::expr_kind Kind, typename Tuple>
struct lazy_vector_expr {
    static const boost::yap::expr_kind kind = Kind;
    Tuple elements;

    // Eager subscript: transform then evaluate
    auto operator[](std::size_t n) const {
        return boost::yap::evaluate(
            boost::yap::transform(*this, take_nth{n})
        );
    }
};

struct take_nth {
    boost::yap::terminal<lazy_vector_expr, double>
    operator()(
        boost::yap::terminal<lazy_vector_expr, std::vector<double>> const& expr
    ) {
        return boost::yap::make_terminal<lazy_vector_expr>(
            boost::yap::value(expr)[n]
        );
    }
    std::size_t n;
};

BOOST_YAP_USER_BINARY_OPERATOR(plus, lazy_vector_expr, lazy_vector_expr)
BOOST_YAP_USER_BINARY_OPERATOR(minus, lazy_vector_expr, lazy_vector_expr)

// Usage - no temporaries created!
lazy_vector v1{...}, v2{...}, v3{...};
double d = (v2 + v3)[2];  // Expression tree built, evaluated at index 2
```

### 1.8 YAP vs Proto

| Aspect | Proto (C++98) | YAP (C++14+) |
|--------|---------------|--------------|
| Expression definition | Inherit `proto::extends<>` | Struct with `kind` + `elements` |
| Transforms | Inherit `proto::transform<>`, `impl<>` | Callable with overloads |
| Transform params | `(Expr, State, Data)` triple | `(Expr)` or `(tag, args...)` |
| Value storage | References (lifetime issues) | Decayed values |
| Grammar validation | First-class `proto::matches<>` | Manual in transforms |
| Semantics | Custom contexts | Matches C++ expressions |

**YAP doesn't have:** Grammar DSL, `proto::matches<>`, domains, context-based eval

---

## 2. Mathematical & CS Foundations

### 2.1 Abstract Algebra

**Algebraic Structures and Valid Simplifications:**

| Structure | Properties | Enables |
|-----------|------------|---------|
| Semigroup | Associativity | `(a+b)+c = a+(b+c)` flattening |
| Monoid | + Identity | `a+0 = a` elimination |
| Group | + Inverses | `a+(-a) = 0` cancellation |
| Ring | Distributivity | `a*(b+c) = a*b+a*c` expansion |

**Free Algebra (Term Algebra):**

A signature Σ defines operations with arities:
```
Σ = { Const: ℕ→Expr, Var: String→Expr, Add: Expr×Expr→Expr, ... }
```

The free algebra T(Σ,X) is all well-formed terms with no equations - pure syntax. This is what an AST represents.

**Universal Property:** Any variable assignment extends uniquely to a homomorphism. Basis for:
- Interpretation (syntax → semantics)
- Evaluation (to numeric type)
- Compilation (to machine code)

**Equational Theories & Rewriting:**

Impose equations for simplification:
```
x + 0 → x       (identity)
x * 0 → 0       (annihilation)
--x   → x       (involution)
```

### 2.2 Type Theory

**Algebraic Data Types:**
- **Sum types**: `A | B` (variant)
- **Product types**: `A × B` (tuple)

ASTs are sums of products:
```haskell
data Expr = Const Int | Var String | Add Expr Expr | Mul Expr Expr
```

**GADTs** - constructors refine type parameter:
```haskell
data Expr a where
    LitInt  :: Int -> Expr Int
    LitBool :: Bool -> Expr Bool
    Add     :: Expr Int -> Expr Int -> Expr Int
```

The type `a` tracks result type. C++ approximation via concepts:
```cpp
template<typename L, typename R>
    requires std::same_as<typename L::ResultType, int>
          && std::same_as<typename R::ResultType, int>
struct Add : Expr<int, Add<L, R>> { ... };
```

**Higher-Kinded Types:** Types parameterized by type constructors. C++ via template-template parameters. Enables recursion schemes and annotation polymorphism.

### 2.3 Category Theory

**F-Algebras:** For functor F, an algebra is `(Carrier, F(Carrier) → Carrier)`:
```cpp
struct EvalAlgebra {
    using Carrier = int;
    int operator()(int n) { return n; }           // Const
    int operator()(int l, int r) { return l + r; } // Add
};
```

**Initial Algebra:** The least fixed point μF - the recursive AST type.

**Catamorphism (Fold):** `cata : (F(A) → A) → μF → A`
1. Unwrap one layer
2. Recursively fold children
3. Apply algebra

**Recursion Schemes:**

| Scheme | Algebra Gets | Use Case |
|--------|--------------|----------|
| Catamorphism | Child results | eval, print |
| Paramorphism | Original + result | differentiation |
| Anamorphism | Builds structure | code generation |
| Hylomorphism | unfold → fold | compile (no intermediate) |
| Zygomorphism | Two folds parallel | forward-mode AD |

### 2.4 Rewriting Systems

**Term Rewriting System (TRS):** Rules `l → r`

**Confluence:** If `t → u` and `t → v`, both reach common `w`. Guarantees deterministic normal forms.

**Termination:** No infinite sequences. Prove via lexicographic ordering, polynomial interpretation.

**Strategy:** Controls rule application order (innermost, outermost, domain-specific).

### 2.5 The Expression Problem

Extend data with new **variants** AND new **operations** without recompiling.

| Style | Add Variant | Add Operation |
|-------|-------------|---------------|
| FP (ADT + match) | Hard | Easy |
| OO (inheritance) | Easy | Hard |

**Solutions:**
- **Visitor**: Adding ops easy, variants hard
- **Object Algebras**: Both extensible
- **Finally Tagless**: Ops as type class methods
- **Concepts**: Templates over constraints

---

## 3. Other Libraries & Approaches

### 3.1 Boost.Proto (YAP's Predecessor)

Proto provides what YAP omits:
- **Grammar DSL**: `proto::terminal<>`, `proto::plus<>`, etc.
- **Compile-time validation**: `proto::matches<Expr, Grammar>`
- **Domains**: Automatic grammar checking on operator overloads

```cpp
struct CalcGrammar : proto::or_<
    proto::terminal<proto::convertible_to<double>>,
    proto::plus<CalcGrammar, CalcGrammar>,
    proto::multiplies<CalcGrammar, CalcGrammar>
> {};

static_assert(proto::matches<Expr, CalcGrammar>::value);
```

Trade-off: More powerful but heavier compile-time cost, C++98 design.

### 3.2 Other C++ Libraries

**Boost.Hana:**
- Types as values: `hana::type<T>{}`
- `[[no_unique_address]]` compressed tuple
- Used by YAP for `elements` storage

**SymEngine:**
- Runtime expression tree with `RCP<T>` (ref-counted)
- Visitor pattern for operations
- LLVM JIT support

**GiNaC:**
- Extends C++ rather than external DSL
- Originally for quantum field theory calculations

### 3.3 Other Languages

**Haskell Recursion Schemes:**
- `Fix f = Fix (f (Fix f))` separates recursion from structure
- `recursion-schemes` package for generic folds
- [okmij.org/ftp/tagless-final](https://okmij.org/ftp/tagless-final/)

**Julia Symbolics** - layered architecture:
1. Metatheory.jl: E-graph rewriting
2. SymbolicUtils.jl: Rule-rewriting core
3. Symbolics.jl: Full CAS
4. ModelingToolkit.jl: Domain DSLs

**Scala LMS:**
- `Rep[T]` marks staged (generated) code
- Type system distinguishes compile-time from run-time

### 3.4 Academic Approaches

**Finally Tagless:**
- DSL terms as type class methods, not data constructors
- No pattern matching; interpretation is function application
- Multiple interpreters = multiple instances

```haskell
class Symantics repr where
  lit :: Int -> repr Int
  add :: repr Int -> repr Int -> repr Int
```

**Object Algebras:**
- Interfaces define variants; implementations define operations
- Solves expression problem in OO languages

**Trees That Grow:**
- Type families parameterize extension points
- Single AST supports multiple phase instantiations
- [arxiv.org/abs/1610.04799](https://arxiv.org/abs/1610.04799)

---

## 4. Modern C++ Techniques

### C++20 Concepts

Replace SFINAE with named constraints:
```cpp
template<typename T>
concept Symbolic = requires {
    typename T::id_set;
    { T::name } -> std::convertible_to<std::string_view>;
};
```

### std::variant + std::visit

Runtime tagged union:
```cpp
using Expr = std::variant<Const, Var, Add, Mul>;

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };

std::visit(overload{
    [](Const c) { ... },
    [](Add a) { ... },
}, expr);
```

### C++26 Static Reflection

- `^^T` to reflect, `[:...:]` to splice
- `define_class` generates types at compile time
- Could auto-generate interpreter dispatch
- [P2996R4](https://isocpp.org/files/papers/P2996R4.html)

### constexpr Everything

C++26 enables full compile-time computation:
```cpp
constexpr auto simplified = simplify(parse("x + 0"));
static_assert(simplified == parse("x"));
```

---

## 5. Synthesis & Design Implications

### Key Insights from YAP

1. **Minimal expression concept**: Just `kind` + `elements` tuple
2. **Transform-centric design**: Callables with overloads, not inheritance
3. **Tag dispatch**: Clean pattern matching via `expr_tag<Kind>`
4. **Value semantics**: Store by value, not reference
5. **Compiler pipeline**: capture → transform → evaluate

### Mapping YAP Patterns to C++26

| YAP Pattern | C++26 Equivalent |
|-------------|------------------|
| `expr_kind` enum | `constexpr` tag types or concepts |
| `hana::tuple` | `std::tuple` with `[[no_unique_address]]` |
| TagTransform overloads | Concept-constrained overloads |
| `BOOST_YAP_USER_*` macros | Deduced `this` + concepts |
| Runtime `transform()` | `constexpr` recursive templates |

### What a C++26 Design Can Improve

| YAP Limitation | C++26 Solution |
|----------------|----------------|
| Runtime tuple storage | Type-level only (zero storage) |
| No grammar validation | Concepts as grammars |
| Runtime transform dispatch | `if constexpr` + concepts |
| No compile-time compute | Full `constexpr`/`consteval` |
| Macro-based operators | Deducing `this` patterns |

### Opportunities from Other Sources

| Technique | Source | Benefit |
|-----------|--------|---------|
| Grammar as concept | Proto | Compile-time DSL validation |
| E-graph rewriting | Julia | Better simplification |
| Phase type families | Trees That Grow | Phase-specific metadata |
| Zygomorphism | Haskell | Forward-mode AD in one pass |
| Static reflection | C++26 | Auto-generate boilerplate |

### Recommended Architecture

```
┌─────────────────────────────────────────────────────────┐
│  User DSL (operator overloads, literals)                │
├─────────────────────────────────────────────────────────┤
│  Expression Types (Symbol, Add, Mul, etc.)              │
│  - Minimal: tag + children types                        │
│  - Concept-constrained: Symbolic, GraphNode             │
├─────────────────────────────────────────────────────────┤
│  Transforms (Interpreters)                              │
│  - Eval, Differentiate, Simplify, Print                 │
│  - Pattern: fold/para based on needs                    │
├─────────────────────────────────────────────────────────┤
│  Evaluation (constexpr compute or codegen)              │
└─────────────────────────────────────────────────────────┘
```

### Recommended Reading

1. **YAP Manual**: [boost.org/doc/libs/1_81_0/doc/html/boost_yap](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_yap/manual.html)
2. **Tagless-Final**: [okmij.org/ftp/tagless-final](https://okmij.org/ftp/tagless-final/)
3. **Recursion Schemes**: [schoolofhaskell catamorphisms](https://www.schoolofhaskell.com/user/edwardk/recursion-schemes/catamorphisms)
4. **Trees That Grow**: [arxiv.org/abs/1610.04799](https://arxiv.org/abs/1610.04799)
5. **C++26 Reflection**: [P2996](https://isocpp.org/files/papers/P2996R4.html)

---

## References

### Boost Libraries
- [Boost.YAP Manual](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_yap/manual.html)
- [Boost.YAP GitHub](https://github.com/boostorg/yap)
- [Boost.Proto User's Guide](https://www.boost.org/doc/libs/1_84_0/doc/html/proto/users_guide.html)
- [Boost.Hana](https://www.boost.org/doc/libs/1_61_0/libs/hana/doc/html/index.html)

### Symbolic Math
- [SymEngine](https://github.com/symengine/symengine)
- [GiNaC](https://www.ginac.de/)
- [Julia Symbolics](https://juliasymbolics.org/)

### Academic
- [Finally Tagless (Okmij)](https://okmij.org/ftp/tagless-final/index.html)
- [Object Algebras](https://www.cs.utexas.edu/~wcook/Drafts/2012/ecoop2012.pdf)
- [Trees That Grow](https://arxiv.org/abs/1610.04799)

### C++ Standards
- [C++20 Concepts](https://en.cppreference.com/w/cpp/language/constraints)
- [C++26 Reflection P2996R4](https://isocpp.org/files/papers/P2996R4.html)
