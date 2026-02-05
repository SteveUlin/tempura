#include <experimental/meta>

#include "symbolic4/constants.h"
#include "symbolic4/core.h"
#include "symbolic4/indexed/gather.h"
#include "symbolic4/indexed/reduce_over.h"
#include "symbolic4/operators.h"
#include "symbolic4/strategy/diff.h"
#include "symbolic4/strategy/info_diff.h"
#include "unit.h"

using namespace tempura;
using namespace tempura::symbolic4;

// Namespace-scope tags for stable reflection.
struct DX {};
struct DY {};
struct DZ {};
struct DDim {};

// =========================================================================
// Key discovery: clang-p2996 alias opacity is TOTAL.
//
// Neither alias templates (template<T> using Symbol = Atom<T,Free>) nor
// plain type aliases (using X = Atom<DX, Free>) are transparent to ^^.
// The reflection captures the *name*, not the underlying type.
//
// Template parameters DO resolve aliases: when ^^E appears inside
// template<typename E>, the compiler substitutes the actual type before
// reflecting. This is why diff() works — ^^E and ^^Var go through
// template parameters.
//
// Workaround: R<T>() forces alias resolution through a template parameter
// boundary. Use R<T>() wherever ^^T would reflect an alias.
// =========================================================================

// Force alias resolution by reflecting through a template parameter.
template <typename T>
consteval auto R() -> std::meta::info { return ^^T; }

// Helper: differentiate in info domain and compare to expected type.
consteval auto infoDiffEq(std::meta::info expr, std::meta::info var,
                          std::meta::info expected) -> bool {
  return diffInfo(expr, var) == expected;
}

// Helper: check that info-domain diff produces the same result as type-domain diff.
// Since diff() now calls diffInfo internally, this validates the splice roundtrip:
//   diffInfo(^^E, ^^V) == ^^[:diffInfo(^^E, ^^V):]
template <typename E, typename V>
consteval auto crossValidates() -> bool {
  constexpr auto info_result = diffInfo(^^E, ^^V);
  using TypeResult = decltype(diff(E{}, V{}));
  // ^^TypeResult is opaque (alias). R<> forces resolution.
  return info_result == R<TypeResult>();
}

// Shorthand: the symbol types as the compiler sees them (post-alias-resolution)
using X = Atom<DX, Free>;
using Y = Atom<DY, Free>;
using Z = Atom<DZ, Free>;

auto main() -> int {
  // =========================================================================
  // TERMINALS
  // =========================================================================

  "infoDiff: d/dx[x] = 1"_test = [] {
    static_assert(infoDiffEq(R<X>(), R<X>(), ^^Constant<1.0>));
  };

  "infoDiff: d/dx[y] = 0"_test = [] {
    static_assert(infoDiffEq(R<Y>(), R<X>(), ^^Constant<0.0>));
  };

  "infoDiff: d/dx[constant] = 0"_test = [] {
    static_assert(infoDiffEq(^^Constant<42.0>, R<X>(), ^^Constant<0.0>));
    static_assert(infoDiffEq(^^Constant<0.0>, R<X>(), ^^Constant<0.0>));
  };

  "infoDiff: d/dx[pi] = 0"_test = [] {
    static_assert(infoDiffEq(^^Expression<PiOp>, R<X>(), ^^Constant<0.0>));
  };

  "infoDiff: d/dx[e] = 0"_test = [] {
    static_assert(infoDiffEq(^^Expression<EOp>, R<X>(), ^^Constant<0.0>));
  };

  "infoDiff: d/dx[fraction] = 0"_test = [] {
    static_assert(infoDiffEq(^^Fraction<1, 2>, R<X>(), ^^Constant<0.0>));
    static_assert(infoDiffEq(^^Fraction<3, 7>, R<X>(), ^^Constant<0.0>));
  };

  // =========================================================================
  // LINEAR OPERATIONS
  // =========================================================================

  "infoDiff: d/dx[x + y] = 1"_test = [] {
    // d/dx[x + y] = 1 + 0 → simplified to 1
    static_assert(infoDiffEq(
        ^^Expression<AddOp, X, Y>,
        R<X>(),
        ^^Constant<1.0>));
  };

  "infoDiff: d/dx[x - y] = 1"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<SubOp, X, Y>,
        R<X>(),
        ^^Constant<1.0>));
  };

  "infoDiff: d/dx[-x] = -1"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<NegOp, X>,
        R<X>(),
        ^^Expression<NegOp, Constant<1.0>>));
  };

  // =========================================================================
  // PRODUCT RULE
  // =========================================================================

  "infoDiff: d/dx[x * y] = y"_test = [] {
    // d/dx[x*y] = 1*y + x*0 → y + 0 → y
    static_assert(infoDiffEq(
        ^^Expression<MulOp, X, Y>,
        R<X>(),
        R<Y>()));
  };

  "infoDiff: d/dx[x * x] = x + x"_test = [] {
    // d/dx[x*x] = 1*x + x*1 → x + x
    static_assert(infoDiffEq(
        ^^Expression<MulOp, X, X>,
        R<X>(),
        ^^Expression<AddOp, X, X>));
  };

  // =========================================================================
  // QUOTIENT RULE
  // =========================================================================

  "infoDiff: d/dx[x/y] has DivOp at root"_test = [] {
    constexpr auto result = diffInfo(
        ^^Expression<DivOp, X, Y>,
        R<X>());
    // Should be y / (y*y) which simplifies to 1/y... or a div structure
    static_assert(std::meta::has_template_arguments(result));
    static_assert(std::meta::template_of(result) == ^^Expression);
    // Verify it's a division
    static_assert(std::meta::template_arguments_of(result)[0] == ^^::tempura::DivOp);
  };

  // =========================================================================
  // TRANSCENDENTAL FUNCTIONS
  // =========================================================================

  "infoDiff: d/dx[exp(x)] = exp(x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<ExpOp, X>,
        R<X>(),
        ^^Expression<ExpOp, X>));
  };

  "infoDiff: d/dx[log(x)] = 1/x"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<LogOp, X>,
        R<X>(),
        ^^Expression<DivOp, Constant<1.0>, X>));
  };

  "infoDiff: d/dx[sqrt(x)] = 1 / (2*sqrt(x))"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<SqrtOp, X>,
        R<X>(),
        ^^Expression<DivOp, Constant<1.0>,
            Expression<MulOp, Constant<2.0>, Expression<SqrtOp, X>>>));
  };

  "infoDiff: d/dx[sin(x)] = cos(x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<SinOp, X>,
        R<X>(),
        ^^Expression<CosOp, X>));
  };

  "infoDiff: d/dx[cos(x)] = -sin(x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<CosOp, X>,
        R<X>(),
        ^^Expression<NegOp, Expression<SinOp, X>>));
  };

  "infoDiff: d/dx[sinh(x)] = cosh(x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<SinhOp, X>,
        R<X>(),
        ^^Expression<CoshOp, X>));
  };

  "infoDiff: d/dx[cosh(x)] = sinh(x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<CoshOp, X>,
        R<X>(),
        ^^Expression<SinhOp, X>));
  };

  "infoDiff: d/dx[log1p(x)] = 1/(1+x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<Log1pOp, X>,
        R<X>(),
        ^^Expression<DivOp, Constant<1.0>,
            Expression<AddOp, Constant<1.0>, X>>));
  };

  // =========================================================================
  // CHAIN RULE
  // =========================================================================

  "infoDiff: d/dx[exp(2x)] = exp(2x) * 2"_test = [] {
    // exp(2x) where 2x = Expression<MulOp, Constant<2.0>, X>
    using TwoX = Expression<MulOp, Constant<2.0>, X>;
    static_assert(infoDiffEq(
        ^^Expression<ExpOp, TwoX>,
        R<X>(),
        ^^Expression<MulOp, Expression<ExpOp, TwoX>, Constant<2.0>>));
  };

  "infoDiff: d/dx[sin(x²)] has chain rule structure"_test = [] {
    // sin(x²): d/dx = cos(x²) · 2x = cos(x²) · (x + x)
    using X2 = Expression<MulOp, X, X>;
    constexpr auto result = diffInfo(^^Expression<SinOp, X2>, R<X>());
    // Result should be MulOp at root (cos(x²) * (x+x))
    static_assert(std::meta::template_of(result) == ^^Expression);
    static_assert(std::meta::template_arguments_of(result)[0] == ^^::tempura::MulOp);
  };

  // =========================================================================
  // SPECIAL FUNCTIONS
  // =========================================================================

  "infoDiff: d/dx[lgamma(x)] = digamma(x)"_test = [] {
    static_assert(infoDiffEq(
        ^^Expression<LgammaOp, X>,
        R<X>(),
        ^^Expression<DigammaOp, X>));
  };

  "infoDiff: d/dx[erf(x)] has correct structure"_test = [] {
    constexpr auto result = diffInfo(
        ^^Expression<ErfOp, X>,
        R<X>());
    // Should contain MulOp at root and ExpOp inside
    static_assert(std::meta::template_of(result) == ^^Expression);
  };

  // =========================================================================
  // ReduceOver
  // =========================================================================

  "infoDiff: d/dx[Σ(x)] = Σ(1) via SumReduce linearity"_test = [] {
    // ReduceOver<SumReduce, DDim, X> — use R<> to resolve the alias
    using Sum = ReduceOver<SumReduce, DDim, X>;
    constexpr auto result = diffInfo(R<Sum>(), R<X>());
    // Result should be ReduceOver<SumReduce, DDim, Constant<1.0>>
    static_assert(result ==
        ^^ReduceOver<SumReduce, DDim, Constant<1.0>>);
  };

  "infoDiff: d/dx[Σ(x*y)] = Σ(y) via SumReduce linearity"_test = [] {
    using Body = Expression<MulOp, X, Y>;
    using Sum = ReduceOver<SumReduce, DDim, Body>;
    constexpr auto result = diffInfo(R<Sum>(), R<X>());
    // d/dx[x*y] = y, so result = Σ(y)
    static_assert(result == ^^ReduceOver<SumReduce, DDim, Y>);
  };

  "infoDiff: d/dx[Π(f)] uses log-derivative trick"_test = [] {
    using Prod = ReduceOver<ProdReduce, DDim, X>;
    constexpr auto result = diffInfo(R<Prod>(), R<X>());
    // Should produce Prod * Σ(1/x)
    static_assert(std::meta::has_template_arguments(result));
    // Root should be MulOp (prod * sum_ratio)
    static_assert(std::meta::template_of(result) == ^^Expression);
  };

  // =========================================================================
  // Gather
  // =========================================================================

  "infoDiff: d/dx[gather(x, idx)] = gather(1, idx)"_test = [] {
    using Idx = Z;  // index is some other symbol
    constexpr auto result = diffInfo(
        ^^Gather<X, Idx>,
        R<X>());
    // d/dx[gather(x, idx)] = gather(d/dx[x], idx) = gather(1, idx)
    static_assert(result == ^^Gather<Constant<1.0>, Idx>);
  };

  "infoDiff: d/dx[gather(y, idx)] = gather(0, idx)"_test = [] {
    using Idx = Z;
    constexpr auto result = diffInfo(
        ^^Gather<Y, Idx>,
        R<X>());
    static_assert(result == ^^Gather<Constant<0.0>, Idx>);
  };

  // =========================================================================
  // Sample atoms (RandomVar atoms matched by Id)
  // =========================================================================

  "infoDiff: Sample atom with same Id → 1"_test = [] {
    // Atom<DX, Sample<int>> — same Id as X
    // Using int as a placeholder dist type (doesn't matter for diff)
    static_assert(infoDiffEq(
        ^^Atom<DX, Sample<int>>,
        R<X>(),
        ^^Constant<1.0>));
  };

  "infoDiff: Sample atom with different Id → 0"_test = [] {
    static_assert(infoDiffEq(
        ^^Atom<DY, Sample<int>>,
        R<X>(),
        ^^Constant<0.0>));
  };

  // =========================================================================
  // CROSS-VALIDATION: info-domain == type-domain
  // =========================================================================
  //
  // crossValidates<E, V> passes E and V through template parameters, so
  // aliases are resolved automatically. No R<>() needed here.

  "cross-validate: d/dx[x] via both paths"_test = [] {
    static_assert(crossValidates<X, X>());
  };

  "cross-validate: d/dx[y] via both paths"_test = [] {
    static_assert(crossValidates<Y, X>());
  };

  "cross-validate: d/dx[x+y] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<AddOp, X, Y>,
        X>());
  };

  "cross-validate: d/dx[x*y] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<MulOp, X, Y>,
        X>());
  };

  "cross-validate: d/dx[exp(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<ExpOp, X>,
        X>());
  };

  "cross-validate: d/dx[log(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<LogOp, X>,
        X>());
  };

  "cross-validate: d/dx[sin(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<SinOp, X>,
        X>());
  };

  "cross-validate: d/dx[cos(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<CosOp, X>,
        X>());
  };

  "cross-validate: d/dx[sqrt(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<SqrtOp, X>,
        X>());
  };

  "cross-validate: d/dx[sinh(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<SinhOp, X>,
        X>());
  };

  "cross-validate: d/dx[lgamma(x)] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<LgammaOp, X>,
        X>());
  };

  "cross-validate: d/dx[exp(2x)] chain rule"_test = [] {
    using TwoX = Expression<MulOp, Constant<2.0>, X>;
    static_assert(crossValidates<
        Expression<ExpOp, TwoX>,
        X>());
  };

  "cross-validate: d/dx[x*x] via both paths"_test = [] {
    static_assert(crossValidates<
        Expression<MulOp, X, X>,
        X>());
  };

  // =========================================================================
  // containsLiteralInfo
  // =========================================================================

  return TestRegistry::result();
}
