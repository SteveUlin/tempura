# tempura Symbolics

tempura Symoblics is a compile time symbolic computation library for C++. This
library separates symbolic function definitions from the data objects. This
enables the user to work directly with abstract symbolic expressions then
bind data to the symbols at a later time.


```cpp
constexpr auto a = SYMBOL();
constexpr auto b = SYMBOL();
static_assert((a * a)(Substitution{a = 5}) == 25);
static_assert((a * b)(Substitution{a = 5, b = 2}) == 10);
```

## Implementation Details

This library is inspired by this great talk:
[Symbolic Calculus for High-performance Computing From Scratch Using C++23](https://www.youtube.com/watch?v=lPfA4SFojao)

This library relies on two main concepts: expression template and tag types.

### Expression Templates

Expression templates are templated C++ classes that represent a generic
computation without actually running the computation:

```cpp
template <typename LHS, typename RHS>
class Plus {
public:
    auto operator() { ... };
};

tempura Symbolics represents mathematical expressions as nested expression
templates:

```cpp
Plus<Symbol<0>, Multiplies<Symbol<1>, Symbol<0>>> expr = a + b * a;
```

### Tag Types

All of the expression in tempura Symbolics are "tag types", C++ struts with
zero data members. These tag types enable temprua Symbolics to pass symbols
around as if they were values:

```cpp
constexpr auto a = SYMBOL();
auto expr = sin(a);
```

