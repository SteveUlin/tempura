#include "dual.h"

#include <cmath>
#include <concepts>
#include <string>
#include <type_traits>
#include <utility>

#include "unit.h"

using namespace tempura;
using namespace tempura::autodiff;

// constexpr: derivative of x² at x=3 is 2x=6 (exact polynomial — proves the constexpr path).
constexpr auto derivXSquared() -> double {
  Dual<double> x{3.0, 1.0};
  return (x * x).gradient;
}
static_assert(derivXSquared() == 6.0);

// constexpr transcendental — also proves std::sin/cos are constexpr under GCC trunk.
constexpr auto sinDerivConstexpr() -> bool {
  Dual<double> x{0.5, 1.0};
  return isClose(sin(x).gradient, std::cos(0.5), {.rtol = 1e-12, .atol = 1e-12});
}
static_assert(sinDerivConstexpr());

// Allocation contract: a gradient test-double counting every copy, move, and
// make. Counters live in a pointer-threaded struct (statics can't mutate at compile time).
struct Counts {
  int copies = 0;
  int moves = 0;
  int makes = 0;  // fresh G born from an arithmetic op
};
struct CountingG {
  Counts* c = nullptr;
  constexpr CountingG() = default;
  constexpr explicit CountingG(Counts* p) : c{p} {}
  constexpr CountingG(const CountingG& o) : c{o.c} {
    if (c) ++c->copies;
  }
  constexpr CountingG(CountingG&& o) noexcept : c{o.c} {
    if (c) ++c->moves;
  }
  constexpr auto operator=(const CountingG& o) -> CountingG& {
    c = o.c;
    if (c) ++c->copies;
    return *this;
  }
  constexpr auto operator=(CountingG&& o) noexcept -> CountingG& {
    c = o.c;
    if (c) ++c->moves;
    return *this;
  }
  constexpr auto operator+=(const CountingG&) -> CountingG& { return *this; }
  constexpr auto operator-=(const CountingG&) -> CountingG& { return *this; }
  constexpr auto operator*=(double) -> CountingG& { return *this; }
  constexpr auto operator/=(double) -> CountingG& { return *this; }
  friend constexpr auto operator+(const CountingG& a, const CountingG&) -> CountingG {
    if (a.c) ++a.c->makes;
    return CountingG{a.c};
  }
  friend constexpr auto operator-(const CountingG& a, const CountingG&) -> CountingG {
    if (a.c) ++a.c->makes;
    return CountingG{a.c};
  }
  friend constexpr auto operator*(const CountingG& a, double) -> CountingG {
    if (a.c) ++a.c->makes;
    return CountingG{a.c};
  }
  friend constexpr auto operator*(double, const CountingG& a) -> CountingG {
    if (a.c) ++a.c->makes;
    return CountingG{a.c};
  }
  friend constexpr auto operator/(const CountingG& a, double) -> CountingG {
    if (a.c) ++a.c->makes;
    return CountingG{a.c};
  }
  friend constexpr auto operator-(const CountingG& a) -> CountingG {
    if (a.c) ++a.c->makes;
    return CountingG{a.c};
  }
};
using CD = Dual<double, CountingG>;

constexpr auto countAddLL() -> Counts {
  Counts c;
  CD x{1.0, CountingG{&c}}, y{2.0, CountingG{&c}};
  auto r = x + y;
  (void)r;
  return c;
}
constexpr auto countAddChain() -> Counts {
  Counts c;
  CD x{1.0, CountingG{&c}}, y{2.0, CountingG{&c}};
  auto r = (x + y) + x;
  (void)r;
  return c;
}
constexpr auto countAddRvalRhs() -> Counts {
  Counts c;
  CD x{1.0, CountingG{&c}}, y{2.0, CountingG{&c}}, z{3.0, CountingG{&c}};
  auto r = x + (y * z);
  (void)r;
  return c;
}
constexpr auto countMul() -> Counts {
  Counts c;
  CD x{1.0, CountingG{&c}}, y{2.0, CountingG{&c}};
  auto r = x * y;
  (void)r;
  return c;
}
constexpr auto countDiv() -> Counts {
  Counts c;
  CD x{1.0, CountingG{&c}}, y{2.0, CountingG{&c}};
  auto r = x / y;
  (void)r;
  return c;
}
constexpr auto countAddScalar() -> Counts {
  Counts c;
  CD x{4.0, CountingG{&c}};
  auto r = x + 2.0;
  (void)r;
  return c;
}
constexpr auto countScalarMul() -> Counts {
  Counts c;
  CD x{4.0, CountingG{&c}};
  auto r = 2.0 * x;
  (void)r;
  return c;
}
constexpr auto countScalarSubTemp() -> Counts {
  Counts c;
  auto r = 5.0 - CD{2.0, CountingG{&c}};
  (void)r;
  return c;
}
constexpr auto countScalarDivTemp() -> Counts {
  Counts c;
  auto r = 5.0 / CD{2.0, CountingG{&c}};
  (void)r;
  return c;
}
constexpr auto countSubRvalRhs() -> Counts {  // x − temp: temp donates its buffer
  Counts c;
  CD x{1.0, CountingG{&c}};
  auto r = x - CD{2.0, CountingG{&c}};
  (void)r;
  return c;
}
constexpr auto countDivRvalRhs() -> Counts {  // x / temp: one make for the hoisted a′/w
  Counts c;
  CD x{1.0, CountingG{&c}};
  auto r = x / CD{2.0, CountingG{&c}};
  (void)r;
  return c;
}
constexpr auto countSubTempLhs() -> Counts {  // temp − x: by-value-left path
  Counts c;
  CD x{1.0, CountingG{&c}};
  auto r = CD{2.0, CountingG{&c}} - x;
  (void)r;
  return c;
}
constexpr auto countDivTempLhs() -> Counts {  // temp / x: by-value-left path
  Counts c;
  CD x{1.0, CountingG{&c}};
  auto r = CD{2.0, CountingG{&c}} / x;
  (void)r;
  return c;
}

// dual⊕dual: one semantic copy for the by-value lhs, one move out (id-expression
// return). x+(y*z) reuses the rhs temp — the outer op adds zero copies. The
// reversed sinks (5−t, 5/t) consume an rvalue dual with zero copies.
static_assert(countAddLL().copies == 1 && countAddLL().moves == 1 && countAddLL().makes == 0);
static_assert(countAddChain().copies == 1 && countAddChain().moves == 2 && countAddChain().makes == 0);
static_assert(countAddRvalRhs().copies == 1 && countAddRvalRhs().moves == 2 && countAddRvalRhs().makes == 1);
static_assert(countMul().copies == 1 && countMul().moves == 1 && countMul().makes == 1);
static_assert(countDiv().copies == 1 && countDiv().moves == 1 && countDiv().makes == 1);
static_assert(countAddScalar().copies == 1 && countAddScalar().moves == 1 && countAddScalar().makes == 0);
static_assert(countScalarMul().copies == 1 && countScalarMul().moves == 1 && countScalarMul().makes == 0);
static_assert(countScalarSubTemp().copies == 0 && countScalarSubTemp().moves == 1);
static_assert(countScalarDivTemp().copies == 0 && countScalarDivTemp().moves == 1);
// Uniform rvalue-donates contract for − and ÷: x−temp recycles with zero
// copies/makes; x/temp adds one make for the hoisted a′/w.
static_assert(countSubRvalRhs().copies == 0 && countSubRvalRhs().moves == 1 && countSubRvalRhs().makes == 0);
static_assert(countDivRvalRhs().copies == 0 && countDivRvalRhs().moves == 1 && countDivRvalRhs().makes == 1);
static_assert(countSubTempLhs().copies == 0 && countSubTempLhs().moves == 1 && countSubTempLhs().makes == 0);
static_assert(countDivTempLhs().copies == 0 && countDivTempLhs().moves == 1 && countDivTempLhs().makes == 1);

// Every {lvalue,rvalue}² arrangement of the four ops resolves without ambiguity —
// the rvalue-rhs overload and the by-value form must not tie.
constexpr auto opMatrixResolves() -> bool {
  Dual<double> a{1.0, 1.0}, b{2.0, 1.0};
  auto mk = [] { return Dual<double>{3.0, 1.0}; };
  (void)(a + b);
  (void)(a + mk());
  (void)(mk() + b);
  (void)(mk() + mk());
  (void)(a - b);
  (void)(a - mk());
  (void)(mk() - b);
  (void)(mk() - mk());
  (void)(a * b);
  (void)(a * mk());
  (void)(mk() * b);
  (void)(mk() * mk());
  (void)(a / b);
  (void)(a / mk());
  (void)(mk() / b);
  (void)(mk() / mk());
  return true;
}
static_assert(opMatrixResolves());

// The rvalue-donating − and ÷ must match the by-value path bit for bit, even
// with nontrivial gradients on both operands.
constexpr auto subRvalEqualsLval() -> bool {
  Dual<double> a{3.0, 2.0}, b{5.0, 7.0};
  auto lval = a - b;
  auto rval = a - Dual<double>{5.0, 7.0};
  return rval.value == lval.value && rval.gradient == lval.gradient;
}
constexpr auto divRvalEqualsLval() -> bool {
  Dual<double> a{3.0, 2.0}, b{5.0, 7.0};
  auto lval = a / b;
  auto rval = a / Dual<double>{5.0, 7.0};
  return rval.value == lval.value && rval.gradient == lval.gradient;
}
static_assert(subRvalEqualsLval());
static_assert(divRvalEqualsLval());

// Self-aliasing on the reversed kernels: x − move(x) → {0,0}, x / move(x) → {1,0}.
// Overload resolution binds both a and b to x, so the reversed order must read
// every old value before it clobbers b.
constexpr auto selfSubMove() -> Dual<double> {
  Dual<double> x{3.0, 1.0};
  return x - std::move(x);
}
constexpr auto selfDivMove() -> Dual<double> {
  Dual<double> x{3.0, 1.0};
  return x / std::move(x);
}
static_assert(selfSubMove().value == 0.0 && selfSubMove().gradient == 0.0);
static_assert(selfDivMove().value == 1.0 && selfDivMove().gradient == 0.0);

// Self-aliasing: x *= x and x /= x read all old state before any write.
constexpr auto selfMul() -> Dual<double> {
  Dual<double> x{3.0, 1.0};
  x *= x;
  return x;
}
constexpr auto selfDiv() -> Dual<double> {
  Dual<double> x{3.0, 1.0};
  x /= x;
  return x;
}
static_assert(selfMul().value == 9.0 && selfMul().gradient == 6.0);      // (x²)′ = 2x = 6
static_assert(selfDiv().value == 1.0 && selfDiv().gradient == 0.0);      // (x/x)′ = 0

// Overflow: forming w² = (1e200)² = inf would zero the gradient; the folded
// /w/w keeps it exact. u/w with u={1,1}, w={1e200,0} ⇒ gradient = u′/w = 1/1e200.
constexpr auto quotientOverflow() -> double {
  return (Dual<double>{1.0, 1.0} / Dual<double>{1e200, 0.0}).gradient;
}
static_assert(quotientOverflow() == 1.0 / 1e200);
static_assert(isFinite(quotientOverflow()));
// scalar ÷ dual: value = 2/1e200 exact; gradient −2/x² underflows to a finite 0.
constexpr auto scalarQuotientOverflow() -> Dual<double> {
  return 2.0 / Dual<double>{1e200, 1.0};
}
static_assert(scalarQuotientOverflow().value == 2.0 / 1e200);
static_assert(isFinite(scalarQuotientOverflow().gradient));

// Mixed scalar comparison: value-only verdicts, both operand orders, int scalar,
// and a nested Dual<Dual<double>> against a plain double — all via ADL rewriting.
constexpr auto mixedComparisons() -> bool {
  Dual<double> x{2.0, 5.0};
  bool ok = (x < 3.0) && (3.0 > x) && (x >= 2.0) && (x == 2.0) && (2.0 == x);
  ok = ok && !(x < 0.5) && (x >= 2) && (3 > x) && (x != 5.0);
  using DD = Dual<Dual<double>>;
  DD z{Dual<double>{1.0, 1.0}, Dual<double>{0.0, 0.0}};
  return ok && (z < 2.0) && (z == 1.0) && !(z > 5.0);
}
static_assert(mixedComparisons());

// Nested-dual scalar absorption: Dual<Dual<double>> + 2.0 casts 2.0 through
// parenthesized-aggregate init to a zero-tangent constant, so no tangent is
// added at either order — nothing but P0960 guards this.
constexpr auto nestedAbsorb() -> Dual<Dual<double>> {
  Dual<Dual<double>> x{Dual<double>{1.0, 1.0}, Dual<double>{1.0, 0.0}};
  return x + 2.0;
}
static_assert(nestedAbsorb().value.value == 3.0);        // value shifted by 2
static_assert(nestedAbsorb().value.gradient == 1.0);     // inner tangent untouched
static_assert(nestedAbsorb().gradient.value == 1.0);     // outer tangent untouched
static_assert(nestedAbsorb().gradient.gradient == 0.0);  // second order untouched

// Mixed-level nesting: a Dual<double> is a scalar of Dual<Dual<double>>,
// constructing into the outer T=Dual<double> by copy with tangent intact. Scaling
// by it runs T's own dual multiplication, so mixed second-order terms appear.
// Numbers from dd = {{2,3},{5,7}}, c = {11,13}.
constexpr auto nestDd() -> Dual<Dual<double>> {
  return {Dual<double>{2.0, 3.0}, Dual<double>{5.0, 7.0}};
}
constexpr auto nestC() -> Dual<double> { return {11.0, 13.0}; }

// dd + c: inner tangents ADD (c's 13 lands in value.gradient); outer gradient untouched.
constexpr auto nestAdd() -> Dual<Dual<double>> { return nestDd() + nestC(); }
static_assert(nestAdd().value.value == 13.0);       // 2 + 11
static_assert(nestAdd().value.gradient == 16.0);    // 3 + 13
static_assert(nestAdd().gradient.value == 5.0);     // outer untouched
static_assert(nestAdd().gradient.gradient == 7.0);
// dd · c: value *= c and gradient *= c, each a Dual<double> product (product rule):
//   value    = {2,3}·{11,13} = {2·11, 3·11 + 2·13} = {22, 59}
//   gradient = {5,7}·{11,13} = {5·11, 7·11 + 5·13} = {55, 142}
constexpr auto nestMul() -> Dual<Dual<double>> { return nestDd() * nestC(); }
static_assert(nestMul().value.value == 22.0);
static_assert(nestMul().value.gradient == 59.0);
static_assert(nestMul().gradient.value == 55.0);
static_assert(nestMul().gradient.gradient == 142.0);
// Reversed forms sink the dual by value and stay exact.
//   c − dd: value = c − dd.value = {9,10};  gradient = −dd.gradient = {−5,−7}
constexpr auto nestRevSub() -> Dual<Dual<double>> { return nestC() - nestDd(); }
static_assert(nestRevSub().value.value == 9.0 && nestRevSub().value.gradient == 10.0);
static_assert(nestRevSub().gradient.value == -5.0 && nestRevSub().gradient.gradient == -7.0);
//   c ÷ dd: value = c/dd.value = {5.5,−1.75};  gradient = −(c/w²)·dd.gradient = {−13.75, 5.75}
constexpr auto nestRevDiv() -> Dual<Dual<double>> { return nestC() / nestDd(); }
static_assert(nestRevDiv().value.value == 5.5 && nestRevDiv().value.gradient == -1.75);
static_assert(nestRevDiv().gradient.value == -13.75 && nestRevDiv().gradient.gradient == 5.75);

// Mixed-level comparison compares inner values only — the tangent never steers a branch.
constexpr auto nestCompare() -> bool {
  auto dd = nestDd();  // inner value 2
  auto c = nestC();    // inner value 11
  return (dd < c) && (c > dd) && !(dd < 0.5) && !(dd == c) &&
         (dd == Dual<double>{2.0, 99.0});  // 2 == 2 ignores the tangent 99
}
static_assert(nestCompare());

// The scalar-mixed forms admit exactly what constructs into T. Dual<double> +
// std::string satisfies no operator; a lossy cross-type mix (Dual<float> vs
// Dual<double>, neither T constructing the other) also fails loud, so a derivative
// is never silently dropped. The inner dual IS a scalar and mixes cleanly.
// Concepts wrap each check so an ill-formed body subsumes instead of hard-erroring.
template <typename A, typename B>
concept Addable = requires(A a, B b) { a + b; };
template <typename A, typename B>
concept ThreeWayComparable = requires(A a, B b) { a <=> b; };
static_assert(Addable<Dual<double>, double>);
static_assert(Addable<Dual<double>, int>);
static_assert(!Addable<Dual<double>, std::string>);
static_assert(!Addable<Dual<float>, Dual<double>>);
static_assert(!ThreeWayComparable<Dual<float>, Dual<double>>);
static_assert(Addable<Dual<Dual<double>>, Dual<double>>);
static_assert(ThreeWayComparable<Dual<Dual<double>>, Dual<double>>);

// No converting constructor, ever: a scalar never implicitly becomes a Dual;
// generic literal init spells T a = {3}.
static_assert(!std::is_convertible_v<double, Dual<double>>);
static_assert(!std::is_convertible_v<int, Dual<double>>);
static_assert(!std::is_convertible_v<Dual<double>, Dual<Dual<double>>>);
constexpr auto genericBraceInit() -> bool {
  auto check = []<typename T>() {
    T a = {3};
    return a == T{3};
  };
  return check.template operator()<double>() && check.template operator()<Dual<double>>();
}
static_assert(genericBraceInit());

auto main() -> int {
  "value-only comparison ignores the tangent"_test = [] {
    Dual<double> a{2.0, 1.0};
    Dual<double> b{2.0, 99.0};  // same value, different derivative
    expectTrue(a == b);
    expectFalse(a < b);
    expectTrue(Dual<double>{1.0, 5.0} < Dual<double>{2.0, 0.0});
  };

  "product and quotient rules"_test = [] {
    Dual<double> x{3.0, 1.0};
    auto p = x * x;  // x² → 9, 2x = 6
    expectClose((p.value), 9.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((p.gradient), 6.0, {.rtol = 1e-12, .atol = 1e-12});
    auto q = Dual<double>{1.0, 0.0} / x;  // 1/x → d = −1/x² = −1/9
    expectClose((q.value), 1.0 / 3.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((q.gradient), -1.0 / 9.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "scalar * Dual keeps the dual's value type and does not truncate"_test = [] {
    auto y = 2 * Dual<double>{2.5, 1.0};  // int scalar keeps Dual<double>, not Dual<int>→4.0
    static_assert(std::same_as<decltype(y), Dual<double>>);
    expectClose((y.value), 5.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((y.gradient), 2.0, {.rtol = 1e-12, .atol = 1e-12});
    // all four scalar-mixed ops carry the constant (zero-tangent) correctly
    Dual<double> x{4.0, 1.0};
    expectClose(((x + 10).value), 14.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose(((10 - x).gradient), -1.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose(((x / 2).gradient), 0.5, {.rtol = 1e-12, .atol = 1e-12});
    expectClose(((8 / x).gradient), -0.5, {.rtol = 1e-12, .atol = 1e-12});  // −8/x² = −0.5
  };

  "elementary function derivatives (chain rule)"_test = [] {
    Dual<double> x{0.7, 1.0};
    expectClose((sin(x).gradient), std::cos(0.7), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((cos(x).gradient), -std::sin(0.7), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((exp(x).gradient), std::exp(0.7), {.rtol = 1e-12, .atol = 1e-12});
    expectClose((log(x).gradient), 1.0 / 0.7, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((sqrt(x).gradient), 1.0 / (2 * std::sqrt(0.7)), {.rtol = 1e-12, .atol = 1e-12});
    // composition sin(x²): dy/dx = cos(x²)·2x
    auto y = sin(x * x);
    expectClose((y.gradient), std::cos(0.49) * 1.4, {.rtol = 1e-12, .atol = 1e-12});
    // pow with constant exponent: d/dx x³ = 3x²
    expectClose((pow(x, 3.0).gradient), 3 * 0.49, {.rtol = 1e-12, .atol = 1e-12});
  };

  "nested Dual<Dual> gives exact second derivatives"_test = [] {
    // x = a + ε₁ + ε₂ (ε₁ε₂ coeff 0); after f, the ε₁ε₂ coefficient is f''(a).
    using DD = Dual<Dual<double>>;
    DD x{Dual<double>{1.0, 1.0}, Dual<double>{1.0, 0.0}};
    auto r = x * x * x;  // f = x³ at a=1 → f=1, f'=3, f''=6
    expectClose((r.value.value), 1.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((r.value.gradient), 3.0, {.rtol = 1e-12, .atol = 1e-12});  // f'
    expectClose((r.gradient.gradient), 6.0, {.rtol = 1e-12, .atol = 1e-12});  // f''
  };

  "mixed scalar comparison drives branches, both orders and int scalars"_test = [] {
    Dual<double> x{2.0, 5.0};
    expectTrue(x < 3.0);
    expectTrue(3.0 > x);
    expectTrue(x >= 2);   // int scalar
    expectTrue(3 > x);    // int scalar
    expectTrue(x == 2.0);
    expectFalse(x < 0.5);
  };

  "pow with a bare int-literal exponent differentiates"_test = [] {
    Dual<double> x{0.7, 1.0};
    expectClose((pow(x, 2).value), 0.49, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((pow(x, 2).gradient), 1.4, {.rtol = 1e-12, .atol = 1e-12});  // 2x
    expectClose((pow(x, 3.0).gradient), 3 * 0.49, {.rtol = 1e-12, .atol = 1e-12});
  };

  "fma value and derivative, dual and constant addend"_test = [] {
    Dual<double> a{2.0, 1.0}, b{3.0, 1.0}, c{4.0, 1.0};
    auto r = fma(a, b, c);  // a·b + c = 10; d = a'b + ab' + c' = 3 + 2 + 1 = 6
    expectClose((r.value), 10.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((r.gradient), 6.0, {.rtol = 1e-12, .atol = 1e-12});
    auto r2 = fma(a, b, 5.0);  // 2·3 + 5 = 11; d = a'b + ab' = 3 + 2 = 5
    expectClose((r2.value), 11.0, {.rtol = 1e-12, .atol = 1e-12});
    expectClose((r2.gradient), 5.0, {.rtol = 1e-12, .atol = 1e-12});
  };

  "quotient stays finite when w² would overflow"_test = [] {
    auto q = Dual<double>{1.0, 1.0} / Dual<double>{1e200, 0.0};  // gradient = 1/1e200
    expectTrue(isFinite(q.gradient));
    expectClose((q.gradient), 1.0 / 1e200, {.rtol = 1e-12, .atol = 0.0});
    auto s = 2.0 / Dual<double>{1e200, 1.0};  // −2/x² underflows to a finite 0
    expectTrue(isFinite(s.gradient));
    expectClose((s.value), 2.0 / 1e200, {.rtol = 1e-12, .atol = 0.0});
  };

  return TestRegistry::result();
}
