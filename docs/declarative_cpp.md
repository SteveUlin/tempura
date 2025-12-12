# Declarative C++: Fringe & Controversial Techniques

Advanced type-level programming techniques that push C++ toward being a dependently-typed, effect-tracked, protocol-verified language. These ideas are experimental, sometimes controversial, and often require significant template machinery.

> "The type system is a theorem prover. Use it."

---

## 1. Parse, Don't Validate

The idea: never represent invalid states. Don't validate data and set a boolean flag—*parse* raw data into types that can only hold valid values.

```cpp
// ❌ Validation: invalid states ARE representable
struct Email {
  std::string value;
  bool is_valid = false;  // Can forget to check!
};

Email parse(std::string s) {
  Email e{s};
  e.is_valid = isValidEmail(s);
  return e;  // Caller can ignore is_valid
}

// ✅ Parsing: invalid states are UNREPRESENTABLE
class Email {
  std::string value_;
  explicit Email(std::string s) : value_{std::move(s)} {}  // Private!
public:
  // The ONLY way to create an Email is through parsing
  static auto parse(std::string s) -> std::optional<Email> {
    if (!isValidEmail(s)) return std::nullopt;
    return Email{std::move(s)};
  }
  auto get() const -> const std::string& { return value_; }
};

// If you have an Email, it IS valid. No checking needed downstream.
void sendTo(Email e);  // Cannot receive invalid email
```

### Non-Empty Collections

```cpp
// A list that is GUARANTEED non-empty by construction
template <typename T>
class NonEmpty {
  T head_;
  std::vector<T> tail_;

  NonEmpty(T h, std::vector<T> t) : head_{std::move(h)}, tail_{std::move(t)} {}
public:
  static auto fromVector(std::vector<T> v) -> std::optional<NonEmpty<T>> {
    if (v.empty()) return std::nullopt;
    T head = std::move(v.front());
    v.erase(v.begin());
    return NonEmpty{std::move(head), std::move(v)};
  }

  // These are TOTAL functions—no exceptions, no UB
  auto first() const -> const T& { return head_; }
  auto size() const -> std::size_t { return 1 + tail_.size(); }
};

// Cannot fail—input type proves non-emptiness
template <typename T>
auto safeHead(const NonEmpty<T>& xs) -> const T& {
  return xs.first();  // No bounds check needed!
}
```

**References:**
- [Parse, Don't Validate (Alexis King)](https://lexi-lambda.github.io/blog/2019/11/05/parse-don-t-validate/)
- [Make Illegal States Unrepresentable](https://functional-architecture.org/make_illegal_states_unrepresentable/)

---

## 2. Free Monads: Programs as Data

A free monad turns your DSL into a data structure that can be interpreted in multiple ways—run it, simulate it, log it, optimize it.

```cpp
// The "language" of console operations
template <typename Next>
struct Print { std::string msg; Next next; };

template <typename Next>
struct Read { std::function<Next(std::string)> cont; };

// The free monad: a program is either done, or an instruction + continuation
template <typename A>
struct Console;

template <typename A>
using ConsolePtr = std::shared_ptr<Console<A>>;

template <typename A>
struct Pure { A value; };

template <typename A>
struct Console : std::variant<
    Pure<A>,
    Print<ConsolePtr<A>>,
    Read<std::function<ConsolePtr<A>(std::string)>>
> {
  using std::variant<Pure<A>, Print<ConsolePtr<A>>,
                     Read<std::function<ConsolePtr<A>(std::string)>>>::variant;
};

// Smart constructors build the program
template <typename A>
auto pure(A a) -> ConsolePtr<A> {
  return std::make_shared<Console<A>>(Pure<A>{std::move(a)});
}

auto print(std::string msg) -> ConsolePtr<std::monostate> {
  return std::make_shared<Console<std::monostate>>(
      Print<ConsolePtr<std::monostate>>{msg, pure(std::monostate{})});
}

// The program is DATA—we haven't executed anything yet
auto myProgram() -> ConsolePtr<std::string> {
  // print("What's your name?") >> read() >>= \name -> print("Hello " + name)
  // ... (requires monadic bind implementation)
}

// Interpreter 1: Actually run it
void runIO(ConsolePtr<std::monostate> prog);

// Interpreter 2: Pure test—no I/O!
auto runPure(ConsolePtr<std::monostate> prog,
             std::vector<std::string> inputs) -> std::vector<std::string>;

// Same program, different interpretations!
```

**The Controversy:** C++ lacks syntactic sugar for monadic composition, making this verbose. Some argue it's not worth it without do-notation or for-comprehensions.

**References:**
- [Free Monads in C++](https://toby-allsopp.github.io/2016/10/12/free-monads-in-cpp.html)
- [Free Monad vs Tagless Final](https://softwaremill.com/free-tagless-compared-how-not-to-commit-to-monad-too-early/)

---

## 3. Tagless Final: Extensible Interpreters

Instead of building an AST, define operations as an *interface* (concept). Different interpreters implement the interface differently.

```cpp
// The "finally tagless" approach: operations as a concept
template <typename Repr>
concept ExprDSL = requires(Repr r, int n, Repr a, Repr b) {
  { Repr::lit(n) } -> std::same_as<Repr>;
  { Repr::add(a, b) } -> std::same_as<Repr>;
  { Repr::mul(a, b) } -> std::same_as<Repr>;
};

// Interpreter 1: Evaluation
struct Eval {
  int value;
  static auto lit(int n) -> Eval { return {n}; }
  static auto add(Eval a, Eval b) -> Eval { return {a.value + b.value}; }
  static auto mul(Eval a, Eval b) -> Eval { return {a.value * b.value}; }
};

// Interpreter 2: Pretty printing
struct Pretty {
  std::string repr;
  static auto lit(int n) -> Pretty { return {std::to_string(n)}; }
  static auto add(Pretty a, Pretty b) -> Pretty {
    return {"(" + a.repr + " + " + b.repr + ")"};
  }
  static auto mul(Pretty a, Pretty b) -> Pretty {
    return {"(" + a.repr + " * " + b.repr + ")"};
  }
};

// Interpreter 3: Optimization (constant folding)
struct Optimize {
  std::variant<int, std::string> value;  // Literal or expression
  // ... partial evaluation logic
};

// A single expression, multiple interpretations
template <ExprDSL E>
auto expr() -> E {
  return E::mul(E::add(E::lit(1), E::lit(2)), E::lit(3));
}

auto evaluated = expr<Eval>().value;      // 9
auto printed = expr<Pretty>().repr;       // "((1 + 2) * 3)"
```

**Why Controversial:** Critics say this over-abstracts simple problems. Fans say it's the purest separation of syntax and semantics.

**References:**
- [Tagless Final Style (Oleg Kiselyov)](https://okmij.org/ftp/tagless-final/index.html)

---

## 4. Continuation Passing Style (CPS)

Transform all control flow into explicit continuations. Functions never return—they call their continuation.

```cpp
// ❌ Direct style: control flow is implicit
int factorial(int n) {
  if (n <= 1) return 1;
  return n * factorial(n - 1);
}

// ✅ CPS: control flow is explicit data
template <typename Cont>
void factorial_cps(int n, Cont&& k) {
  if (n <= 1) {
    k(1);  // "Return" by calling continuation
  } else {
    factorial_cps(n - 1, [n, k = std::forward<Cont>(k)](int result) {
      k(n * result);
    });
  }
}

// Usage: pass a continuation that receives the result
factorial_cps(5, [](int result) {
  std::cout << result << "\n";  // 120
});
```

### CPS Enables:
- **Custom control flow:** exceptions, coroutines, backtracking
- **Tail call optimization:** every call is a tail call
- **Delimited continuations:** capture "the rest of the computation"

```cpp
// Amb: non-deterministic choice via continuations
template <typename T, typename Cont>
void amb(std::vector<T> choices, Cont&& k) {
  for (auto& choice : choices) {
    k(choice);  // Try each choice—backtracking for free!
  }
}

// Find all Pythagorean triples
amb({1,2,3,4,5}, [](int a) {
  amb({1,2,3,4,5}, [a](int b) {
    amb({1,2,3,4,5}, [a,b](int c) {
      if (a*a + b*b == c*c)
        std::cout << a << "," << b << "," << c << "\n";
    });
  });
});
```

**References:**
- [Continuation Passing Style (Functional C++)](https://functionalcpp.wordpress.com/2013/11/19/continuation-passing-style/)
- [C++ Synchronous CPS (ACCU)](https://accu.org/journals/overload/24/135/weatherhead_2296/)

---

## 5. Type-Level Computation with Peano Numbers

Encode natural numbers as types. Arithmetic happens at compile time.

```cpp
// Peano axioms as types
struct Zero {};

template <typename N>
struct Succ {};

// Type aliases for readability
using One = Succ<Zero>;
using Two = Succ<One>;
using Three = Succ<Two>;

// Addition: compile-time recursion
template <typename A, typename B>
struct Add;

template <typename B>
struct Add<Zero, B> {
  using Type = B;
};

template <typename A, typename B>
struct Add<Succ<A>, B> {
  using Type = Succ<typename Add<A, B>::Type>;
};

// 2 + 3 = 5, computed at compile time
using Five = Add<Two, Three>::Type;
static_assert(std::is_same_v<Five, Succ<Succ<Succ<Succ<Succ<Zero>>>>>>);

// Convert to runtime value
template <typename N>
struct ToInt;

template <>
struct ToInt<Zero> { static constexpr int value = 0; };

template <typename N>
struct ToInt<Succ<N>> { static constexpr int value = 1 + ToInt<N>::value; };

static_assert(ToInt<Five>::value == 5);
```

### Sized Vectors (Dependent Types Lite)

```cpp
template <typename T, typename N>
class Vec;

template <typename T>
class Vec<T, Zero> {
public:
  // No head() or tail()—empty vector has no elements!
};

template <typename T, typename N>
class Vec<T, Succ<N>> {
  T head_;
  Vec<T, N> tail_;
public:
  auto head() const -> const T& { return head_; }  // Always safe!
  auto tail() const -> const Vec<T, N>& { return tail_; }
};

// Concatenation knows the result size at compile time
template <typename T, typename N, typename M>
auto concat(Vec<T, N> a, Vec<T, M> b) -> Vec<T, typename Add<N, M>::Type>;

// Type error: can't take head of empty vector
Vec<int, Zero> empty;
// empty.head();  // Compile error! Method doesn't exist.
```

**References:**
- [Type Arithmetic (Okmij)](https://okmij.org/ftp/Computation/type-arithmetics.html)
- [Peano Numbers in C++](https://colliot.org/en/2017/09/【c-模板元编程入门】在编译期实现-peano-数/)

---

## 6. Row Polymorphism: Structural Typing for Records

Accept any record that has *at least* the required fields. Like duck typing, but type-safe.

```cpp
// Using C++20 with compile-time strings (simplified from Mitama library)
template <auto Name, typename T>
struct Field {
  T value;
};

template <typename... Fields>
struct Record : Fields... {
  using Fields::operator[]...;
};

// Create records with named fields
auto person = Record{
    Field<"name", std::string>{"Alice"},
    Field<"age", int>{30},
    Field<"email", std::string>{"alice@example.com"}
};

// Function accepts ANY record with "name" and "age" fields
template <typename R>
  requires requires(R r) {
    { r.template get<"name">() } -> std::convertible_to<std::string>;
    { r.template get<"age">() } -> std::convertible_to<int>;
  }
void greet(const R& r) {
  std::cout << "Hello " << r.template get<"name">()
            << ", age " << r.template get<"age">() << "\n";
}

// Works with any record that has those fields—extra fields ignored
greet(person);  // OK: has name, age (email is ignored)

auto minimal = Record{
    Field<"name", std::string>{"Bob"},
    Field<"age", int>{25}
};
greet(minimal);  // OK: exactly the required fields
```

**Why Controversial:** Some say this fights against C++'s nominal typing. Others love the flexibility.

**References:**
- [Row Polymorphism in C++20 (CADDi Tech)](https://caddi.tech/archives/2846)
- [Mitama Library](https://github.com/LoliGothick/mitama-cpp-result)

---

## 7. Linear Types: Use Exactly Once

Affine types (like `unique_ptr`) let you use at most once. *Linear* types require exactly once—you can't forget to use them.

```cpp
// A handle that MUST be closed—compile error if dropped
template <typename T>
class [[nodiscard]] MustUse {
  T value_;
  bool consumed_ = false;

public:
  explicit MustUse(T v) : value_{std::move(v)} {}

  // Destructor checks—but this is runtime, not compile-time :(
  ~MustUse() {
    if (!consumed_) std::terminate();  // You forgot!
  }

  // The only way to get the value marks it consumed
  auto consume() && -> T {
    consumed_ = true;
    return std::move(value_);
  }

  // No copying
  MustUse(const MustUse&) = delete;
  MustUse& operator=(const MustUse&) = delete;
};

// Better: typestate makes forgetting a COMPILE error
struct Open {};
struct Closed {};

template <typename State>
class File;

template <>
class File<Open> {
  int fd_;
public:
  auto close() && -> File<Closed>;  // Consumes self, returns closed
  auto write(std::span<const char>) && -> File<Open>;  // Must chain!
};

template <>
class File<Closed> {
  // No operations—can only be destroyed
};

// Usage: must close, or code won't compile (if using rvalue methods)
auto f = File<Open>::open("data.txt")
    .write("hello")
    .close();  // Returns File<Closed>
// f is now File<Closed>—write() unavailable
```

**The Problem:** C++ can't truly enforce linear types without language changes. `[[nodiscard]]` and runtime checks are workarounds.

**References:**
- [Linear Types (Substructural Type Systems)](https://en.wikipedia.org/wiki/Substructural_type_system)
- [C++ Move Semantics Considered Harmful](https://www.thecodedmessage.com/posts/cpp-move/)

---

## 8. Higher-Kinded Types via Defunctionalization

C++ has template templates, but not true HKTs. Defunctionalization encodes type-level functions as data.

```cpp
// "Defunctionalized" type constructors
struct ListTag {};
struct OptionalTag {};
struct VectorTag {};

// Apply: given a tag and a type, produce the actual type
template <typename Tag, typename T>
struct Apply;

template <typename T>
struct Apply<ListTag, T> { using Type = std::list<T>; };

template <typename T>
struct Apply<OptionalTag, T> { using Type = std::optional<T>; };

template <typename T>
struct Apply<VectorTag, T> { using Type = std::vector<T>; };

// Now we can write generic code over "any container"
template <typename F>  // F is a "type function" (tag)
struct Functor {
  template <typename A, typename Fn>
  static auto fmap(Fn&& f, typename Apply<F, A>::Type container)
      -> typename Apply<F, std::invoke_result_t<Fn, A>>::Type;
};

// Specialize for each tag
template <>
struct Functor<OptionalTag> {
  template <typename A, typename Fn>
  static auto fmap(Fn&& f, std::optional<A> opt) {
    if (opt) return std::optional{f(*opt)};
    return std::optional<std::invoke_result_t<Fn, A>>{};
  }
};

// Generic over the container type!
auto result = Functor<OptionalTag>::fmap(
    [](int x) { return x * 2; },
    std::optional{21}
);  // optional<int>{42}
```

**References:**
- [Lightweight Higher-Kinded Polymorphism (Yallop & White)](https://www.cl.cam.ac.uk/~jdy22/papers/lightweight-higher-kinded-polymorphism.pdf)
- [Functor, Maybe, and HKT in C++](https://gist.github.com/amrali/716d4c342a8f7fc3f23fee8c2b82e300)

---

## 9. Session Types: Typed Communication Protocols

Encode communication protocols in the type system. The type *is* the protocol.

```cpp
// Protocol: Client sends int, receives string, done
struct SendInt {};
struct RecvString {};
struct Done {};

// Session type: a sequence of operations
template <typename... Ops>
struct Protocol {};

using MyProtocol = Protocol<SendInt, RecvString, Done>;

// Channel that enforces the protocol at compile time
template <typename Proto>
class Channel;

template <typename Op, typename... Rest>
class Channel<Protocol<Op, Rest...>> {
public:
  // Only the operation matching Op is available

  auto send(int x) && -> Channel<Protocol<Rest...>>
    requires std::same_as<Op, SendInt>
  {
    // ... actual send
    return Channel<Protocol<Rest...>>{};
  }

  auto recv() && -> std::pair<std::string, Channel<Protocol<Rest...>>>
    requires std::same_as<Op, RecvString>
  {
    // ... actual receive
    return {"response", Channel<Protocol<Rest...>>{}};
  }
};

template <>
class Channel<Protocol<Done>> {
  // No operations—protocol complete
};

// Usage: type system enforces send-before-receive
auto chan = Channel<MyProtocol>{};
auto chan2 = std::move(chan).send(42);        // OK
auto [msg, chan3] = std::move(chan2).recv();  // OK
// chan3 is Channel<Protocol<Done>>—nothing more allowed!

// ❌ Compile error: wrong order
// auto bad = std::move(chan).recv();  // recv not available in SendInt state
```

**References:**
- [Session Types (Stanford CS242)](https://stanford-cs242.github.io/f18/lectures/07-2-session-types.html)
- [ProtEnc Library](https://www.fluentcpp.com/2019/09/24/expressive-code-for-state-machines-in-cpp/)

---

## 10. Algebraic Effects (Simulated)

Effects as first-class values that can be handled (interpreted) differently.

```cpp
// Effect: "I need to log something"
template <typename Next>
struct Log {
  std::string message;
  Next continuation;
};

// Effect: "I need a random number"
template <typename Next>
struct Random {
  std::function<Next(int)> continuation;
};

// A program is a tree of effects
template <typename A>
using Program = std::variant<
    Pure<A>,
    Log<std::shared_ptr<Program<A>>>,
    Random<std::function<std::shared_ptr<Program<A>>(int)>>
>;

// Handler 1: Actually perform effects
template <typename A>
A runReal(std::shared_ptr<Program<A>> prog) {
  return std::visit(overloaded{
    [](Pure<A>& p) { return p.value; },
    [](Log<std::shared_ptr<Program<A>>>& l) {
      std::cout << l.message << "\n";
      return runReal(l.continuation);
    },
    [](Random<std::function<std::shared_ptr<Program<A>>(int)>>& r) {
      return runReal(r.continuation(std::rand()));
    }
  }, *prog);
}

// Handler 2: Pure test—collect logs, use deterministic "random"
template <typename A>
std::pair<A, std::vector<std::string>> runPure(
    std::shared_ptr<Program<A>> prog,
    std::vector<int> randoms
) {
  std::vector<std::string> logs;
  // ... interpret Log by appending to logs, Random by popping from randoms
}

// Same program, different effect handlers!
```

**References:**
- [Algebraic Effects and Handlers](https://www.eff-lang.org/)
- [Effect Handlers for the Masses](https://dl.acm.org/doi/10.1145/3276481)

---

## Summary: The Spectrum of Declarativeness

| Technique | Compile-Time Safety | Ergonomics | Controversy Level |
|-----------|---------------------|------------|-------------------|
| Parse Don't Validate | ✓✓✓ | ✓✓ | Low |
| Typestate | ✓✓✓ | ✓ | Medium |
| Free Monads | ✓✓ | ✗ | High |
| Tagless Final | ✓✓ | ✓ | Medium |
| CPS | ✓ | ✗✗ | High |
| Peano Numbers | ✓✓✓ | ✗✗ | High |
| Row Polymorphism | ✓✓ | ✓ | Medium |
| Linear Types | ✓ (partial) | ✓ | High |
| HKT Emulation | ✓✓ | ✗ | High |
| Session Types | ✓✓✓ | ✓ | Medium |
| Algebraic Effects | ✓✓ | ✗ | High |

---

## Further Reading

### Papers
- [Lightweight Higher-Kinded Polymorphism](https://www.cl.cam.ac.uk/~jdy22/papers/lightweight-higher-kinded-polymorphism.pdf)
- [Typed Tagless Final Interpreters](https://okmij.org/ftp/tagless-final/course/lecture.pdf)
- [Session Types for Object-Oriented Languages](https://www.dcs.gla.ac.uk/~ornela/Sessions&Types/session-ecoop.pdf)

### Blog Posts
- [Parse, Don't Validate](https://lexi-lambda.github.io/blog/2019/11/05/parse-don-t-validate/)
- [Free Monads in C++](https://toby-allsopp.github.io/2016/10/12/free-monads-in-cpp.html)
- [Row Polymorphism in C++20](https://caddi.tech/archives/2846)

### Libraries
- [Mitama (Row Polymorphism)](https://github.com/LoliGothick/mitama-cpp-result)
- [cpp-effects (Algebraic Effects)](https://github.com/maciejpirog/cpp-effects)
- [refl-cpp (Compile-time Reflection)](https://github.com/veselink1/refl-cpp)
