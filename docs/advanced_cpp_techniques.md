# Advanced C++ Techniques: Fringe, Controversial & Obscure

A collection of advanced C++ techniques that go beyond the typical "modern C++" patterns. Some are controversial, some are obscure, all are interesting.

---

## 1. Data-Oriented Design: Kill Your Objects

OOP organizes code around objects. DOD organizes code around *data transformations*. The insight: CPUs don't care about your abstractions, only about memory access patterns.

### Array of Structures vs Structure of Arrays

```cpp
// ❌ AoS: Object-oriented, cache-hostile
struct Particle {
  Vec3 position;    // 12 bytes
  Vec3 velocity;    // 12 bytes
  float mass;       // 4 bytes
  float lifetime;   // 4 bytes
  Color color;      // 16 bytes
  // ... more fields
};
std::vector<Particle> particles;  // 48+ bytes per particle

void updatePositions(float dt) {
  for (auto& p : particles) {
    p.position += p.velocity * dt;  // Loads 48 bytes, uses 24
  }
}

// ✅ SoA: Data-oriented, cache-friendly
struct Particles {
  std::vector<Vec3> positions;
  std::vector<Vec3> velocities;
  std::vector<float> masses;
  std::vector<float> lifetimes;
  std::vector<Color> colors;
};

void updatePositions(Particles& p, float dt) {
  for (size_t i = 0; i < p.positions.size(); ++i) {
    p.positions[i] += p.velocities[i] * dt;  // Loads only what's needed
  }
  // Bonus: trivially SIMD-able!
}
```

**The Numbers:** SoA can be 40-60% faster for selective field access. Pulling 3300 cache lines (AoS) vs 1250 (SoA) for the same operation.

**The Controversy:** DOD advocates say OOP is fundamentally broken for performance-critical code. OOP advocates say DOD sacrifices maintainability.

**References:**
- [Data-Oriented Design Book (Richard Fabian)](https://www.dataorienteddesign.com/dodbook/)
- [CppCon 2014: Mike Acton "Data-Oriented Design"](https://www.youtube.com/watch?v=rX0ItVEVjHc)
- [ECS FAQ](https://github.com/SanderMertens/ecs-faq)

---

## 2. Type Erasure: Runtime Polymorphism Without Inheritance

Sean Parent's technique: polymorphism through *concepts*, not class hierarchies. Any type that "walks like a duck" works—no base class required.

```cpp
// The external interface—accepts ANYTHING drawable
class Drawable {
  struct Concept {  // Internal interface
    virtual ~Concept() = default;
    virtual void draw_() const = 0;
    virtual std::unique_ptr<Concept> clone_() const = 0;
  };

  template <typename T>
  struct Model final : Concept {  // Wraps any T with draw()
    T data_;
    Model(T x) : data_{std::move(x)} {}
    void draw_() const override { draw(data_); }  // Free function!
    std::unique_ptr<Concept> clone_() const override {
      return std::make_unique<Model>(*this);
    }
  };

  std::unique_ptr<Concept> self_;

public:
  template <typename T>
  Drawable(T x) : self_{std::make_unique<Model<T>>(std::move(x))} {}

  Drawable(const Drawable& other) : self_{other.self_->clone_()} {}

  friend void draw(const Drawable& d) { d.self_->draw_(); }
};

// Now ANY type with a free `draw` function works:
struct Circle { float radius; };
void draw(const Circle& c) { /* ... */ }

struct Square { float side; };
void draw(const Square& s) { /* ... */ }

std::vector<Drawable> shapes;
shapes.push_back(Circle{5.0f});   // No inheritance!
shapes.push_back(Square{3.0f});   // Just needs draw()

for (auto& s : shapes) draw(s);
```

### Small Buffer Optimization

Avoid heap allocation for small types:

```cpp
template <typename T, size_t BufferSize = 64>
class Drawable {
  alignas(std::max_align_t) std::byte buffer_[BufferSize];
  Concept* self_;  // Points into buffer_ or heap

  template <typename U>
  void construct(U&& x) {
    if constexpr (sizeof(Model<std::decay_t<U>>) <= BufferSize) {
      self_ = new (buffer_) Model<std::decay_t<U>>{std::forward<U>(x)};
    } else {
      self_ = new Model<std::decay_t<U>>{std::forward<U>(x)};
    }
  }
  // ...
};
```

**References:**
- [Sean Parent: Inheritance Is the Base Class of Evil](https://sean-parent.stlab.cc/papers-and-presentations)
- [Better Polymorphic Ducks](https://mropert.github.io/2017/12/17/better_polymorphic_ducks/)

---

## 3. The Expression Problem: Extensibility in Both Dimensions

The fundamental tension: OOP makes adding new *types* easy but new *operations* hard. FP makes adding new *operations* easy but new *types* hard. Can we have both?

### Object Algebras (Tagless Final in Disguise)

```cpp
// The "algebra" defines operations as a factory interface
template <typename E>
struct ExprAlgebra {
  virtual E lit(int n) = 0;
  virtual E add(E a, E b) = 0;
};

// Interpretation 1: Evaluation
struct Eval {
  int value;
};

struct EvalAlgebra : ExprAlgebra<Eval> {
  Eval lit(int n) override { return {n}; }
  Eval add(Eval a, Eval b) override { return {a.value + b.value}; }
};

// Interpretation 2: Pretty printing
struct Pretty {
  std::string repr;
};

struct PrettyAlgebra : ExprAlgebra<Pretty> {
  Pretty lit(int n) override { return {std::to_string(n)}; }
  Pretty add(Pretty a, Pretty b) override {
    return {"(" + a.repr + " + " + b.repr + ")"};
  }
};

// Expression written ONCE, interpreted MANY ways
template <typename E>
E expr(ExprAlgebra<E>& alg) {
  return alg.add(alg.lit(1), alg.add(alg.lit(2), alg.lit(3)));
}

EvalAlgebra eval;
PrettyAlgebra pretty;
auto result = expr(eval);      // {6}
auto printed = expr(pretty);   // "(1 + (2 + 3))"

// EXTEND with multiplication—no modification to existing code!
template <typename E>
struct MulAlgebra : virtual ExprAlgebra<E> {
  virtual E mul(E a, E b) = 0;
};
```

**References:**
- [Extensibility for the Masses: Object Algebras](https://www.cs.utexas.edu/~wcook/Drafts/2012/ecoop2012.pdf)
- [From Object Algebras to Tagless Final](https://oleksandrmanzyuk.wordpress.com/2014/06/18/from-object-algebras-to-finally-tagless-interpreters/)

---

## 4. Coroutines: Symmetric Transfer & Stackless Magic

C++20 coroutines are *stackless*—they don't preserve the call stack on suspension. This enables million-coroutine concurrency but requires careful design.

### The Stack Overflow Problem (Pre-Symmetric Transfer)

```cpp
// ❌ Naive task implementation: stack grows on each resume
task<int> recursive() {
  co_return co_await recursive();  // Each await adds a stack frame!
}
// Stack overflow after ~10k iterations
```

### Symmetric Transfer: Tail-Call for Coroutines

```cpp
// ✅ With symmetric transfer: constant stack usage
struct task_promise {
  std::coroutine_handle<> continuation_;

  auto final_suspend() noexcept {
    struct Awaiter {
      std::coroutine_handle<> cont;
      bool await_ready() noexcept { return false; }

      // KEY: return the next coroutine instead of resuming it
      std::coroutine_handle<> await_suspend(
          std::coroutine_handle<>) noexcept {
        return cont ? cont : std::noop_coroutine();
      }

      void await_resume() noexcept {}
    };
    return Awaiter{continuation_};
  }
};
// Compiler generates: jmp to continuation (no call, no stack growth)
```

### Generator Pattern

```cpp
template <typename T>
struct Generator {
  struct promise_type {
    T current_value;

    auto yield_value(T value) {
      current_value = std::move(value);
      return std::suspend_always{};
    }
    // ...
  };

  struct iterator {
    std::coroutine_handle<promise_type> coro;
    // ...
  };

  iterator begin() { coro_.resume(); return {coro_}; }
  std::default_sentinel_t end() { return {}; }
};

Generator<int> iota(int start) {
  while (true) co_yield start++;
}

for (int x : iota(0) | std::views::take(10)) {
  std::cout << x << "\n";  // 0, 1, 2, ...
}
```

**References:**
- [Lewis Baker: Understanding Symmetric Transfer](https://lewissbaker.github.io/2020/05/11/understanding_symmetric_transfer)
- [cppcoro library](https://github.com/lewissbaker/cppcoro)

---

## 5. Lock-Free Programming: Dancing with the Memory Model

Lock-free data structures avoid mutexes but require understanding hardware memory models.

### Memory Orderings Cheat Sheet

```cpp
// Relaxed: no ordering guarantees (fastest)
counter.fetch_add(1, std::memory_order_relaxed);

// Acquire/Release: synchronizes between threads
// Producer:
data = 42;
ready.store(true, std::memory_order_release);  // Barrier: all before visible

// Consumer:
while (!ready.load(std::memory_order_acquire));  // Barrier: all after see data
assert(data == 42);  // Guaranteed!

// Seq_cst: total order across all threads (slowest, default)
x.store(1, std::memory_order_seq_cst);
```

### Hazard Pointers (C++26)

The ABA problem: pointer reused before you notice it changed.

```cpp
// Without protection: use-after-free possible
Node* head = list.load();
// ... another thread frees head and reuses memory ...
if (list.compare_exchange(head, head->next)) {  // ABA: head is "same" but wrong!

// ✅ Hazard pointers: "I'm using this, don't free it"
std::hazard_pointer hp = std::make_hazard_pointer();
Node* head;
do {
  head = hp.protect(list);  // Announce: "I'm reading this"
} while (head != list.load());
// Now safe to use head—won't be freed until hp releases

hp.reset();  // Done with it
```

### RCU (Read-Copy-Update)

Readers never block. Writers copy, modify, then atomically swap.

```cpp
// Read path: no synchronization!
rcu_read_lock();
Data* d = rcu_dereference(global_data);
use(d);
rcu_read_unlock();

// Write path: copy-modify-publish
Data* old = global_data;
Data* new_data = new Data(*old);
modify(new_data);
rcu_assign_pointer(global_data, new_data);
synchronize_rcu();  // Wait for all readers to finish
delete old;
```

**References:**
- [C++ Concurrency in Action (Anthony Williams)](https://www.manning.com/books/c-plus-plus-concurrency-in-action)
- [Hazard Pointers Paper](https://www.researchgate.net/publication/3300862_Hazard_pointers_Safe_memory_reclamation_for_lock-free_objects)

---

## 6. Undefined Behavior: Time Travel & Nasal Demons

UB isn't just "anything can happen"—compilers *actively exploit* it for optimization.

### Time Travel

```cpp
int table[4];
bool exists_in_table(int v) {
  for (int i = 0; i <= 4; i++) {  // Bug: should be < 4
    if (table[i] == v) return true;
  }
  return false;
}
```

The compiler reasons: "If `i == 4`, that's UB (out of bounds). UB never happens. Therefore `i` never equals 4. Therefore the loop never terminates without returning true. Therefore this function always returns true."

The optimizer can *remove the entire loop* and return `true` unconditionally.

**The C++ standard explicitly permits this:** "If any such execution contains an undefined operation, this Standard places no requirement on the implementation... (not even with regard to operations *preceding* the first undefined operation)."

### Sanitizers: Your Defense

```bash
# Compile with UBSan
clang++ -fsanitize=undefined -g mycode.cpp

# Runtime output:
# mycode.cpp:3:14: runtime error: index 4 out of bounds for type 'int [4]'
```

**References:**
- [Raymond Chen: UB Can Result in Time Travel](https://devblogs.microsoft.com/oldnewthing/20140627-00/?p=633)
- [What Every C Programmer Should Know About UB](https://blog.llvm.org/2011/05/what-every-c-programmer-should-know.html)

---

## 7. Arena Allocators: Allocation in O(1)

Forget `new`/`delete`. Bump a pointer, free everything at once.

```cpp
class Arena {
  char* buffer_;
  char* current_;
  char* end_;

public:
  Arena(size_t size)
      : buffer_{static_cast<char*>(std::malloc(size))},
        current_{buffer_},
        end_{buffer_ + size} {}

  ~Arena() { std::free(buffer_); }

  template <typename T, typename... Args>
  T* alloc(Args&&... args) {
    // Align
    size_t space = end_ - current_;
    void* ptr = current_;
    if (!std::align(alignof(T), sizeof(T), ptr, space)) {
      throw std::bad_alloc{};
    }
    current_ = static_cast<char*>(ptr) + sizeof(T);
    return new (ptr) T{std::forward<Args>(args)...};
  }

  void reset() { current_ = buffer_; }  // "Free" everything instantly!
};

// Usage: parse a huge JSON, then discard everything
Arena arena{1024 * 1024};  // 1MB
JsonNode* root = parse(json_string, arena);
process(root);
arena.reset();  // All nodes freed in O(1)!
```

### When to Use

- **Per-frame game allocations:** reset every frame
- **Request handling:** reset after each request
- **Compilers:** reset after each function

**References:**
- [Ryan Fleury: Untangling Lifetimes: The Arena Allocator](https://www.rfleury.com/p/untangling-lifetimes-the-arena-allocator)
- [std::pmr::monotonic_buffer_resource](https://en.cppreference.com/w/cpp/memory/monotonic_buffer_resource)

---

## 8. Intrusive Containers: No Allocations, No Copies

Standard containers store *copies* and allocate nodes. Intrusive containers link *existing* objects.

```cpp
// Object contains its own links
struct Task : boost::intrusive::list_base_hook<> {
  std::string name;
  void execute();
};

boost::intrusive::list<Task> pending;
boost::intrusive::list<Task> completed;

Task t1{"compile"}, t2{"link"}, t3{"run"};

pending.push_back(t1);  // No allocation! Just pointer manipulation
pending.push_back(t2);
pending.push_back(t3);

// Move between lists: O(1), no copying
auto it = pending.begin();
pending.erase(it);
completed.push_back(*it);
```

### Advantages

- **Zero allocation:** insertion is just pointer updates
- **O(1) list-to-iterator:** given object, find its position
- **No copies:** objects stay in place
- **Predictable performance:** no allocator surprises

### Disadvantages

- Objects must be modified to include hooks
- Lifetime management is your responsibility
- Can't store the same object in multiple containers (without multiple hooks)

**References:**
- [Boost.Intrusive](https://www.boost.org/doc/libs/release/doc/html/intrusive.html)
- [P0406: Intrusive Containers Proposal](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0406r1.html)

---

## 9. CTRE: Regex at Compile Time

Why parse regex at runtime when you can do it at compile time?

```cpp
#include <ctre.hpp>

// Pattern compiled to state machine at compile time
constexpr auto pattern = ctll::fixed_string{"^([a-z]+)@([a-z]+)\\.com$"};

bool is_email(std::string_view sv) {
  return ctre::match<pattern>(sv);
}

// With captures
auto parse_email(std::string_view sv) {
  if (auto m = ctre::match<"^([a-z]+)@([a-z]+)\\.com$">(sv)) {
    auto user = m.get<1>().to_view();
    auto domain = m.get<2>().to_view();
    return std::pair{user, domain};
  }
  return std::pair<std::string_view, std::string_view>{};
}
```

### The Magic

- Pattern syntax validated at compile time
- State machine generated at compile time
- Zero runtime regex compilation
- Often **40 lines of assembly** for a complete match
- Faster compilation than `std::regex`!

**References:**
- [CTRE Library](https://github.com/hanickadot/compile-time-regular-expressions)
- [Hana Dusíková: Compile Time Regular Expressions](https://compile-time.re/)

---

## 10. Deducing This (C++23): CRTP Without the C

The explicit object parameter lets you name `this` and deduce its type.

### Recursive Lambdas (Finally!)

```cpp
// ❌ Pre-C++23: need Y combinator or std::function
std::function<int(int)> fib = [&](int n) {
  return n < 2 ? n : fib(n-1) + fib(n-2);  // Heap allocation for std::function!
};

// ✅ C++23: self-referential lambda
auto fib = [](this auto&& self, int n) -> int {
  return n < 2 ? n : self(n-1) + self(n-2);
};
```

### CRTP Replacement

```cpp
// ❌ Classic CRTP: verbose, confusing
template <typename Derived>
struct Addable {
  auto operator+(const Derived& other) const {
    return static_cast<const Derived&>(*this).add(other);
  }
};

struct Vec2 : Addable<Vec2> { /* ... */ };

// ✅ C++23: just use deducing this
struct Addable {
  auto operator+(this auto&& self, const auto& other) {
    return self.add(other);
  }
};

struct Vec2 : Addable { /* ... */ };  // No template argument!
```

### Perfect Forwarding for Member Functions

```cpp
struct Widget {
  std::string data;

  // One function handles lvalue and rvalue
  auto getData(this auto&& self) -> decltype(auto) {
    return std::forward<decltype(self)>(self).data;
  }
};

Widget w;
auto& ref = w.getData();              // Returns std::string&
auto val = std::move(w).getData();    // Returns std::string&&
```

**References:**
- [C++23: Deducing This (Microsoft)](https://devblogs.microsoft.com/cppblog/cpp23-deducing-this/)
- [P0847: Deducing This](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0847r6.html)

---

## 11. Compile-Time Reflection (C++26)

Inspect and generate code at compile time. The metaprogramming revolution.

```cpp
// Get all member names of a struct
template <typename T>
consteval auto member_names() {
  std::vector<std::string_view> names;
  template for (constexpr auto member : std::meta::members_of(^T)) {
    if constexpr (std::meta::is_nonstatic_data_member(member)) {
      names.push_back(std::meta::name_of(member));
    }
  }
  return names;
}

struct Point { int x; int y; };
static_assert(member_names<Point>() == std::vector{"x", "y"});

// Auto-generate serialization
template <typename T>
std::string to_json(const T& obj) {
  std::string result = "{";
  bool first = true;
  template for (constexpr auto member : std::meta::nonstatic_data_members_of(^T)) {
    if (!first) result += ",";
    first = false;
    result += "\"" + std::string(std::meta::name_of(member)) + "\":";
    result += to_json(obj.[:member:]);  // Splice: access the member
  }
  return result + "}";
}
```

**References:**
- [P2996: Reflection for C++26](https://isocpp.org/files/papers/P2996R4.html)
- [Herb Sutter: Metaclasses](https://herbsutter.com/2017/07/26/metaclasses-thoughts-on-generative-c/)

---

## 12. The Zero-Cost Abstraction Myth

Chandler Carruth (2019): "There are no zero-cost abstractions."

### Virtual Dispatch: The Real Cost

```cpp
struct Base {
  virtual int compute(int x) = 0;
};

struct Derived : Base {
  int compute(int x) override { return x * 2; }
};

void process(Base* b, int n) {
  for (int i = 0; i < n; ++i) {
    sum += b->compute(i);  // Indirect call every iteration
  }
}
```

**Measured overhead:** 10-20% for tiny functions, <1% for expensive functions.

### When Devirtualization Fails

```cpp
// Compiler CAN devirtualize:
Derived d;
d.compute(5);  // Direct call—type is known

// Compiler CANNOT devirtualize:
Base* b = getFromSomewhere();
b->compute(5);  // Must be indirect—type unknown

// Sometimes it can:
std::unique_ptr<Base> b = std::make_unique<Derived>();
b->compute(5);  // Modern compilers often devirtualize this!
```

**Benchmark surprise:** Clang missed a devirtualization that GCC caught, making a benchmark **60% slower**.

**References:**
- [The True Price of Virtual Functions](https://johnnysswlab.com/the-true-price-of-virtual-functions-in-c/)
- [When Can the Compiler Devirtualize?](https://quuxplusone.github.io/blog/2021/02/15/devirtualization/)

---

## 13. Flyweight/String Interning: Memory Through Sharing

Store each unique value once. All "copies" are just pointers.

```cpp
class InternedString {
  static std::unordered_set<std::string>& pool() {
    static std::unordered_set<std::string> p;
    return p;
  }

  const std::string* ptr_;

public:
  InternedString(std::string_view sv) {
    auto [it, _] = pool().emplace(sv);
    ptr_ = &*it;
  }

  // Comparison is pointer comparison—O(1)!
  bool operator==(const InternedString& other) const {
    return ptr_ == other.ptr_;
  }

  const std::string& str() const { return *ptr_; }
};

// 1 million "hello" strings = 1 actual string + 1 million pointers
std::vector<InternedString> strings;
for (int i = 0; i < 1'000'000; ++i) {
  strings.emplace_back("hello");
}
// Memory: ~8MB (pointers) instead of ~6GB (strings)
```

**Use cases:**
- Compiler symbol tables
- JSON keys
- Game asset names
- Any scenario with many duplicate strings

**References:**
- [Boost.Flyweight](https://www.boost.org/doc/libs/release/libs/flyweight/doc/index.html)

---

## Summary: When to Use What

| Technique | Use When |
|-----------|----------|
| Data-Oriented Design | Processing large arrays, cache matters |
| Type Erasure | Runtime polymorphism without inheritance |
| Object Algebras | Need to extend both types and operations |
| Coroutines | Async I/O, generators, state machines |
| Lock-Free | High-contention multithreading |
| Arena Allocator | Bulk allocation with bulk deallocation |
| Intrusive Containers | Zero-allocation containers needed |
| CTRE | Performance-critical regex |
| Deducing This | CRTP replacement, recursive lambdas |
| Reflection | Code generation, serialization |
| Flyweight | Many duplicate objects |

---

## Further Reading

### Books
- [Data-Oriented Design](https://www.dataorienteddesign.com/dodbook/) - Richard Fabian
- [C++ Concurrency in Action](https://www.manning.com/books/c-plus-plus-concurrency-in-action) - Anthony Williams

### Talks
- [CppCon 2014: Mike Acton "Data-Oriented Design"](https://www.youtube.com/watch?v=rX0ItVEVjHc)
- [Sean Parent: Inheritance Is the Base Class of Evil](https://sean-parent.stlab.cc/)
- [Hana Dusíková: Compile Time Regular Expressions](https://www.youtube.com/watch?v=QM3W36COnE4)

### Libraries
- [Boost.Intrusive](https://www.boost.org/doc/libs/release/doc/html/intrusive.html)
- [CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
- [cppcoro](https://github.com/lewissbaker/cppcoro)
- [libunifex](https://github.com/facebookexperimental/libunifex)
